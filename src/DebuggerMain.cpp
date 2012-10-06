/*
Copyright (C) 2006 - 2011 Evan Teran
                          eteran@alum.rit.edu

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "DebuggerMain.h"
#include "CommentServer.h"
#include "Configuration.h"
#include "Debugger.h"
#include "DebuggerInternal.h"
#include "DebuggerOps.h"
#include "DialogArguments.h"
#include "DialogAttach.h"
#include "DialogMemoryRegions.h"
#include "DialogPlugins.h"
#include "DialogThreads.h"
#include "Expression.h"
#include "IAnalyzer.h"
#include "IArchProcessor.h"
#include "IBinary.h"
#include "IDebuggerCore.h"
#include "IPlugin.h"
#include "ISessionFile.h"
#include "Instruction.h"
#include "MemoryRegions.h"
#include "QHexView"
#include "RecentFileManager.h"
#include "State.h"
#include "SymbolManager.h"
#include "version.h"

#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QShortcut>
#include <QStringListModel>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVector>
#include <QtDebug>

#include <boost/bind.hpp>
#include <memory>

#include "ui_debuggerui.h"

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
#include <link.h>
#endif

#if defined(Q_OS_UNIX)
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(YAD64_X86)
	static const char instruction_pointer_name[] = "EIP";
	static const char stack_pointer_name[]       = "ESP";
	static const char frame_pointer_name[]       = "EBP";
	static const char stack_type_name[]          = "DWORD";
#elif defined(YAD64_X86_64)
	static const char instruction_pointer_name[] = "RIP";
	static const char stack_pointer_name[]       = "RSP";
	static const char frame_pointer_name[]       = "RBP";
	static const char stack_type_name[]          = "QWORD";
#endif

namespace {
	//------------------------------------------------------------------------------
	// Name: is_trap(const DebugEvent &event)
	// Desc:
	//------------------------------------------------------------------------------
	bool is_trap(const DebugEvent &event) {
		return event.stopped() && event.stop_code() == DebugEvent::sigtrap;
	}

	//--------------------------------------------------------------------------
	// Name: is_instruction_ret()
	//--------------------------------------------------------------------------
	bool is_instruction_ret(yad64::address_t address) {

		quint8 buffer[yad64::Instruction::MAX_SIZE];
		int size = sizeof(buffer);

		if(yad64::v1::get_instruction_bytes(address, buffer, size)) {
			yad64::Instruction insn(buffer, buffer + size, address, std::nothrow);
			return insn.valid() && insn.type() == yad64::Instruction::OP_RET;
		}
		return false;
	}
}

class RunUntilRet : public IDebugEventHandler {
public:
	//--------------------------------------------------------------------------
	// Name: RunUntilRet(DebuggerMain *ui)
	//--------------------------------------------------------------------------
	RunUntilRet(DebuggerMain *ui) : ui_(ui), previous_handler_(0), last_call_return_(0) {
		previous_handler_ = yad64::v1::set_debug_event_handler(this);
	}

	//--------------------------------------------------------------------------
	// Name: handle_event(const DebugEvent &event)
	//--------------------------------------------------------------------------
	virtual yad64::EVENT_STATUS handle_event(const DebugEvent &event) {

		if(event.trap_reason() == DebugEvent::TRAP_STEPPING) {

			State state;
			yad64::v1::debugger_core->get_state(state);
			const yad64::address_t address = state.instruction_pointer();

			if(last_call_return_ == address) {
				last_call_return_ = 0;
			}


			if(!is_instruction_ret(address)) {

				// we haven't seen a top-level call yet, so this one is noteworthy,
				// record where we think it will return to (assuming normal call
				// semantics).
				if(last_call_return_ == 0) {
					quint8 buffer[yad64::Instruction::MAX_SIZE];
					int sz = sizeof(buffer);

					// if this some variant of a call, then we should
					// record where we think it'll return to
					if(yad64::v1::get_instruction_bytes(address, buffer, sz)) {
						yad64::Instruction insn(buffer, buffer + sz, 0, std::nothrow);
						if(insn.valid() && yad64::v1::arch_processor().can_step_over(insn)) {
							last_call_return_ = address + insn.size();
						}
					}
				}

				return yad64::DEBUG_CONTINUE_STEP;
			} else {
				// if we are on the top level, then we are done because this is a RET
				if(last_call_return_ == 0) {
					const yad64::EVENT_STATUS status = previous_handler_->handle_event(event);
					yad64::v1::set_debug_event_handler(previous_handler_);
					delete this;
					return status;
				} else {
					return yad64::DEBUG_CONTINUE_STEP;
				}
			}

		} else {
			const yad64::EVENT_STATUS status = previous_handler_->handle_event(event);

			if(status == yad64::DEBUG_CONTINUE) {
				return yad64::DEBUG_CONTINUE_STEP;
			}

			yad64::v1::set_debug_event_handler(previous_handler_);
			delete this;
			return status;
		}
	}

private:
	DebuggerMain *const          ui_;
	IDebugEventHandler * previous_handler_;
	yad64::address_t               last_call_return_;
};



//------------------------------------------------------------------------------
// Name: DebuggerMain(QWidget *parent)
// Desc: constructor
//------------------------------------------------------------------------------
DebuggerMain::DebuggerMain(QWidget *parent) : DebuggerUI(parent),
		arguments_dialog_(new DialogArguments),
		timer_(new QTimer(this)),
		recent_file_manager_(new RecentFileManager(this)),
		stack_comment_server_(new CommentServer),
		stack_view_locked_(false),
		step_run_(false)
#ifdef Q_OS_UNIX
		,debug_pointer_(0)
#endif
	{

	setup_ui();

	// connect the timer to the debug event
	connect(timer_, SIGNAL(timeout()), this, SLOT(next_debug_event()));

	// create a context menu for the tab bar as well
	connect(ui->tabWidget, SIGNAL(customContextMenuRequested(int, const QPoint &)), this, SLOT(tab_context_menu(int, const QPoint &)));

	connect(new QShortcut(QKeySequence(tr("Ctrl+G")), this), SIGNAL(activated()), this, SLOT(goto_triggered()));

	setAcceptDrops(true);

	// setup the list model for instruction details list
	list_model_ = new QStringListModel(this);
	ui->listView->setModel(list_model_);

	// setup the recent file manager
	ui->action_Recent_Files->setMenu(recent_file_manager_->create_menu());
	connect(recent_file_manager_, SIGNAL(file_selected(const QString &)), SLOT(open_file(const QString &)));

	// make us the default event handler
	yad64::v1::set_debug_event_handler(this);

	// enable the arch processor
	yad64::v1::arch_processor().setup_register_view(ui->registerList);

	// default the working directory to ours
	working_directory_ = QDir().absolutePath();
}

//------------------------------------------------------------------------------
// Name: goto_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::goto_triggered() {
	QWidget *const widget = QApplication::focusWidget();
	if(QHexView *const hexview = qobject_cast<QHexView*>(widget)) {
		if(hexview == stack_view_) {
			mnuStackGotoAddress();
		} else {
			mnuDumpGotoAddress();
		}
	} else if(qobject_cast<QDisassemblyView*>(widget)) {
		mnuCPUJumpToAddress();
	}
}

//------------------------------------------------------------------------------
// Name: setup_ui()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::setup_ui() {
	// setup the global pointers as early as possible.
	// NOTE:  this should never be changed after this point
	// NOTE:  this is important that this happens BEFORE any components which
	// read settings as it could end up being a memory leak (and therefore never
	// calling it's destructor which writes the settings to disk!).
	yad64::v1::debugger_ui = this;

	ui->setupUi(this);

	// add toggles for the dock windows
	ui->menu_View->addAction(ui->registersDock ->toggleViewAction());
	ui->menu_View->addAction(ui->dataDock      ->toggleViewAction());
	ui->menu_View->addAction(ui->stackDock     ->toggleViewAction());
	ui->menu_View->addAction(ui->toolBar       ->toggleViewAction());

	// make sure our widgets use custom context menus
	ui->registerList->setContextMenuPolicy(Qt::CustomContextMenu);
	ui->cpuView->setContextMenuPolicy(Qt::CustomContextMenu);

	// set the listbox to about 4 lines
	const QFontMetrics &fm = ui->listView->fontMetrics();
	ui->listView->setFixedHeight(fm.height() * 4);

	setup_stack_view();
	setup_tab_buttons();

	// remove the one in the designer and put in our built one.
	// it's a bit ugly to do it this way, but the designer wont let me
	// make a tabless entry..and the ones i create look slightly diff
	// (probably lack of layout manager in mine...
	ui->tabWidget->clear();
	mnuDumpCreateTab();

	// apply any fonts we may have stored
	apply_default_fonts();

	// apply the default setting for showing address separators
	apply_default_show_separator();
}

//------------------------------------------------------------------------------
// Name: setup_stack_view()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::setup_stack_view() {

	stack_view_ = QSharedPointer<QHexView>(new QHexView);
	ui->stackDock->setWidget(stack_view_.data());

	// setup the context menu
	stack_view_->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(stack_view_.data(), SIGNAL(customContextMenuRequested(const QPoint &)), SLOT(mnuStackContextMenu(const QPoint &)));

	// we placed a view in the designer, so just set it here
	// this may get transitioned to heap allocated, we'll see
	stack_view_info_.view = stack_view_;

	// setup the comment server for the stack viewer
	stack_view_->setCommentServer(stack_comment_server_);
}

//------------------------------------------------------------------------------
// Name: ~DebuggerMain()
// Desc: destructor
//------------------------------------------------------------------------------
DebuggerMain::~DebuggerMain() {
	detach_from_process((yad64::v1::config().close_behavior == Configuration::Terminate) ? KILL_ON_DETACH : NO_KILL_ON_DETACH);
}

//------------------------------------------------------------------------------
// Name: closeEvent(QCloseEvent *)
// Desc: triggered on main window close, saves window state
//------------------------------------------------------------------------------
void DebuggerMain::closeEvent(QCloseEvent *event) {
	QSettings settings;
	const QByteArray state = saveState();
	settings.beginGroup("Window");
	settings.setValue("window.state", state);
	settings.setValue("window.width", width());
	settings.setValue("window.height", height());
	settings.setValue("window.stack.show_address.enabled", stack_view_->showAddress());
	settings.setValue("window.stack.show_hex.enabled", stack_view_->showHexDump());
	settings.setValue("window.stack.show_ascii.enabled", stack_view_->showAsciiDump());
	settings.setValue("window.stack.show_comments.enabled", stack_view_->showComments());
	settings.setValue("window.stack.row_width", stack_view_->rowWidth());
	settings.setValue("window.stack.word_width", stack_view_->wordWidth());
	settings.endGroup();
	event->accept();
}

//------------------------------------------------------------------------------
// Name: showEvent(QShowEvent *)
// Desc: triggered on show, restores window state
//------------------------------------------------------------------------------
void DebuggerMain::showEvent(QShowEvent *) {
	QSettings settings;
	settings.beginGroup("Window");
	const QByteArray state = settings.value("window.state").value<QByteArray>();
	const int width        = settings.value("window.width", -1).value<int>();
	const int height       = settings.value("window.height", -1).value<int>();

	if(width != -1) {
		resize(width, size().height());
	}

	if(height != -1) {
		resize(size().width(), height);
	}

	stack_view_->setShowAddress(settings.value("window.stack.show_address.enabled", true).value<bool>());
	stack_view_->setShowHexDump(settings.value("window.stack.show_hex.enabled", true).value<bool>());
	stack_view_->setShowAsciiDump(settings.value("window.stack.show_ascii.enabled", true).value<bool>());
	stack_view_->setShowComments(settings.value("window.stack.show_comments.enabled", true).value<bool>());

	int row_width = settings.value("window.stack.row_width", 1).value<int>();
	int word_width = settings.value("window.stack.word_width", yad64::v1::pointer_size()).value<int>();

	// normalize values
	if(word_width != 1 && word_width != 2 && word_width != 4 && word_width != 8) {
		word_width = yad64::v1::pointer_size();
	}

	if(row_width != 1 && row_width != 2 && row_width != 4 && row_width != 8 && row_width != 16) {
		row_width = 1;
	}

	stack_view_->setRowWidth(row_width);
	stack_view_->setWordWidth(word_width);

	settings.endGroup();
	restoreState(state);
}

//------------------------------------------------------------------------------
// Name: dragEnterEvent(QDragEnterEvent* event)
// Desc: triggered when dragging data onto the main window
//------------------------------------------------------------------------------
void DebuggerMain::dragEnterEvent(QDragEnterEvent* event) {
	const QMimeData* mimeData = event->mimeData();

	// check for our needed mime type (file)
	// make sure it's only one file
	if(mimeData->hasUrls() && mimeData->urls().size() == 1) {
		// extract the local path of the file
		QUrl url = mimeData->urls()[0].toLocalFile();
		if(!url.isEmpty()) {
			event->accept();
		}
	}
}

//------------------------------------------------------------------------------
// Name: dropEvent(QDropEvent* event)
// Desc: triggered when data was dropped onto the main window
//------------------------------------------------------------------------------
void DebuggerMain::dropEvent(QDropEvent* event) {
	const QMimeData* mimeData = event->mimeData();

	if(mimeData->hasUrls() && mimeData->urls().size() == 1) {
		const QString s = mimeData->urls()[0].toLocalFile();
		if(!s.isEmpty()) {
			Q_CHECK_PTR(yad64::v1::debugger_core);

			detach_from_process(KILL_ON_DETACH);
			common_open(s, QList<QByteArray>());
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_actionAbout_QT_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_actionAbout_QT_triggered() {
	QMessageBox::aboutQt(this, tr("About Qt"));
}

//------------------------------------------------------------------------------
// Name: apply_default_fonts()
// Desc: applies the configuration's fonts to all necessary widgets
//------------------------------------------------------------------------------
void DebuggerMain::apply_default_fonts() {

	QFont font;
	const Configuration &config = yad64::v1::config();

    //font.fromString(config.registers_font);
    //QApplication::setFont(font);

	// set some default fonts
	if(font.fromString(config.stack_font)) {
		stack_view_->setFont(font);
	}

	if(font.fromString(config.registers_font)) {
		ui->registerList->setFont(font);
	}

	if(font.fromString(config.disassembly_font)) {
		ui->cpuView->setFont(font);
	}

	if(font.fromString(config.data_font)) {
		Q_FOREACH(const DataViewInfo::pointer &data_view, data_regions_) {
			data_view->view->setFont(font);
		}
	}
}

//------------------------------------------------------------------------------
// Name: setup_tab_buttons()
// Desc: creates the add/remove tab buttons in the data view
//------------------------------------------------------------------------------
void DebuggerMain::setup_tab_buttons() {
	// add the corner widgets to the data view
	add_tab_ = new QToolButton(ui->tabWidget);
	del_tab_ = new QToolButton(ui->tabWidget);

	add_tab_->setToolButtonStyle(Qt::ToolButtonIconOnly);
	del_tab_->setToolButtonStyle(Qt::ToolButtonIconOnly);
	add_tab_->setIcon(QIcon(":/debugger/images/yad6416-addtab.png"));
	del_tab_->setIcon(QIcon(":/debugger/images/yad6416-deltab.png"));
	add_tab_->setAutoRaise(true);
	del_tab_->setAutoRaise(true);
	add_tab_->setEnabled(false);
	del_tab_->setEnabled(false);

	ui->tabWidget->setCornerWidget(add_tab_, Qt::TopLeftCorner);
	ui->tabWidget->setCornerWidget(del_tab_, Qt::TopRightCorner);

	connect(add_tab_, SIGNAL(clicked()), SLOT(mnuDumpCreateTab()));
	connect(del_tab_, SIGNAL(clicked()), SLOT(mnuDumpDeleteTab()));
}

//------------------------------------------------------------------------------
// Name: on_registerList_customContextMenuRequested(const QPoint &pos)
// Desc: context menu handler for register view
//------------------------------------------------------------------------------
void DebuggerMain::on_registerList_customContextMenuRequested(const QPoint &pos) {
	QTreeWidgetItem *const item = ui->registerList->itemAt(pos);
	if(item && !ui->registerList->isCategory(item)) {
		// a little bit cheesy of a solution, but should work nicely
		if(const Register reg = yad64::v1::arch_processor().value_from_item(*item)) {
			if(reg.type() & (Register::TYPE_GPR | Register::TYPE_IP | Register::TYPE_COND)) {
				QMenu menu;
				menu.addAction(tr("&Follow In Dump"), this,           SLOT(mnuRegisterFollowInDump()));
				menu.addAction(tr("&Follow In Dump (New Tab)"), this, SLOT(mnuRegisterFollowInDumpNewTab()));
				menu.addAction(tr("&Follow In Stack"), this,          SLOT(mnuRegisterFollowInStack()));

				add_plugin_context_menu(&menu, &IPlugin::register_context_menu);

				menu.exec(ui->registerList->mapToGlobal(pos));
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_cpuView_breakPointToggled(yad64::address_t address)
// Desc: handler for toggling the breakpoints
//------------------------------------------------------------------------------
void DebuggerMain::on_cpuView_breakPointToggled(yad64::address_t address) {
	yad64::v1::toggle_breakpoint(address);
}

//------------------------------------------------------------------------------
// Name: on_registerList_itemDoubleClicked(QTreeWidgetItem *item)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_registerList_itemDoubleClicked(QTreeWidgetItem *item) {
	Q_CHECK_PTR(item);

	if(const Register reg = yad64::v1::arch_processor().value_from_item(*item)) {
		yad64::reg_t r = *reg;
		if(yad64::v1::get_value_from_user(r, tr("Register Value"))) {

			State state;
			yad64::v1::debugger_core->get_state(state);
			state.set_register(reg.name(), r);
			yad64::v1::debugger_core->set_state(state);
			update_gui();
			refresh_gui();
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_action_About_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_About_triggered() {
	QMessageBox::about(this, tr("About yad64"),
		tr(
        "<p>yad64 - Yet another debugger for windows x64 executables</p>"
        "<p>Front-end based on <a href=\"http://codef00.com\">edb</a> by Evan Teran.</p>"
        "<p>Written by Nikolenko Konstantin and Bondarenko Roman</p>"
		"<p>version: %1</p>"
		).arg(yad64::version));
}

//------------------------------------------------------------------------------
// Name: apply_default_show_separator()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::apply_default_show_separator() {
	const bool show = yad64::v1::config().show_address_separator;

	ui->cpuView->setShowAddressSeparator(show);
	stack_view_->setShowAddressSeparator(show);
	Q_FOREACH(const DataViewInfo::pointer &data_view, data_regions_) {
		data_view->view->setShowAddressSeparator(show);
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Configure_Debugger_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Configure_Debugger_triggered() {

	yad64::v1::dialog_options()->exec();

	// reload symbols in case they changed, or our symbol files changes
	yad64::v1::reload_symbols();

	// re-read the memory region information
	yad64::v1::memory_regions().sync();

	// apply the selected fonts
	apply_default_fonts();

	// apply changes to the GUI options
	apply_default_show_separator();

	// show changes
	refresh_gui();
}

//------------------------------------------------------------------------------
// Name: follow_memory(yad64::address_t address, F follow_func)
// Desc:
//------------------------------------------------------------------------------
template <class F>
void DebuggerMain::follow_memory(yad64::address_t address, F follow_func) {
	if(!follow_func(address)) {
		QMessageBox::information(this,
			tr("No Memory Found"),
			tr("There appears to be no memory at that location (<strong>0x%1</strong>)").arg(yad64::v1::format_pointer(address)));
	}
}

//------------------------------------------------------------------------------
// Name: follow_register_in_dump(bool tabbed)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::follow_register_in_dump(bool tabbed) {
	bool ok;
	const yad64::address_t address = get_follow_register(ok);
	if(ok && !yad64::v1::dump_data(address, tabbed)) {
		QMessageBox::information(this,
			tr("No Memory Found"),
			tr("There appears to be no memory at that location (<strong>0x%1</strong>)").arg(yad64::v1::format_pointer(address)));
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackGotoESP()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuStackGotoESP() {
	State state;
	yad64::v1::debugger_core->get_state(state);
	follow_memory(state.stack_pointer(), boost::bind(yad64::v1::dump_stack, _1));
}

//------------------------------------------------------------------------------
// Name: mnuStackGotoEBP()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuStackGotoEBP() {
	State state;
	yad64::v1::debugger_core->get_state(state);
	follow_memory(state.frame_pointer(), boost::bind(yad64::v1::dump_stack, _1));
}

//------------------------------------------------------------------------------
// Name: mnuCPUJumpToEIP()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuCPUJumpToEIP() {
	State state;
	yad64::v1::debugger_core->get_state(state);
	follow_memory(state.instruction_pointer(), boost::bind(yad64::v1::jump_to_address, _1));
}

//------------------------------------------------------------------------------
// Name: mnuCPUJumpToAddress()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuCPUJumpToAddress() {
	bool ok;
	const yad64::address_t address = get_goto_expression(ok);
	if(ok) {
		follow_memory(address, boost::bind(yad64::v1::jump_to_address, _1));
	}
}

//------------------------------------------------------------------------------
// Name: mnuDumpGotoAddress()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuDumpGotoAddress() {
    bool ok;
	const yad64::address_t address = get_goto_expression(ok);
	if(ok) {
		follow_memory(address, boost::bind(yad64::v1::dump_data, _1));
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackGotoAddress()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuStackGotoAddress() {
    bool ok;
	const yad64::address_t address = get_goto_expression(ok);
	if(ok) {
		follow_memory(address, boost::bind(yad64::v1::dump_stack, _1));
	}
}

//------------------------------------------------------------------------------
// Name: mnuRegisterFollowInStack()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuRegisterFollowInStack() {
	bool ok;
	const yad64::address_t address = get_follow_register(ok);
	if(ok) {
		follow_memory(address, boost::bind(yad64::v1::dump_stack, _1));
	}
}


//------------------------------------------------------------------------------
// Name: get_follow_address(const T &hv, bool &ok)
// Desc:
//------------------------------------------------------------------------------
template <class T>
yad64::address_t DebuggerMain::get_follow_address(const T &hv, bool &ok) {
	ok = false;

	Q_CHECK_PTR(hv);

	if(hv->hasSelectedText()) {
		const QByteArray data = hv->selectedBytes();

		if(data.size() == yad64::v1::pointer_size()) {
			yad64::address_t d;
			std::memcpy(&d, data.data(), yad64::v1::pointer_size());

			ok = true;
			return d;
		}
	}

	QMessageBox::information(this,
		tr("Invalid Selection"),
		tr("Please select %1 bytes to use this function.").arg(yad64::v1::pointer_size()));

	return 0;
}

//------------------------------------------------------------------------------
// Name: follow_in_stack(const T &hv)
// Desc:
//------------------------------------------------------------------------------
template <class T>
void DebuggerMain::follow_in_stack(const T &hv) {
	bool ok;
	const yad64::address_t address = get_follow_address(hv, ok);
	if(ok) {
		follow_memory(address, boost::bind(yad64::v1::dump_stack, _1));
	}
}

//------------------------------------------------------------------------------
// Name: follow_in_dump(const T &hv)
// Desc:
//------------------------------------------------------------------------------
template <class T>
void DebuggerMain::follow_in_dump(const T &hv) {
	bool ok;
	const yad64::address_t address = get_follow_address(hv, ok);
	if(ok) {
		follow_memory(address, boost::bind(yad64::v1::dump_data, _1));
	}
}

//------------------------------------------------------------------------------
// Name: follow_in_cpu(const T &hv)
// Desc:
//------------------------------------------------------------------------------
template <class T>
void DebuggerMain::follow_in_cpu(const T &hv) {
	bool ok;
	const yad64::address_t address = get_follow_address(hv, ok);
	if(ok) {
		follow_memory(address, boost::bind(yad64::v1::jump_to_address, _1));
	}
}

//------------------------------------------------------------------------------
// Name: mnuDumpFollowInCPU()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuDumpFollowInCPU() {
	follow_in_cpu(qobject_cast<QHexView *>(ui->tabWidget->currentWidget()));
}

//------------------------------------------------------------------------------
// Name: mnuDumpFollowInDump()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuDumpFollowInDump() {
	follow_in_dump(qobject_cast<QHexView *>(ui->tabWidget->currentWidget()));
}

//------------------------------------------------------------------------------
// Name: mnuDumpFollowInStack()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuDumpFollowInStack() {
	follow_in_stack(qobject_cast<QHexView *>(ui->tabWidget->currentWidget()));
}

//------------------------------------------------------------------------------
// Name: mnuStackFollowInDump()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuStackFollowInDump() {
	follow_in_dump(stack_view_);
}

//------------------------------------------------------------------------------
// Name: mnuStackFollowInCPU()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuStackFollowInCPU() {
	follow_in_cpu(stack_view_);
}

//------------------------------------------------------------------------------
// Name: mnuStackFollowInStack()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuStackFollowInStack() {
	follow_in_stack(stack_view_);
}

//------------------------------------------------------------------------------
// Name: on_actionApplication_Arguments_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_actionApplication_Arguments_triggered() {
	arguments_dialog_->exec();
}

//------------------------------------------------------------------------------
// Name: on_actionApplication_Working_Directory_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_actionApplication_Working_Directory_triggered() {
	const QString new_dir = QFileDialog::getExistingDirectory(
		this,
		tr("Application Working Directory"),
		QString(),
		QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if(!new_dir.isEmpty()) {
		working_directory_ = new_dir;
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackPush()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuStackPush() {
	yad64::reg_t value = 0;
	State state;
	yad64::v1::debugger_core->get_state(state);

	// ask for a replacement
	if(yad64::v1::get_value_from_user(value, tr("Enter value to push"))) {

		// if they said ok, do the push, just like the hardware would do
		yad64::v1::push_value(state, value);

		// update the state
		yad64::v1::debugger_core->set_state(state);
		update_gui();
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackPop()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuStackPop() {
	State state;
	yad64::v1::debugger_core->get_state(state);
	yad64::v1::pop_value(state);
	yad64::v1::debugger_core->set_state(state);
	update_gui();
}

//------------------------------------------------------------------------------
// Name: on_cpuView_customContextMenuRequested(const QPoint &pos)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_cpuView_customContextMenuRequested(const QPoint &pos) {
	QMenu menu;
	// TODO: add comment
	//menu.addAction(tr("Add &Comment"), this, SLOT(""));
	//menu.addSeparator();
	menu.addAction(tr("&Goto Address"), this, SLOT(mnuCPUJumpToAddress()));
	menu.addAction(tr("&Goto %1").arg(instruction_pointer_name), this, SLOT(mnuCPUJumpToEIP()));

	const yad64::address_t address = ui->cpuView->selectedAddress();
	int size                     = ui->cpuView->selectedSize();


	if(yad64::v1::debugger_core->pid() != 0) {
		quint8 buffer[yad64::Instruction::MAX_SIZE + 1];
		if(yad64::v1::get_instruction_bytes(address, buffer, size)) {
			yad64::Instruction insn(buffer, buffer + size, address, std::nothrow);
			if(insn.valid()) {

				switch(insn.type()) {
				case yad64::Instruction::OP_JMP:
				case yad64::Instruction::OP_CALL:
				case yad64::Instruction::OP_JCC:
					if(insn.operand(0).general_type() == yad64::Operand::TYPE_REL) {
						QAction *const action = menu.addAction(tr("&Follow"), this, SLOT(mnuCPUFollow()));
						action->setData(static_cast<qlonglong>(insn.operand(0).relative_target()));
					}
					
					/*
					if(insn.operand(0).general_type() == yad64::Operand::TYPE_EXPRESSION) {
						if(insn.operand(0).expression().base == yad64::Operand::REG_RIP && insn.operand(0).expression().index == yad64::Operand::REG_NULL && insn.operand(0).expression().scale == 1) {
							QAction *const action = menu.addAction(tr("&Follow"), this, SLOT(mnuCPUFollow()));
							action->setData(static_cast<qlonglong>(address + insn.operand(0).displacement()));
						}
					}
					*/
					break;
				default:
					for(std::size_t i = 0; i < insn.operand_count(); ++i) {
						if(insn.operand(i).general_type() == yad64::Operand::TYPE_IMMEDIATE) {
							QAction *const action = menu.addAction(tr("Follow Constant In &Dump"), this, SLOT(mnuCPUFollowInDump()));
							action->setData(static_cast<qlonglong>(insn.operand(i).immediate()));

							QAction *const action2 = menu.addAction(tr("Follow Constant In &Stack"), this, SLOT(mnuCPUFollowInStack()));
							action2->setData(static_cast<qlonglong>(insn.operand(i).immediate()));
						}
					}
				}
			}
		}
	}

	menu.addSeparator();
	menu.addAction(tr("&Set %1 to this Instruction").arg(instruction_pointer_name), this, SLOT(mnuCPUSetEIP()));
	menu.addSeparator();
	menu.addAction(tr("&Edit Bytes"), this, SLOT(mnuCPUModify()));
	menu.addAction(tr("&Fill with 00's"), this, SLOT(mnuCPUFillZero()));
	menu.addAction(tr("Fill with &NOPs"), this, SLOT(mnuCPUFillNop()));
	menu.addSeparator();
	menu.addAction(tr("&Add Breakpoint"), this, SLOT(mnuCPUAddBreakpoint()));
	menu.addAction(tr("Add &Conditional Breakpoint"), this, SLOT(mnuCPUAddConditionalBreakpoint()));
	menu.addAction(tr("&Remove Breakpoint"), this, SLOT(mnuCPURemoveBreakpoint()));

	add_plugin_context_menu(&menu, &IPlugin::cpu_context_menu);

	menu.exec(ui->cpuView->viewport()->mapToGlobal(pos));
}

