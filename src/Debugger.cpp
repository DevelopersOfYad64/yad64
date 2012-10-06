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

/*
 * NOTE: system specific functions may be implemented in
 * src/os/[*]/Debugger*.cpp this is to isolate the system specific code away
 * from the general code.
 */

#include "Debugger.h"
#include "ArchProcessor.h"
#include "BinaryString.h"
#include "ByteShiftArray.h"
#include "Configuration.h"
#include "IDebuggerCore.h"
#include "DebuggerMain.h"
#include "IPlugin.h"
#include "DialogInputBinaryString.h"
#include "DialogInputValue.h"
#include "DialogOptions.h"
#include "Expression.h"
#include "FunctionInfo.h"
#include "MD5.h"
#include "MemoryRegions.h"
#include "QHexView"
#include "State.h"
#include "SymbolManager.h"
#include "version.h"
#include "serializer.h"
#include "qobjecthelper.h"

#include <QAction>
#include <QAtomicPointer>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QString>
#include <QDomDocument>

#include <cctype>

#include "ui_debuggerui.h"

IDebuggerCore *yad64::v1::debugger_core = 0;
QWidget       *yad64::v1::debugger_ui   = 0;

namespace {

	typedef QList<IBinary::create_func_ptr_t> BinaryInfoList;

	QAtomicPointer<IDebugEventHandler> g_DebugEventHandler = 0;
	QAtomicPointer<IAnalyzer>          g_Analyzer          = 0;
	QAtomicPointer<ISessionFile>       g_SessionHandler    = 0;
	QHash<QString, QObject *>          g_GeneralPlugins;
	BinaryInfoList                     g_BinaryInfoList;
	
	QHash<QString, FunctionInfo>       g_FunctionDB;
	
	DebuggerMain *ui() {
		return qobject_cast<DebuggerMain *>(yad64::v1::debugger_ui);
	}

	bool function_symbol_base(yad64::address_t address, QString &value, int &offset) {
		bool ret = false;
		offset = 0;
		const Symbol::pointer s = yad64::v1::symbol_manager().find_near_symbol(address);
		if(s) {
			value = s->name;
			offset = address - s->address;
			ret = true;
		}
		return ret;
	}
}

namespace yad64 {
namespace internal {

//------------------------------------------------------------------------------
// Name: register_plugin(const QString &filename, QObject *plugin)
// Desc:
//------------------------------------------------------------------------------
bool register_plugin(const QString &filename, QObject *plugin) {
	if(!g_GeneralPlugins.contains(filename)) {
		g_GeneralPlugins[filename] = plugin;
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
void load_function_db() {
	QFile file(":/debugger/xml/functions.xml");
	QDomDocument doc;
		
	if(file.open(QIODevice::ReadOnly)) {	
		if(doc.setContent(&file)) {
		
			QDomElement root = doc.firstChildElement("functions");
			QDomElement function = root.firstChildElement("function");
			for (; !function.isNull(); function = function.nextSiblingElement("function")) {
			
			
				const QString function_name = function.attribute("name");
				
				FunctionInfo info;
						
				QDomElement argument = function.firstChildElement("argument");
				for (; !argument.isNull(); argument = argument.nextSiblingElement("argument")) {
					const QString argument_type = argument.attribute("type");
					info.params_.push_back(argument_type[0]);
				}
				
				g_FunctionDB[function_name] = info;	
			}
		}
	}
}

}
}

//------------------------------------------------------------------------------
// Name: cpu_selected_address()
// Desc:
//------------------------------------------------------------------------------
yad64::address_t yad64::v1::cpu_selected_address() {
	return ui()->ui->cpuView->selectedAddress();
}

//------------------------------------------------------------------------------
// Name: current_cpu_view_region()
// Desc:
//------------------------------------------------------------------------------
MemoryRegion yad64::v1::current_cpu_view_region() {
	return ui()->ui->cpuView->region();
}

//------------------------------------------------------------------------------
// Name: repaint_cpu_view()
// Desc:
//------------------------------------------------------------------------------
void yad64::v1::repaint_cpu_view() {
	DebuggerMain *const gui = ui();
	Q_CHECK_PTR(gui);
	gui->ui->cpuView->viewport()->repaint();
}

//------------------------------------------------------------------------------
// Name: symbol_manager()
// Desc:
//------------------------------------------------------------------------------
ISymbolManager &yad64::v1::symbol_manager() {
	static SymbolManager g_SymbolManager;
	return g_SymbolManager;
}

//------------------------------------------------------------------------------
// Name: memory_regions()
// Desc:
//------------------------------------------------------------------------------
MemoryRegions &yad64::v1::memory_regions() {
	static MemoryRegions g_MemoryRegions;
	return g_MemoryRegions;
}

//------------------------------------------------------------------------------
// Name: arch_processor()
// Desc:
//------------------------------------------------------------------------------
IArchProcessor &yad64::v1::arch_processor() {
	static ArchProcessor g_ArchProcessor;
	return g_ArchProcessor;
}

//------------------------------------------------------------------------------
// Name: set_analyzer(IAnalyzer *p)
// Desc:
//------------------------------------------------------------------------------
IAnalyzer *yad64::v1::set_analyzer(IAnalyzer *p) {
	Q_CHECK_PTR(p);
	return g_Analyzer.fetchAndStoreAcquire(p);
}

//------------------------------------------------------------------------------
// Name: analyzer()
// Desc:
//------------------------------------------------------------------------------
IAnalyzer *yad64::v1::analyzer() {
	return g_Analyzer;
}

//------------------------------------------------------------------------------
// Name: set_session_file_handler(ISessionFile *p)
// Desc:
//------------------------------------------------------------------------------
ISessionFile *yad64::v1::set_session_file_handler(ISessionFile *p) {
	Q_CHECK_PTR(p);
	return g_SessionHandler.fetchAndStoreAcquire(p);
}

//------------------------------------------------------------------------------
// Name: session_file_handler()
// Desc:
//------------------------------------------------------------------------------
ISessionFile *yad64::v1::session_file_handler() {
	return g_SessionHandler;;
}

//------------------------------------------------------------------------------
// Name: set_debug_event_handler(IDebugEventHandler *p)
// Desc:
//------------------------------------------------------------------------------
IDebugEventHandler *yad64::v1::set_debug_event_handler(IDebugEventHandler *p) {
	Q_CHECK_PTR(p);
	return g_DebugEventHandler.fetchAndStoreAcquire(p);
}

//------------------------------------------------------------------------------
// Name: debug_event_handler();
// Desc:
//------------------------------------------------------------------------------
IDebugEventHandler *yad64::v1::debug_event_handler() {
	return g_DebugEventHandler;
}

//------------------------------------------------------------------------------
// Name: jump_to_address(yad64::address_t address)
// Desc: sets the disassembly display to a given address, returning success
//       status
//------------------------------------------------------------------------------
bool yad64::v1::jump_to_address(yad64::address_t address) {
	DebuggerMain *const gui = ui();
	Q_CHECK_PTR(gui);
	return gui->jump_to_address(address);
}

//------------------------------------------------------------------------------
// Name: dump_data_range(yad64::address_t address, yad64::address_t end_address, bool newTab)
// Desc: shows a given address through a given end address in the data view,
//       optionally in a new tab
//------------------------------------------------------------------------------
bool yad64::v1::dump_data_range(yad64::address_t address, yad64::address_t end_address, bool new_tab) {
	DebuggerMain *const gui = ui();
	Q_CHECK_PTR(gui);
	return gui->dump_data_range(address, end_address, new_tab);
}

//------------------------------------------------------------------------------
// Name: dump_data_range(yad64::address_t address, yad64::address_t end_address)
// Desc: shows a given address through a given end address in the data view
//------------------------------------------------------------------------------
bool yad64::v1::dump_data_range(yad64::address_t address, yad64::address_t end_address) {
	return dump_data_range(address, end_address, false);
}

//------------------------------------------------------------------------------
// Name: dump_stack(yad64::address_t address)
// Desc:
//------------------------------------------------------------------------------
bool yad64::v1::dump_stack(yad64::address_t address) {
	return dump_stack(address, true);
}

//------------------------------------------------------------------------------
// Name: dump_stack(yad64::address_t address, bool scroll_to)
// Desc: shows a given address in the stack view
//------------------------------------------------------------------------------
bool yad64::v1::dump_stack(yad64::address_t address, bool scroll_to) {
	DebuggerMain *const gui = ui();
	Q_CHECK_PTR(gui);
	return gui->dump_stack(address, scroll_to);
}

//------------------------------------------------------------------------------
// Name: dump_data(yad64::address_t address, bool new_tab)
// Desc: shows a given address in the data view, optionally in a new tab
//------------------------------------------------------------------------------
bool yad64::v1::dump_data(yad64::address_t address, bool new_tab) {
	DebuggerMain *const gui = ui();
	Q_CHECK_PTR(gui);
	return gui->dump_data(address, new_tab);
}

//------------------------------------------------------------------------------
// Name: dump_data(yad64::address_t address)
// Desc: shows a given address in the data view
//------------------------------------------------------------------------------
bool yad64::v1::dump_data(yad64::address_t address) {
	return dump_data(address, false);
}

//------------------------------------------------------------------------------
// Name: set_breakpoint_condition(yad64::address_t address, const QString &condition)
// Desc:
//------------------------------------------------------------------------------
void yad64::v1::set_breakpoint_condition(yad64::address_t address, const QString &condition) {
	IBreakpoint::pointer bp = find_breakpoint(address);
	if(bp) {
		bp->condition = condition;
	}
}

//------------------------------------------------------------------------------
// Name: get_breakpoint_condition(yad64::address_t address)
// Desc:
//------------------------------------------------------------------------------
QString yad64::v1::get_breakpoint_condition(yad64::address_t address) {
	QString ret;
	IBreakpoint::pointer bp = find_breakpoint(address);
	if(bp) {
		ret = bp->condition;
	}

	return ret;
}


//------------------------------------------------------------------------------
// Name: create_breakpoint(yad64::address_t address)
// Desc: adds a breakpoint at a given address
//------------------------------------------------------------------------------
void yad64::v1::create_breakpoint(yad64::address_t address) {

	MemoryRegion region;
	memory_regions().sync();
	if(memory_regions().find_region(address, region)) {
		int ret = QMessageBox::Yes;

		if(!region.executable() && config().warn_on_no_exec_bp) {
			ret = QMessageBox::question(
				0,
				QT_TRANSLATE_NOOP("yad64", "Suspicious breakpoint"),
				QT_TRANSLATE_NOOP("yad64",
					"You want to place a breakpoint in a non-executable region.\n"
					"An INT3 breakpoint set on data will not execute and may cause incorrect results or crashes.\n"
					"Do you really want to set a breakpoint here?"),
				QMessageBox::Yes, QMessageBox::No);
		} else {
			quint8 buffer[yad64::Instruction::MAX_SIZE + 1];
			int size = sizeof(buffer);

			if(yad64::v1::get_instruction_bytes(address, buffer, size)) {
				yad64::Instruction insn(buffer, buffer + size, address, std::nothrow);
				if(!insn.valid()) {
					ret = QMessageBox::question(
						0,
						QT_TRANSLATE_NOOP("yad64", "Suspicious breakpoint"),
						QT_TRANSLATE_NOOP("yad64",
							"It looks like you may be setting an INT3 breakpoint on data.\n"
							"An INT3 breakpoint set on data will not execute and may cause incorrect results or crashes.\n"
							"Do you really want to set a breakpoint here?"),
						QMessageBox::Yes, QMessageBox::No);
				}
			}
		}

		if(ret == QMessageBox::Yes) {
			debugger_core->add_breakpoint(address);
			repaint_cpu_view();
		}


	} else {
		QMessageBox::information(
			0,
			QT_TRANSLATE_NOOP("yad64", "Error Setting Breakpoint"),
			QT_TRANSLATE_NOOP("yad64", "Sorry, but setting a breakpoint which is not in a valid region is not allowed."));
	}
}

//------------------------------------------------------------------------------
// Name: enable_breakpoint(yad64::address_t address)
// Desc:
//------------------------------------------------------------------------------
yad64::address_t yad64::v1::enable_breakpoint(yad64::address_t address) {
	if(address != 0) {
		IBreakpoint::pointer bp = find_breakpoint(address);
		if(bp && bp->enable()) {
			return address;
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: disable_breakpoint(yad64::address_t address)
// Desc:
//------------------------------------------------------------------------------
yad64::address_t yad64::v1::disable_breakpoint(yad64::address_t address) {
	if(address != 0) {
		IBreakpoint::pointer bp = find_breakpoint(address);
		if(bp && bp->disable()) {
			return address;
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: toggle_breakpoint(yad64::address_t address)
// Desc: toggles the existence of a breakpoint at a given address
//------------------------------------------------------------------------------
void yad64::v1::toggle_breakpoint(yad64::address_t address) {
	if(find_breakpoint(address)) {
		remove_breakpoint(address);
	} else {
		create_breakpoint(address);
	}
}

//------------------------------------------------------------------------------
// Name: remove_breakpoint(yad64::address_t address)
// Desc: removes a breakpoint
//------------------------------------------------------------------------------
void yad64::v1::remove_breakpoint(yad64::address_t address) {
	debugger_core->remove_breakpoint(address);
	repaint_cpu_view();
}

//------------------------------------------------------------------------------
// Name: eval_expression(const QString &expression, yad64::address_t &value
// Desc:
//------------------------------------------------------------------------------
bool yad64::v1::eval_expression(const QString &expression, yad64::address_t &value) {
	Expression<yad64::address_t> expr(expression, get_variable, get_value);
	ExpressionError err;

	bool ok;
	const yad64::address_t address = expr.evaluate_expression(ok, err);
	if(ok) {
		value = address;
		return true;
	} else {
		QMessageBox::information(debugger_ui, QT_TRANSLATE_NOOP("yad64", "Error In Expression!"), err.what());
		return false;
	}
}

//------------------------------------------------------------------------------
// Name: get_expression_from_user(const QString &title, const QString prompt, yad64::address_t &value)
// Desc:
//------------------------------------------------------------------------------
bool yad64::v1::get_expression_from_user(const QString &title, const QString prompt, yad64::address_t &value) {
	bool ok;
    const QString text = QInputDialog::getText(debugger_ui, title, prompt, QLineEdit::Normal, QString(), &ok);

	if(ok && !text.isEmpty()) {
		return eval_expression(text, value);
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: get_value_from_user(yad64::reg_t &value)
// Desc:
//------------------------------------------------------------------------------
bool yad64::v1::get_value_from_user(yad64::reg_t &value) {
	return get_value_from_user(value, QT_TRANSLATE_NOOP("yad64", "Input Value"));
}

//------------------------------------------------------------------------------
// Name: get_value_from_user(yad64::reg_t &value, const QString &title)
// Desc:
//------------------------------------------------------------------------------
bool yad64::v1::get_value_from_user(yad64::reg_t &value, const QString &title) {
	static DialogInputValue *const dlg = new DialogInputValue(debugger_ui);
	bool ret = false;

	dlg->setWindowTitle(title);
	dlg->set_value(value);
	if(dlg->exec() == QDialog::Accepted) {
		value = dlg->value();
		ret = true;
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: get_binary_string_from_user(QByteArray &value, const QString &title)
// Desc:
//------------------------------------------------------------------------------
bool yad64::v1::get_binary_string_from_user(QByteArray &value, const QString &title) {
	return get_binary_string_from_user(value, title, 10);
}

//------------------------------------------------------------------------------
// Name: get_binary_string_from_user(QByteArray &value, const QString &title, int max_length)
// Desc:
//------------------------------------------------------------------------------
bool yad64::v1::get_binary_string_from_user(QByteArray &value, const QString &title, int max_length) {
	static DialogInputBinaryString *const dlg = new DialogInputBinaryString(debugger_ui);

	bool ret = false;

	dlg->setWindowTitle(title);

	BinaryString *const bs = dlg->binary_string();

	// set the max length BEFORE the value! (or else we truncate incorrectly)
	if(value.length() <= max_length) {
		bs->setMaxLength(max_length);
	}

	bs->setValue(value);

	if(dlg->exec() == QDialog::Accepted) {
		value = bs->value();
		ret = true;
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: dialog_options()
// Desc: returns a pointer to the options dialog
//------------------------------------------------------------------------------
QPointer<QDialog> yad64::v1::dialog_options() {
	static QPointer<QDialog> dialog = new DialogOptions(debugger_ui);
	return dialog;
}

//------------------------------------------------------------------------------
// Name: config()
// Desc:
//------------------------------------------------------------------------------
Configuration &yad64::v1::config() {
	static Configuration g_Configuration;
	return g_Configuration;
}

//------------------------------------------------------------------------------
// Name: get_ascii_string_at_address(yad64::address_t address, QString &s, int min_length, int max_length, int &found_length)
// Desc: attempts to get a string at a given address whose length is >= min_length
//       and < max_length
// Note: strings are comprised of printable characters and whitespace.
// Note: found_length is needed because we replace characters which need an
//       escape char with the escape sequence (thus the resultant string may be
//       longer than the original). found_length is the original length.
//------------------------------------------------------------------------------
bool yad64::v1::get_ascii_string_at_address(yad64::address_t address, QString &s, int min_length, int max_length, int &found_length) {

	bool is_string = false;

	if(debugger_core) {
		s.clear();

		if(min_length <= max_length) {
			while(max_length--) {
				char ch;
				if(!debugger_core->read_bytes(address++, &ch, sizeof(ch))) {
					break;
				}

				const int ascii_char = static_cast<unsigned char>(ch);
				if(ascii_char < 0x80 && (std::isprint(ascii_char) || std::isspace(ascii_char))) {
					s += ch;
				} else {
					break;
				}
			}
		}

		is_string = s.length() >= min_length;

		if(is_string) {
			found_length = s.length();
			s.replace("\r", "\\r");
			s.replace("\n", "\\n");
			s.replace("\t", "\\t");
			s.replace("\v", "\\v");
			s.replace("\"", "\\\"");
		}
	}

	return is_string;
}

//------------------------------------------------------------------------------
// Name: get_utf16_string_at_address(yad64::address_t address, QString &s, int min_length, int max_length, int &found_length)
// Desc: attempts to get a string at a given address whose length os >= min_length
//       and < max_length
// Note: strings are comprised of printable characters and whitespace.
// Note: found_length is needed because we replace characters which need an
//       escape char with the escape sequence (thus the resultant string may be
//       longer than the original). found_length is the original length.
//------------------------------------------------------------------------------
bool yad64::v1::get_utf16_string_at_address(yad64::address_t address, QString &s, int min_length, int max_length, int &found_length) {
	bool is_string = false;
	if(debugger_core) {
		s.clear();

		if(min_length <= max_length) {
			while(max_length--) {

				quint16 val;
				if(!debugger_core->read_bytes(address, &val, sizeof(val))) {
					break;
				}

				address += sizeof(val);

				QChar ch(val);

				// for now, we only acknowledge ASCII chars encoded as unicode
				const int ascii_char = ch.toAscii();
				if(ascii_char >= 0x20 && ascii_char < 0x80) {
					s += ch;
				} else {
					break;
				}
			}
		}

		is_string = s.length() >= min_length;

		if(is_string) {
			found_length = s.length();
			s.replace("\r", "\\r");
			s.replace("\n", "\\n");
			s.replace("\t", "\\t");
			s.replace("\v", "\\v");
			s.replace("\"", "\\\"");
		}
	}
	return is_string;
}

//------------------------------------------------------------------------------
// Name: find_function_symbol(yad64::address_t address, const QString &default_value, int *offset)
// Desc:
//------------------------------------------------------------------------------
QString yad64::v1::find_function_symbol(yad64::address_t address, const QString &default_value, int *offset) {
	QString symname(default_value);
	int off;

	if(function_symbol_base(address, symname, off)) {
		symname = QString("%1+%2").arg(symname).arg(off, 0, 16);
		if(offset) {
			*offset = off;
		}
	}

	return symname;
}

//------------------------------------------------------------------------------
// Name: find_function_symbol(yad64::address_t address, const QString &default_value)
// Desc:
//------------------------------------------------------------------------------
QString yad64::v1::find_function_symbol(yad64::address_t address, const QString &default_value) {
	return find_function_symbol(address, default_value, 0);
}

//------------------------------------------------------------------------------
// Name: find_function_symbol(yad64::address_t address)
// Desc:
//------------------------------------------------------------------------------
QString yad64::v1::find_function_symbol(yad64::address_t address) {
	return find_function_symbol(address, QString(), 0);
}

//------------------------------------------------------------------------------
// Name: get_variable(QString &s, bool &ok, ExpressionError &err)
// Desc:
//------------------------------------------------------------------------------
yad64::address_t yad64::v1::get_variable(const QString &s, bool &ok, ExpressionError &err) {

	Q_CHECK_PTR(debugger_core);


	State state;
	yad64::v1::debugger_core->get_state(state);
	const Register reg = state.value(s);
	ok = reg;
	if(!ok) {
		err = ExpressionError(ExpressionError::UNKNOWN_VARIABLE);
	}

	if(reg.name() == "fs") {
		return *state["fs_base"];
	} else if(reg.name() == "gs") {
		return *state["gs_base"];
	}

	return *reg;
}

//------------------------------------------------------------------------------
// Name: get_value(yad64::address_t address, bool &ok, ExpressionError &err)
// Desc:
//------------------------------------------------------------------------------
yad64::address_t yad64::v1::get_value(yad64::address_t address, bool &ok, ExpressionError &err) {

	Q_CHECK_PTR(debugger_core);

	yad64::address_t ret = 0;

	ok = debugger_core->read_bytes(address, &ret, sizeof(ret));

	if(!ok) {
		err = ExpressionError(ExpressionError::CANNOT_READ_MEMORY);
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: get_instruction_bytes(yad64::address_t address, quint8 *buf, int &size)
// Desc: attempts to read at most size bytes, but will retry using smaller sizes as needed
//------------------------------------------------------------------------------
bool yad64::v1::get_instruction_bytes(yad64::address_t address, quint8 *buf, int &size) {

	Q_CHECK_PTR(debugger_core);
	Q_ASSERT(size >= 0);

	bool ok = debugger_core->read_bytes(address, buf, size);

	while(!ok && size) {
		ok = debugger_core->read_bytes(address, buf, --size);
	}

	return ok;
}

//------------------------------------------------------------------------------
// Name: get_binary_info(const MemoryRegion &region)
// Desc: gets an object which knows how to analyze the binary file provided
//       or NULL if none-found
//------------------------------------------------------------------------------
IBinary *yad64::v1::get_binary_info(const MemoryRegion &region) {
	Q_FOREACH(IBinary::create_func_ptr_t f, g_BinaryInfoList) {
		IBinary *const p = (*f)(region);

		if(p->validate_header()) {
			return p;
		}

		delete p;
	}

	return 0;
}

//------------------------------------------------------------------------------
// Name: locate_main_function()
// Desc:
// Note: this currently only works for glibc linked elf files
//------------------------------------------------------------------------------
yad64::address_t yad64::v1::locate_main_function() {

	const MemoryRegion region = primary_code_region();

	SCOPED_POINTER<IBinary> binfo(get_binary_info(region));
	
	if(binfo) {
		const yad64::address_t main_func = binfo->calculate_main();
		if(main_func != 0) {
			return main_func;
		} else {
			return binfo->entry_point();
		}
	}
	
	return 0;
}

//------------------------------------------------------------------------------
// Name: plugin_list()
// Desc:
//------------------------------------------------------------------------------
const QHash<QString, QObject *> &yad64::v1::plugin_list() {
	return g_GeneralPlugins;
}

//------------------------------------------------------------------------------
// Name: find_plugin_by_name(const QString &name)
// Desc: gets a pointer to a plugin based on it's classname
//------------------------------------------------------------------------------
IPlugin *yad64::v1::find_plugin_by_name(const QString &name) {
	Q_FOREACH(QObject *p, g_GeneralPlugins) {
		if(name == p->metaObject()->className()) {
			return qobject_cast<IPlugin *>(p);
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: reload_symbols()
// Desc:
//------------------------------------------------------------------------------
void yad64::v1::reload_symbols() {
	symbol_manager().load_symbols(yad64::v1::config().symbol_path);
}

//------------------------------------------------------------------------------
// Name: get_function_info(const QString &function)
// Desc:
//------------------------------------------------------------------------------
const FunctionInfo *yad64::v1::get_function_info(const QString &function) {

	QHash<QString, FunctionInfo>::const_iterator it = g_FunctionDB.find(function);
	if(it != g_FunctionDB.end()) {
		return &(it.value());	
	}

	return 0;
}

//------------------------------------------------------------------------------
// Name: primary_data_region()
// Desc: returns the main .data section of the main executable module
// Note: make sure that memory regions has been sync'd first or you will like
//       get a null-region result
//------------------------------------------------------------------------------
MemoryRegion yad64::v1::primary_data_region() {
	const QList<MemoryRegion> &l = yad64::v1::memory_regions().regions();

	if(l.size() >= 2) {
		return l[1];
	} else {
		return MemoryRegion();
	}
}

//------------------------------------------------------------------------------
// Name: pop_value(State &state)
// Desc:
//------------------------------------------------------------------------------
void yad64::v1::pop_value(State &state) {
	state.adjust_stack(sizeof(yad64::reg_t));
}

//------------------------------------------------------------------------------
// Name: push_value(State &state, yad64::reg_t value)
// Desc:
//------------------------------------------------------------------------------
void yad64::v1::push_value(State &state, yad64::reg_t value) {
	state.adjust_stack(- static_cast<int>(sizeof(yad64::reg_t)));
	yad64::v1::debugger_core->write_bytes(state.stack_pointer(), &value, sizeof(yad64::reg_t));
}

//------------------------------------------------------------------------------
// Name: register_binary_info(createFuncPtr fptr)
// Desc:
//------------------------------------------------------------------------------
void yad64::v1::register_binary_info(IBinary::create_func_ptr_t fptr) {
    if(!g_BinaryInfoList.contains(fptr)) {
		g_BinaryInfoList.push_back(fptr);
    }
}

//------------------------------------------------------------------------------
// Name: yad64_version()
// Desc: returns an integer comparable version of our current version string
//------------------------------------------------------------------------------
quint32 yad64::v1::yad64_version() {
	return int_version(version);
}

//------------------------------------------------------------------------------
// Name: overwrite_check(yad64::address_t address, unsigned int size)
// Desc:
//------------------------------------------------------------------------------
bool yad64::v1::overwrite_check(yad64::address_t address, unsigned int size) {
	bool firstConflict = true;
	for(yad64::address_t addr = address; addr != (address + size); ++addr) {
		IBreakpoint::pointer bp = find_breakpoint(addr);

		if(bp && bp->enabled()) {
			if(firstConflict) {
				const int ret = QMessageBox::question(
						0,
						QT_TRANSLATE_NOOP("yad64", "Overwritting breakpoint"),
						QT_TRANSLATE_NOOP("yad64", "You are attempting to modify bytes which overlap with a software breakpoint. Doing this will implicitly remove any breakpoints which are a conflict. Are you sure you want to do this?"),
						QMessageBox::Yes,
						QMessageBox::No);

				if(ret == QMessageBox::No) {
					return false;
				}
				firstConflict = false;
			}

			remove_breakpoint(addr);
		}
	}
	return true;
}

//------------------------------------------------------------------------------
// Name: modify_bytes(yad64::address_t address, unsigned int size, QByteArray &bytes, quint8 fill
// Desc:
//------------------------------------------------------------------------------
void yad64::v1::modify_bytes(yad64::address_t address, unsigned int size, QByteArray &bytes, quint8 fill) {

	if(size != 0) {
		if(get_binary_string_from_user(bytes, QT_TRANSLATE_NOOP("yad64", "Edit Binary String"), size)) {
			if(overwrite_check(address, size)) {

				// fill bytes
				while(bytes.size() < static_cast<int>(size)) {
					bytes.push_back(fill);
				}

				debugger_core->write_bytes(address, bytes.data(), size);

				// do a refresh, not full update
				DebuggerMain *const gui = ui();
				Q_CHECK_PTR(gui);
				gui->refresh_gui();
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: get_md5(const void *p, size_t n)
// Desc:
//------------------------------------------------------------------------------
QByteArray yad64::v1::get_md5(const void *p, size_t n) {
	MD5 md5(p, n);

	// make a deep copy because MD5 is about to go out of scope
	return QByteArray(reinterpret_cast<const char *>(md5.digest()), 16);
}

//------------------------------------------------------------------------------
// Name: get_file_md5(const QString &s)
// Desc: returns a byte array representing the MD5 of a file
//------------------------------------------------------------------------------
QByteArray yad64::v1::get_file_md5(const QString &s) {

	QFile file(s);
	file.open(QIODevice::ReadOnly);
	if(file.isOpen()) {
		const QByteArray file_bytes = file.readAll();
		return get_md5(file_bytes.data(), file_bytes.size());
	}

	return QByteArray();
}


//------------------------------------------------------------------------------
// Name: basename(const QString &s)
// Desc:
//------------------------------------------------------------------------------
QString yad64::v1::basename(const QString &s) {

	const QFileInfo fileInfo(s);
	QString ret = fileInfo.baseName();

	if(!fileInfo.completeSuffix().isEmpty()) {
		ret += '.';
		ret += fileInfo.completeSuffix();
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: symlink_target(const QString &s)
// Desc:
//------------------------------------------------------------------------------
QString yad64::v1::symlink_target(const QString &s) {
	return QFileInfo(s).symLinkTarget();
}

//------------------------------------------------------------------------------
// Name: int_version()
// Desc: returns an integer comparable version of a version string in x.y.z
//       format, or 0 if error
//------------------------------------------------------------------------------
quint32 yad64::v1::int_version(const QString &s) {

	ulong ret = 0;
	const QStringList list = s.split(".");
	if(list.size() == 3) {
		bool ok[3];
		const unsigned int maj = list[0].toUInt(&ok[0]);
		const unsigned int min = list[1].toUInt(&ok[1]);
		const unsigned int rev = list[2].toUInt(&ok[2]);
		if(ok[0] && ok[1] && ok[2]) {
			ret = (maj << 12) | (min << 8) | (rev);
		}
	}
	return ret;
}

//------------------------------------------------------------------------------
// Name: parse_command_line(const QString &cmdline)
// Desc:
//------------------------------------------------------------------------------
QStringList yad64::v1::parse_command_line(const QString &cmdline) {

	QStringList args;
	QString arg;

	int bcount = 0;
	bool in_quotes = false;

	QString::const_iterator s = cmdline.begin();

	while(s != cmdline.end()) {
		if(!in_quotes && s->isSpace()) {

			// Close the argument and copy it
			args << arg;
			arg.clear();

			// skip the remaining spaces
			do {
				++s;
			} while(s->isSpace());

			// Start with a new argument
			bcount = 0;
		} else if(*s == '\\') {

			// '\\'
			arg += *s++;
			++bcount;

		} else if(*s == '"') {

			// '"'
			if((bcount & 1) == 0) {
				/* Preceded by an even number of '\', this is half that
				 * number of '\', plus a quote which we erase.
				 */

				arg.chop(bcount / 2);
				in_quotes = !in_quotes;
			} else {
				/* Preceded by an odd number of '\', this is half that
				 * number of '\' followed by a '"'
				 */

				arg.chop(bcount / 2 + 1);
				arg += '"';
			}

			++s;
			bcount = 0;
		} else {
			arg += *s++;
			bcount = 0;
		}
	}

	if(!arg.isEmpty()) {
		args << arg;
	}

	return args;
}

//------------------------------------------------------------------------------
// Name: string_to_address(const QString &s, bool &ok)
// Desc:
//------------------------------------------------------------------------------
yad64::address_t yad64::v1::string_to_address(const QString &s, bool &ok) {
#if defined(YAD64_X86)
	return s.left(8).toULongLong(&ok, 16);
#elif defined(YAD64_X86_64)
	return s.left(16).toULongLong(&ok, 16);
#endif
}

//------------------------------------------------------------------------------
// Name: format_bytes(const QByteArray &x)
// Desc:
//------------------------------------------------------------------------------
QString yad64::v1::format_bytes(const QByteArray &x) {
	QString bytes;

	if(x.size() != 0) {
		QString temp;
		temp.reserve(4);
		
		bytes.reserve(x.size() * 4);
		
		QByteArray::const_iterator it = x.begin();
		bytes += temp.sprintf("%02x", *it++ & 0xff);
		while(it != x.end()) {
			bytes += temp.sprintf(" %02x", *it++ & 0xff);
		}
	}

	return bytes;
}

//------------------------------------------------------------------------------
// Name: format_pointer(yad64::address_t p)
// Desc:
//------------------------------------------------------------------------------
QString yad64::v1::format_pointer(yad64::address_t p) {
	return QString("%1").arg(p, YAD64_MAX_HEX, 16, QChar('0'));
}

//------------------------------------------------------------------------------
// Name: current_data_view_address()
// Desc:
//------------------------------------------------------------------------------
yad64::address_t yad64::v1::current_data_view_address() {
	return qobject_cast<QHexView *>(ui()->ui->tabWidget->currentWidget())->firstVisibleAddress();
}

//------------------------------------------------------------------------------
// Name: set_status(const QString &message
// Desc:
//------------------------------------------------------------------------------
void yad64::v1::set_status(const QString &message) {
	ui()->ui->statusbar->showMessage(message, 0);
}

//------------------------------------------------------------------------------
// Name: find_breakpoint(yad64::address_t address)
// Desc:
//------------------------------------------------------------------------------
IBreakpoint::pointer yad64::v1::find_breakpoint(yad64::address_t address) {
	if(yad64::v1::debugger_core) {
		return debugger_core->find_breakpoint(address);
	}
	return IBreakpoint::pointer();
}

//------------------------------------------------------------------------------
// Name: pointer_size()
// Desc:
//------------------------------------------------------------------------------
int yad64::v1::pointer_size() {
	
	if(yad64::v1::debugger_core) {
		return yad64::v1::debugger_core->pointer_size();
	}
	
	// default to sizeof the native pointer for sanity!
	return sizeof(void*);
}

//------------------------------------------------------------------------------
// Name: pointer_size()
// Desc:
//------------------------------------------------------------------------------
QWidget *yad64::v1::disassembly_widget() {
	return ui()->ui->cpuView;
}

//------------------------------------------------------------------------------
// Name: serialize_object(const QObject *object)
// Desc:
//------------------------------------------------------------------------------
QByteArray yad64::v1::serialize_object(const QObject *object) {
	QVariantMap variant = QJson::QObjectHelper::qobject2qvariant(object);
	QJson::Serializer serializer;
	return serializer.serialize(variant);
}