//------------------------------------------------------------------------------
// Name: mnuCPUFollow()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuCPUFollow() {
	if(QAction *const action = qobject_cast<QAction *>(sender())) {
		const yad64::address_t address = action->data().toULongLong();
		follow_memory(address, yad64::v1::jump_to_address);
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPUFollowInDump()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuCPUFollowInDump() {
	if(QAction *const action = qobject_cast<QAction *>(sender())) {
		const yad64::address_t address = action->data().toULongLong();
		follow_memory(address, boost::bind(yad64::v1::dump_data, _1));
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPUFollowInStack()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuCPUFollowInStack() {
	if(QAction *const action = qobject_cast<QAction *>(sender())) {
		const yad64::address_t address = action->data().toULongLong();
		follow_memory(address, boost::bind(yad64::v1::dump_stack, _1));
	}
}

//------------------------------------------------------------------------------
// Name: mnuStackToggleLock(bool locked)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuStackToggleLock(bool locked) {
	stack_view_locked_ = locked;
}

//------------------------------------------------------------------------------
// Name: mnuStackContextMenu(const QPoint &pos)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuStackContextMenu(const QPoint &pos) {

	QMenu *const menu = stack_view_->createStandardContextMenu();
	menu->addSeparator();
	menu->addAction(tr("Follow Address In &CPU"), this, SLOT(mnuStackFollowInCPU()));
	menu->addAction(tr("Follow Address In &Dump"), this, SLOT(mnuStackFollowInDump()));
	menu->addAction(tr("Follow Address In &Stack"), this, SLOT(mnuStackFollowInStack()));
	menu->addAction(tr("&Goto Address"), this, SLOT(mnuStackGotoAddress()));
	menu->addAction(tr("Goto %1").arg(stack_pointer_name), this, SLOT(mnuStackGotoESP()));
	menu->addAction(tr("Goto %1").arg(frame_pointer_name), this, SLOT(mnuStackGotoEBP()));

	menu->addSeparator();
	menu->addAction(tr("&Edit Bytes"), this, SLOT(mnuStackModify()));
	menu->addSeparator();
	menu->addAction(tr("&Push %1").arg(stack_type_name), this, SLOT(mnuStackPush()));
	menu->addAction(tr("P&op %1").arg(stack_type_name), this, SLOT(mnuStackPop()));

	// lockable stack feature
	menu->addSeparator();
	QAction *const action = new QAction(tr("&Lock Stack"), this);
    action->setCheckable(true);
    action->setChecked(stack_view_locked_);
	menu->addAction(action);
	connect(action, SIGNAL(toggled(bool)), SLOT(mnuStackToggleLock(bool)));

	add_plugin_context_menu(menu, &IPlugin::stack_context_menu);

	menu->exec(stack_view_->mapToGlobal(pos));
	delete menu;
}

//------------------------------------------------------------------------------
// Name: mnuDumpContextMenu(const QPoint &pos)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuDumpContextMenu(const QPoint &pos) {
	QHexView *const s = qobject_cast<QHexView *>(sender());

	Q_CHECK_PTR(s);

	QMenu *const menu = s->createStandardContextMenu();
	menu->addSeparator();
	menu->addAction(tr("Follow Address In &CPU"), this, SLOT(mnuDumpFollowInCPU()));
	menu->addAction(tr("Follow Address In &Dump"), this, SLOT(mnuDumpFollowInDump()));
	menu->addAction(tr("Follow Address In &Stack"), this, SLOT(mnuDumpFollowInStack()));
	menu->addAction(tr("&Goto Address"), this, SLOT(mnuDumpGotoAddress()));
	menu->addSeparator();
	menu->addAction(tr("&Edit Bytes"), this, SLOT(mnuDumpModify()));
	menu->addSeparator();
	menu->addAction(tr("&Save To File"), this, SLOT(mnuDumpSaveToFile()));

	add_plugin_context_menu(menu, &IPlugin::data_context_menu);

	menu->exec(s->mapToGlobal(pos));
	delete menu;
}

//------------------------------------------------------------------------------
// Name: mnuDumpSaveToFile()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuDumpSaveToFile() {
	QHexView *const s = qobject_cast<QHexView *>(ui->tabWidget->currentWidget());

	Q_CHECK_PTR(s);

	const QString filename = QFileDialog::getSaveFileName(
		this,
		tr("Save File"),
		last_open_directory_);

	if(!filename.isEmpty()) {
		QFile file(filename);
		file.open(QIODevice::WriteOnly);
		if(file.isOpen()) {
			file.write(s->allBytes());
		}
	}
}

//------------------------------------------------------------------------------
// Name: cpu_fill(quint8 byte)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::cpu_fill(quint8 byte) {
	const yad64::address_t address = ui->cpuView->selectedAddress();
	const unsigned int size      = ui->cpuView->selectedSize();

	if(size != 0) {
		if(yad64::v1::overwrite_check(address, size)) {
			QByteArray bytes(size, byte);

			yad64::v1::debugger_core->write_bytes(address, bytes.data(), size);

			// do a refresh, not full update
			refresh_gui();
		}
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPUAddBreakpoint()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuCPUAddBreakpoint() {
	const yad64::address_t address = ui->cpuView->selectedAddress();
	yad64::v1::create_breakpoint(address);
}

//------------------------------------------------------------------------------
// Name: mnuCPUAddConditionalBreakpoint()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuCPUAddConditionalBreakpoint() {
	bool ok;
	const yad64::address_t address = ui->cpuView->selectedAddress();
	const QString condition = QInputDialog::getText(this, tr("Set Breakpoint Condition"), tr("Expression:"), QLineEdit::Normal, QString(), &ok);
	if(ok) {
		yad64::v1::create_breakpoint(address);
		if(!condition.isEmpty()) {
			yad64::v1::set_breakpoint_condition(address, condition);
		}
	}
}

//------------------------------------------------------------------------------
// Name: mnuCPURemoveBreakpoint()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuCPURemoveBreakpoint() {
	const yad64::address_t address = ui->cpuView->selectedAddress();
	yad64::v1::remove_breakpoint(address);
}

//------------------------------------------------------------------------------
// Name: mnuCPUFillZero()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuCPUFillZero() {
	cpu_fill(0x00);
}

//------------------------------------------------------------------------------
// Name: mnuCPUFillNop()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuCPUFillNop() {
	// TODO: get system independent nop-code
	cpu_fill(0x90);
}

//------------------------------------------------------------------------------
// Name: mnuCPUSetEIP()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuCPUSetEIP() {
	const yad64::address_t address = ui->cpuView->selectedAddress();
	State state;
	yad64::v1::debugger_core->get_state(state);
	state.set_instruction_pointer(address);
	yad64::v1::debugger_core->set_state(state);
	update_gui();
}

//------------------------------------------------------------------------------
// Name: mnuCPUModify()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuCPUModify() {
	const yad64::address_t address = ui->cpuView->selectedAddress();
	const unsigned int size      = ui->cpuView->selectedSize();

	quint8 buf[yad64::Instruction::MAX_SIZE];

	Q_ASSERT(size <= sizeof(buf));

	const bool ok = yad64::v1::debugger_core->read_bytes(address, buf, size);
	if(ok) {
		QByteArray bytes = QByteArray::fromRawData(reinterpret_cast<const char *>(buf), size);
		yad64::v1::modify_bytes(address, size, bytes, 0x00);
	}
}

//------------------------------------------------------------------------------
// Name: modify_bytes(const T &hv)
// Desc:
//------------------------------------------------------------------------------
template <class T>
void DebuggerMain::modify_bytes(const T &hv) {
	if(hv) {
		const yad64::address_t address = hv->selectedBytesAddress();
		const unsigned int size      = hv->selectedBytesSize();
		QByteArray bytes             = hv->selectedBytes();

		yad64::v1::modify_bytes(address, size, bytes, 0x00);
	}
}

//------------------------------------------------------------------------------
// Name: mnuDumpModify()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuDumpModify() {
	modify_bytes(qobject_cast<QHexView *>(ui->tabWidget->currentWidget()));
}

//------------------------------------------------------------------------------
// Name: mnuStackModify()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::mnuStackModify() {
	modify_bytes(stack_view_);
}

//------------------------------------------------------------------------------
// Name: breakpoint_condition_true(const QString &condition) const
// Desc:
//------------------------------------------------------------------------------
bool DebuggerMain::breakpoint_condition_true(const QString &condition) {

	yad64::address_t condition_value;
	if(!yad64::v1::eval_expression(condition, condition_value)) {
		return true;
	}
	return condition_value;
}

//------------------------------------------------------------------------------
// Name: handle_trap()
// Desc: returns true if we should resume as if this trap never happened
//------------------------------------------------------------------------------
yad64::EVENT_STATUS DebuggerMain::handle_trap() {

	// we just got a trap event, there are a few possible causes
	// #1: we hit a 0xcc breakpoint, if so, then we want to stop
	// #2: we did a step
	// #3: we hit a 0xcc naturally in the program
	// #4: we hit a hardware breakpoint!
	State state;
	yad64::v1::debugger_core->get_state(state);

	const yad64::address_t previous_ip = state.instruction_pointer() - yad64::v1::debugger_core->breakpoint_size();

	// look it up in our breakpoint list, make sure it is one of OUR int3s!
	// if it is, we need to backup EIP and pause ourselves
	IBreakpoint::pointer bp = yad64::v1::find_breakpoint(previous_ip);
	if(bp && bp->enabled()) {

		// TODO: check if the breakpoint was corrupted
		bp->hit();

		// back up eip the size of a breakpoint, since we executed a breakpoint
		// instead of the real code that belongs there
		state.set_instruction_pointer(previous_ip);
		yad64::v1::debugger_core->set_state(state);

		const QString condition = bp->condition;

		// handle conditional breakpoints
		if(!condition.isEmpty()) {
			if(!breakpoint_condition_true(condition)) {
				return yad64::DEBUG_CONTINUE;
			}
		}

		// if it's a one time breakpoint then we should remove it upon
		// triggering, this is mainly used for situations like step over

		if(bp->one_time()) {
			yad64::v1::debugger_core->remove_breakpoint(bp->address());
		}
	}

	return step_run_ ? yad64::DEBUG_CONTINUE : yad64::DEBUG_STOP;
}

//------------------------------------------------------------------------------
// Name: handle_event_stopped(const DebugEvent &event)
// Desc:
//------------------------------------------------------------------------------
yad64::EVENT_STATUS DebuggerMain::handle_event_stopped(const DebugEvent &event) {

	// ok we just came in from a stop, we need to test some things,
	// generally, we will want to check if it was a step, if it was, was it
	// because we just hit a break point or because we wanted to run, but had
	// to step first in case were were on a breakpoint already...

	if(event.is_error()) {
		const DebugEvent::Message message = event.error_description();
		QMessageBox::information(this, message.caption, message.message);
		return yad64::DEBUG_STOP;
	}

	switch(event.stop_code()) {
	case DebugEvent::sigstop:
		// user asked to pause the debugged process
		return yad64::DEBUG_STOP;

#ifdef Q_OS_UNIX
	case SIGCHLD:
	case SIGPROF:
		return yad64::DEBUG_EXCEPTION_NOT_HANDLED;
#endif

	case DebugEvent::sigtrap:
		return handle_trap();

	default:
		QMessageBox::information(this, tr("Stop Event"),
			tr(
			"<p>The debugged has received a stop event. <strong>%1</strong></p>"
			"<p>If you would like to pass this event to the application press Shift+[F7/F8/F9]</p>"
			"<p>If you would like to ignore this event, press [F7/F8/F9]</p>").arg(event.stop_code()));
		return yad64::DEBUG_STOP;
	}
}

//------------------------------------------------------------------------------
// Name: handle_event_exited(const DebugEvent &event)
// Desc:
//------------------------------------------------------------------------------
yad64::EVENT_STATUS DebuggerMain::handle_event_exited(const DebugEvent &event) {
	QMessageBox::information(
		this,
		tr("Application Exited"),
		tr("The debugged application exited normally with exit code %1.").arg(event.exit_code())
		);

	on_action_Detach_triggered();
	return yad64::DEBUG_STOP;
}

//------------------------------------------------------------------------------
// Name: handle_event_signaled(const DebugEvent &event)
// Desc:
//------------------------------------------------------------------------------
yad64::EVENT_STATUS DebuggerMain::handle_event_signaled(const DebugEvent &event) {
	switch(event.signal_code()) {
	case DebugEvent::sigkill:
		QMessageBox::information(this, tr("Application Killed"), tr("The debugged application was killed."));
		break;


	default:
		qDebug() << "Unknown Signal Received! " << event.signal_code();
		break;
	}

	on_action_Detach_triggered();
	return yad64::DEBUG_STOP;
}

//------------------------------------------------------------------------------
// Name: current_instruction_is_return() const
// Desc:
//------------------------------------------------------------------------------
bool DebuggerMain::current_instruction_is_return() const {
	State state;
	yad64::v1::debugger_core->get_state(state);
	const yad64::address_t address = state.instruction_pointer();

	quint8 buffer[yad64::Instruction::MAX_SIZE + 1];
	int size = sizeof(buffer);

	if(yad64::v1::get_instruction_bytes(address, buffer, size)) {
		yad64::Instruction insn(buffer, buffer + size, address, std::nothrow);
		if(insn.valid() && insn.type() == yad64::Instruction::OP_RET) {
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: handle_event(const DebugEvent &event)
// Desc:
//------------------------------------------------------------------------------
yad64::EVENT_STATUS DebuggerMain::handle_event(const DebugEvent &event) {

	Q_CHECK_PTR(yad64::v1::debugger_core);

	yad64::EVENT_STATUS status;
	switch(event.reason()) {
	// most events
	case DebugEvent::EVENT_STOPPED:
		status = handle_event_stopped(event);
		break;

	// usually an abnormal exit event (unhandled signal/etc)
	case DebugEvent::EVENT_SIGNALED:
		status = handle_event_signaled(event);
		break;

	// normal exit
	case DebugEvent::EVENT_EXITED:
		status = handle_event_exited(event);
		break;

	default:
		Q_ASSERT(false);
		return yad64::DEBUG_EXCEPTION_NOT_HANDLED;
	}

	// re-enable any breakpoints we previously disabled
	if(reenable_breakpoint_) {
		reenable_breakpoint_->enable();
		reenable_breakpoint_.clear();
	}

	return status;
}

//------------------------------------------------------------------------------
// Name: debug_event_handler(const DebugEvent &event)
// Desc:
//------------------------------------------------------------------------------
yad64::EVENT_STATUS DebuggerMain::debug_event_handler(const DebugEvent &event) {
	IDebugEventHandler *const handler = yad64::v1::debug_event_handler();
	Q_CHECK_PTR(handler);
	return handler->handle_event(event);
}

//------------------------------------------------------------------------------
// Name: update_tab_caption(const QSharedPointer<QHexView> &view, yad64::address_t start, yad64::address_t end)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::update_tab_caption(const QSharedPointer<QHexView> &view, yad64::address_t start, yad64::address_t end) {
	const int index = ui->tabWidget->indexOf(view.data());
	const QString caption = ui->tabWidget->data(index).toString();

	if(caption.isEmpty()) {
		ui->tabWidget->setTabText(index, tr("%1-%2").arg(yad64::v1::format_pointer(start), yad64::v1::format_pointer(end)));
	} else {
		ui->tabWidget->setTabText(index, tr("[%1] %2-%3").arg(caption, yad64::v1::format_pointer(start), yad64::v1::format_pointer(end)));
	}
}

//------------------------------------------------------------------------------
// Name: update_data(const DataViewInfo::pointer &v)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::update_data(const DataViewInfo::pointer &v) {

	Q_CHECK_PTR(v);

	const QSharedPointer<QHexView> &view = v->view;

	Q_CHECK_PTR(view);

	v->update();

	update_tab_caption(view, v->region.start(), v->region.end());
}

//------------------------------------------------------------------------------
// Name: clear_data(const DataViewInfo::pointer &v)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::clear_data(const DataViewInfo::pointer &v) {

	Q_CHECK_PTR(v);

	const QSharedPointer<QHexView> &view = v->view;

	Q_CHECK_PTR(view);

	view->clear();
	view->scrollTo(0);

	update_tab_caption(view, 0, 0);
}

//------------------------------------------------------------------------------
// Name: do_jump_to_address(yad64::address_t address, const MemoryRegion &r, bool scrollTo)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::do_jump_to_address(yad64::address_t address, const MemoryRegion &r, bool scrollTo) {
	ui->cpuView->setAddressOffset(r.start());
	ui->cpuView->setRegion(r);
	if(scrollTo && !ui->cpuView->addressShown(address)) {
		ui->cpuView->scrollTo(address);
	}
}

//------------------------------------------------------------------------------
// Name: update_disassembly(yad64::address_t address, const MemoryRegion &r)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::update_disassembly(yad64::address_t address, const MemoryRegion &r) {
	ui->cpuView->setCurrentAddress(address);
	do_jump_to_address(address, r, true);
	list_model_->setStringList(yad64::v1::arch_processor().update_instruction_info(address));
}

//------------------------------------------------------------------------------
// Name: update_stack_view(const State &state)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::update_stack_view(const State &state) {	
	if(!yad64::v1::dump_stack(state.stack_pointer(), !stack_view_locked_)) {
		stack_view_->clear();
		stack_view_->scrollTo(0);
	}
}

//------------------------------------------------------------------------------
// Name: update_cpu_view(const State &state, MemoryRegion &region)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::update_cpu_view(const State &state, MemoryRegion &region) {

	const yad64::address_t address = state.instruction_pointer();

	if(!yad64::v1::memory_regions().find_region(address, region)) {
		ui->cpuView->clear();
		ui->cpuView->scrollTo(0);
		list_model_->setStringList(QStringList());
	} else {
		update_disassembly(address, region);
	}
}

//------------------------------------------------------------------------------
// Name: update_data_views()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::update_data_views() {

	// update all data views with the current region data
	Q_FOREACH(const DataViewInfo::pointer &info, data_regions_) {

		// make sure the regions are still valid..
		if(yad64::v1::memory_regions().find_region(info->region.start())) {
			update_data(info);
		} else {
			clear_data(info);
		}
	}
}

//------------------------------------------------------------------------------
// Name: refresh_gui()
// Desc: refreshes all the different displays
//------------------------------------------------------------------------------
void DebuggerMain::refresh_gui() {
	ui->cpuView->repaint();
	stack_view_->repaint();

	Q_FOREACH(const DataViewInfo::pointer &info, data_regions_) {
		info->view->repaint();
	}

	State state;
	yad64::v1::debugger_core->get_state(state);
	list_model_->setStringList(yad64::v1::arch_processor().update_instruction_info(state.instruction_pointer()));
}

//------------------------------------------------------------------------------
// Name: update_gui()
// Desc: updates all the different displays
//------------------------------------------------------------------------------
void DebuggerMain::update_gui() {
	
	if(yad64::v1::debugger_core) {
		State state;
		yad64::v1::debugger_core->get_state(state);
		
		MemoryRegion region;
		update_cpu_view(state, region);
		update_data_views();
		update_stack_view(state);
		yad64::v1::arch_processor().update_register_view(region.name());
	}
}

//------------------------------------------------------------------------------
// Name: resume_status(bool pass_exception)
// Desc:
//------------------------------------------------------------------------------
yad64::EVENT_STATUS DebuggerMain::resume_status(bool pass_exception) {
	if(pass_exception && last_event_.stopped() && !is_trap(last_event_)) {
		return yad64::DEBUG_EXCEPTION_NOT_HANDLED;
	} else {
		return yad64::DEBUG_CONTINUE;
	}
}

//------------------------------------------------------------------------------
// Name: resume_execution(EXCEPTION_RESUME pass_exception, DEBUG_MODE mode)
// Desc: resumes execution, handles the situation of being on a breakpoint as well
//------------------------------------------------------------------------------
void DebuggerMain::resume_execution(EXCEPTION_RESUME pass_exception, DEBUG_MODE mode) {

	if(step_run_) {
		step_run_ = false;
		yad64::v1::debugger_core->resume(yad64::DEBUG_CONTINUE);
	} else {

		// if necessary pass the trap to the application, otherwise just resume
		// as normal
		const yad64::EVENT_STATUS status = resume_status(pass_exception == PASS_EXCEPTION);

		// if we are on a breakpoint, disable it
		State state;
		yad64::v1::debugger_core->get_state(state);
		reenable_breakpoint_ = yad64::v1::find_breakpoint(state.instruction_pointer());
		if(reenable_breakpoint_) {
			reenable_breakpoint_->disable();

			step_run_ = (mode == MODE_RUN);

			yad64::v1::debugger_core->step(status);
		} else {
			// we get here, we are not sitting on a BP, we can just directly do what we wanted
			step_run_ = false;

			if(mode == MODE_RUN) {
				yad64::v1::debugger_core->resume(status);
			} else {
				yad64::v1::debugger_core->step(status);
			}
		}
	}

	// set the state to 'running'
	update_menu_state(RUNNING);
}

//------------------------------------------------------------------------------
// Name: on_action_Run_Pass_Signal_To_Application_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Run_Pass_Signal_To_Application_triggered() {
	resume_execution(PASS_EXCEPTION, MODE_RUN);
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Into_Pass_Signal_To_Application_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Step_Into_Pass_Signal_To_Application_triggered() {
	resume_execution(PASS_EXCEPTION, MODE_STEP);
}

//------------------------------------------------------------------------------
// Name: on_action_Run_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Run_triggered() {
	resume_execution(IGNORE_EXCEPTION, MODE_RUN);
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Into_triggered(0
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Step_Into_triggered() {
	resume_execution(IGNORE_EXCEPTION, MODE_STEP);
}

//------------------------------------------------------------------------------
// Name: on_action_Detach_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Detach_triggered() {
	detach_from_process(NO_KILL_ON_DETACH);
}

//------------------------------------------------------------------------------
// Name: on_action_Kill_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Kill_triggered() {
	detach_from_process(KILL_ON_DETACH);
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Over_Pass_Signal_To_Application_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Step_Over_Pass_Signal_To_Application_triggered() {
	yad64::detail::step_over(
		boost::bind(&DebuggerMain::on_action_Run_Pass_Signal_To_Application_triggered, this),
		boost::bind(&DebuggerMain::on_action_Step_Into_Pass_Signal_To_Application_triggered, this));
}

//------------------------------------------------------------------------------
// Name: on_action_Step_Over_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Step_Over_triggered() {
	yad64::detail::step_over(
		boost::bind(&DebuggerMain::on_action_Run_triggered, this),
		boost::bind(&DebuggerMain::on_action_Step_Into_triggered, this));
}

//------------------------------------------------------------------------------
// Name: on_actionRun_Until_Return_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_actionRun_Until_Return_triggered() {
	new RunUntilRet(this);
	resume_execution(PASS_EXCEPTION, MODE_STEP);
}

//------------------------------------------------------------------------------
// Name: on_action_Pause_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Pause_triggered() {
	Q_CHECK_PTR(yad64::v1::debugger_core);
	yad64::v1::debugger_core->pause();
}

//------------------------------------------------------------------------------
// Name: cleanup_debugger()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::cleanup_debugger() {

	timer_->stop();

	yad64::v1::memory_regions().clear();
	yad64::v1::symbol_manager().clear();
	yad64::v1::arch_processor().reset();

	// clear up the data view
	while(ui->tabWidget->count() > 1) {
		mnuDumpDeleteTab();
	}

	ui->tabWidget->setData(0, QString());

	Q_ASSERT(!data_regions_.isEmpty());
	data_regions_.first()->region = MemoryRegion();

	setWindowTitle(tr("yad64"));

	update_gui();
}

//------------------------------------------------------------------------------
// Name: session_filename() const
// Desc:
//------------------------------------------------------------------------------
QString DebuggerMain::session_filename() const {
	if(!program_executable_.isEmpty()) {
		const QString basename = yad64::v1::basename(program_executable_);
		return QString("%1/%2.yad64").arg(yad64::v1::config().session_path, basename);
	}
	return QString();
}

//------------------------------------------------------------------------------
// Name: detach_from_process(DETACH_ACTION kill)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::detach_from_process(DETACH_ACTION kill) {

	if(ISessionFile *const session_file = yad64::v1::session_file_handler()) {
		const QString filename = session_filename();
		if(!filename.isEmpty()) {
			session_file->save_session(filename, program_executable_);
			program_executable_.clear();
		}
	}

	if(yad64::v1::debugger_core) {
		if(kill == KILL_ON_DETACH) yad64::v1::debugger_core->kill();
		else                       yad64::v1::debugger_core->detach();
	}

	cleanup_debugger();
	update_menu_state(TERMINATED);
}

//------------------------------------------------------------------------------
// Name: set_initial_debugger_state()
// Desc: resets all of the basic data to sane defaults
//------------------------------------------------------------------------------
void DebuggerMain::set_initial_debugger_state() {

	update_menu_state(PAUSED);
	timer_->start(0);

	yad64::v1::symbol_manager().load_symbols(yad64::v1::config().symbol_path);
	yad64::v1::memory_regions().sync();

	Q_ASSERT(data_regions_.size() > 0);

	data_regions_.first()->region = yad64::v1::primary_data_region();

	if(IAnalyzer *const analyzer = yad64::v1::analyzer()) {
		analyzer->invalidate_analysis();
	}

	step_run_ = false;
#ifdef Q_OS_UNIX
	debug_pointer_ = 0;
#endif
	const QString executable = yad64::v1::debugger_core->process_exe(yad64::v1::debugger_core->pid());

	set_debugger_caption(executable);

	program_executable_.clear();
	if(yad64::v1::debugger_core->pid() != 0) {
		program_executable_ = executable;
	}

	if(ISessionFile *const session_file = yad64::v1::session_file_handler()) {
		const QString filename = session_filename();
		if(!filename.isEmpty()) {
			session_file->load_session(filename, program_executable_);
		}
	}

	// create our binary info object for the primary code module
	binary_info_.reset(yad64::v1::get_binary_info(yad64::v1::primary_code_region()));

	stack_comment_server_->clear();
	if(binary_info_) {
		stack_comment_server_->set_comment(binary_info_->entry_point(), "<entry point>");
	}
}

//------------------------------------------------------------------------------
// Name: test_native_binary()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::test_native_binary() {
	if(binary_info_ && !binary_info_->native()) {
		QMessageBox::warning(
			this,
			tr("Not A Native Binary"),
			tr("The program you just attached to was built for a different architecture than the one that yad64 was built for. "
			"For example a 32-bit binary on x86-64. "
			"This is not supported yet, so you may need to use a version of yad64 that was compiled for the same architecture as your target program")
			);
	}
}

//------------------------------------------------------------------------------
// Name: set_initial_breakpoint(const QString &s)
// Desc: sets the initial breakpoint so we can stop at the entry point of the
//       application
//------------------------------------------------------------------------------
#include <QMessageBox>
void DebuggerMain::set_initial_breakpoint(const QString &s) {
	yad64::address_t entryPoint = 0;

	if(yad64::v1::config().initial_breakpoint == Configuration::MainSymbol) {
		const QString mainSymbol = yad64::v1::basename(s) + "::main";
		const Symbol::pointer sym = yad64::v1::symbol_manager().find(mainSymbol);

		if(sym) {
			entryPoint = sym->address;
		} else if(yad64::v1::config().find_main) {
			entryPoint = yad64::v1::locate_main_function();
		}
	}

	if(entryPoint == 0 || yad64::v1::config().initial_breakpoint == Configuration::EntryPoint) {
        QMessageBox::information(0,"EntryPoint","");
		if(binary_info_) {
            QMessageBox::information(0,"binary_info_","");
			entryPoint = binary_info_->entry_point();
		}
	}

	if(entryPoint != 0) {
		if(IBreakpoint::pointer bp = yad64::v1::debugger_core->add_breakpoint(entryPoint)) {
			bp->set_one_time(true);
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Restart_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Restart_triggered() {

	Q_CHECK_PTR(yad64::v1::debugger_core);

    const yad64::pid_t pid = yad64::v1::debugger_core->pid();

	working_directory_     = yad64::v1::debugger_core->process_cwd(pid);
	QList<QByteArray> args = yad64::v1::debugger_core->process_args(pid);
	const QString s        = yad64::v1::debugger_core->process_exe(pid);

	if(!args.empty()) {
		args.removeFirst();
	}

	if(!s.isEmpty()) {
		detach_from_process(KILL_ON_DETACH);
		common_open(s, args);
	}
}

//------------------------------------------------------------------------------
// Name: common_open(const QString &s, const QStringList &args)
// Desc:
//------------------------------------------------------------------------------
bool DebuggerMain::common_open(const QString &s, const QList<QByteArray> &args) {

	bool ret = false;
	if(!QFile(s).exists()) {
		QMessageBox::information(
			this,
			tr("Could Not Open"),
			tr("The specified file (%1) does not appear to exist, please check privileges and try again.").arg(s));
	} else {

		const QString tty = create_tty();

		if(yad64::v1::debugger_core->open(s, working_directory_, args, tty)) {
			set_initial_debugger_state();
			test_native_binary();
			set_initial_breakpoint(s);
			ret = true;
		} else {
			QMessageBox::information(
				this,
				tr("Could Not Open"),
				tr("Failed to open and attach to process, please check privileges and try again."));
		}
	}


	update_gui();
	return ret;
}

//------------------------------------------------------------------------------
// Name: execute(const QString &program, const QList<QByteArray> &args)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::execute(const QString &program, const QList<QByteArray> &args) {
	if(common_open(program, args)) {
		recent_file_manager_->add_file(program);
	}
}

//------------------------------------------------------------------------------
// Name: open_file(const QString &s)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::open_file(const QString &s) {
	if(!s.isEmpty()) {
		last_open_directory_ = QFileInfo(s).canonicalFilePath();

		detach_from_process(NO_KILL_ON_DETACH);

		execute(s, arguments_dialog_->arguments());
	}
}

//------------------------------------------------------------------------------
// Name: attach(yad64::pid_t pid)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::attach(yad64::pid_t pid) {

	// TODO: we need a core concept of debugger capabilities which
	// may restrict some actions

#ifdef Q_OS_UNIX
	yad64::pid_t current_pid = getpid();
	while(current_pid != 0) {
		if(current_pid == pid) {
			QMessageBox::information(
				this,
				tr("Attach"),
				tr("You may not debug a process which is a parent of the yad64 process."));
			return;
		}
		current_pid = yad64::v1::debugger_core->parent_pid(current_pid);
	}
#endif

	if(pid == yad64::v1::debugger_core->pid()) {
		QMessageBox::information(
			this,
			tr("Attach"),
			tr("You are already debugging that process!"));
	} else {


		detach_from_process(NO_KILL_ON_DETACH);

		if(yad64::v1::debugger_core->attach(pid)) {

			working_directory_ = yad64::v1::debugger_core->process_cwd(yad64::v1::debugger_core->pid());

			set_initial_debugger_state();

			QList<QByteArray> args = yad64::v1::debugger_core->process_args(yad64::v1::debugger_core->pid());
			
			if(!args.empty()) {
				args.removeFirst();
			}

			arguments_dialog_->set_arguments(args);
		} else {
			QMessageBox::information(this, tr("Attach"), tr("Failed to attach to process, please check privileges and try again."));
		}

		update_gui();
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Open_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Open_triggered() {

	// TODO: we need a core concept of debugger capabilities which
	// may restrict some actions

	const QString filename = QFileDialog::getOpenFileName(
		this,
		tr("Choose a file"),
		last_open_directory_);

	open_file(filename);
}

//------------------------------------------------------------------------------
// Name: on_action_Attach_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Attach_triggered() {

	// TODO: we need a core concept of debugger capabilities which
	// may restrict some actions

	QPointer<DialogAttach> dlg = new DialogAttach(this);

	if(dlg->exec() == QDialog::Accepted) {
		if(dlg) {
			bool ok;
			const yad64::pid_t pid = dlg->selected_pid(ok);
			if(ok) {
				attach(pid);
			}
		}
	}
	
	delete dlg;
}

//------------------------------------------------------------------------------
// Name: on_action_Memory_Regions_triggered()
// Desc: displays the memory regions dialog, and optionally dumps some data
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Memory_Regions_triggered() {

	// TODO: we need a core concept of debugger capabilities which
	// may restrict some actions

	QPointer<DialogMemoryRegions> dlg = new DialogMemoryRegions(this);
	dlg->exec();	
	delete dlg;
}


//------------------------------------------------------------------------------
// Name: on_action_Threads_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Threads_triggered() {

	// TODO: we need a core concept of debugger capabilities which
	// may restrict some actions

	QPointer<DialogThreads> dlg = new DialogThreads(this);

	if(dlg->exec() == QDialog::Accepted) {
		if(dlg) {
			yad64::tid_t tid = dlg->selected_thread();
			if(tid != 0) {
				yad64::v1::debugger_core->set_active_thread(tid);
				update_gui();
			}
		}
	}
	
	delete dlg;
}

//------------------------------------------------------------------------------
// Name: mnuDumpCreateTab()
// Desc: duplicates the current tab creating a new one
//------------------------------------------------------------------------------
void DebuggerMain::mnuDumpCreateTab() {
	create_data_tab();
	del_tab_->setEnabled(ui->tabWidget->count() > 1);
}

//------------------------------------------------------------------------------
// Name: mnuDumpDeleteTab()
// Desc: handles removing of a memory view tab
//------------------------------------------------------------------------------
void DebuggerMain::mnuDumpDeleteTab() {
	delete_data_tab();
	del_tab_->setEnabled(ui->tabWidget->count() > 1);
}

//------------------------------------------------------------------------------
// Name: add_plugin_context_menu(const T &menu, const F &f)
// Desc:
//------------------------------------------------------------------------------
template <class F, class T>
void DebuggerMain::add_plugin_context_menu(const T &menu, const F &f) {
	Q_FOREACH(QObject *plugin, yad64::v1::plugin_list()) {
		if(IPlugin *const p = qobject_cast<IPlugin *>(plugin)) {
			const QList<QAction *> acts = (p->*f)();
			if(!acts.isEmpty()) {
				menu->addSeparator();
				menu->addActions(acts);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_action_Plugins_triggered()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::on_action_Plugins_triggered() {
	static QDialog *const dlg = new DialogPlugins(this);
	dlg->show();
}

//------------------------------------------------------------------------------
// Name: jump_to_address(yad64::address_t address)
// Desc:
//------------------------------------------------------------------------------
bool DebuggerMain::jump_to_address(yad64::address_t address) {
	MemoryRegion region;
	if(yad64::v1::memory_regions().find_region(address, region)) {
		do_jump_to_address(address, region, true);
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: dump_data_range(yad64::address_t address, yad64::address_t end_address, bool new_tab)
// Desc:
//------------------------------------------------------------------------------
bool DebuggerMain::dump_data_range(yad64::address_t address, yad64::address_t end_address, bool new_tab) {
	MemoryRegion region;
	if(yad64::v1::memory_regions().find_region(address, region)) {
		if(new_tab) {
			mnuDumpCreateTab();
		}

		DataViewInfo::pointer info = current_data_view_info();

		if(info) {
			info->region = region;

			if(info->region.contains(end_address)) {
				info->region = MemoryRegion(region.start(), end_address, region.base(), region.name(), region.permissions());
			}

			if(info->region.contains(address)) {
				info->region = MemoryRegion(address, region.end(), region.base(), region.name(), region.permissions());
			}

			update_data(info);
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: dump_data(yad64::address_t address, bool new_tab)
// Desc:
//------------------------------------------------------------------------------
bool DebuggerMain::dump_data(yad64::address_t address, bool new_tab) {
	MemoryRegion region;
	if(yad64::v1::memory_regions().find_region(address, region)) {
		if(new_tab) {
			mnuDumpCreateTab();
		}

		DataViewInfo::pointer info = current_data_view_info();

		if(info) {
			info->region = region;
			update_data(info);
			info->view->scrollTo(address - info->region.start());
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: dump_stack(yad64::address_t address, bool scroll_to)
// Desc:
//------------------------------------------------------------------------------
bool DebuggerMain::dump_stack(yad64::address_t address, bool scroll_to) {
	const MemoryRegion last_region = stack_view_info_.region;

	if(yad64::v1::memory_regions().find_region(address, stack_view_info_.region)) {
		stack_view_info_.update();
		if(scroll_to || stack_view_info_.region != last_region) {
			stack_view_->scrollTo(address - stack_view_info_.region.start());
		}
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: tab_context_menu(int index, const QPoint &pos)
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::tab_context_menu(int index, const QPoint &pos) {
	QMenu menu;
	QAction *const actionAdd   = menu.addAction(tr("&Set Label"));
	QAction *const actionClear = menu.addAction(tr("&Clear Label"));
	QAction *const chosen      = menu.exec(ui->tabWidget->mapToGlobal(pos));

	if(chosen == actionAdd) {
		bool ok;
		const QString text = QInputDialog::getText(
			this,
			tr("Set Caption"),
			tr("Caption:"),
			QLineEdit::Normal,
			ui->tabWidget->data(index).toString(),
			&ok);

		if(ok && !text.isEmpty()) {
			ui->tabWidget->setData(index, text);
		}
	} else if(chosen == actionClear) {
		ui->tabWidget->setData(index, QString());
	}

	update_gui();
}

//------------------------------------------------------------------------------
// Name: next_debug_event()
// Desc:
//------------------------------------------------------------------------------
void DebuggerMain::next_debug_event() {


	// TODO: come up with a nice system to inject a debug event
	//       into the system, for example on windows, we want
	//       to deliver real "memory map updated" events, but 
	//       on linux, (at least for now), I would want to send
	//       a fake one before every event so it is always up to
	//       date.

	Q_CHECK_PTR(yad64::v1::debugger_core);

	DebugEvent e;
	if(yad64::v1::debugger_core->wait_debug_event(e, 10)) {
	
		last_event_ = e;

		// TODO: figure out a way to do this less often, if they map an obscene 
		// number of regions, this really slows things down
		yad64::v1::memory_regions().sync();

		// TODO: make the system use this information, this is huge! it will 
		// allow us to have restorable breakpoints...even in libraries!
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
		if(debug_pointer_ == 0 && binary_info_) {
			if((debug_pointer_ = binary_info_->debug_pointer()) != 0) {
				r_debug dynamic_info;
				const bool ok = yad64::v1::debugger_core->read_bytes(debug_pointer_, &dynamic_info, sizeof(dynamic_info));
				if(ok) {
				#if 0
					qDebug("READ DYNAMIC INFO! %p", dynamic_info.r_brk);
				#endif
				}
			}

		}
	#if 0
		qDebug("DEBUG POINTER: %p", debug_pointer_);
	#endif
#endif

		const yad64::EVENT_STATUS status = debug_event_handler(e);
		switch(status) {
		case yad64::DEBUG_STOP:
			step_run_ = false;
			update_gui();
			update_menu_state((yad64::v1::debugger_core->pid() != 0) ? PAUSED : TERMINATED);
			break;
		case yad64::DEBUG_CONTINUE:
			resume_execution(IGNORE_EXCEPTION, MODE_RUN);
			break;
		case yad64::DEBUG_CONTINUE_STEP:
			resume_execution(IGNORE_EXCEPTION, MODE_STEP);
			break;
		case yad64::DEBUG_EXCEPTION_NOT_HANDLED:
			resume_execution(PASS_EXCEPTION, MODE_RUN);
			break;
		}
	}
}
