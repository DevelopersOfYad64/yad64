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

#include "DialogReferences.h"
#include "IDebuggerCore.h"
#include "Debugger.h"
#include "Util.h"
#include "MemoryRegions.h"
#include "Debugger.h"

#include <QVector>
#include <QMessageBox>

#include "ui_dialogreferences.h"

//------------------------------------------------------------------------------
// Name: DialogReferences(QWidget *parent)
// Desc: constructor
//------------------------------------------------------------------------------
DialogReferences::DialogReferences(QWidget *parent) : QDialog(parent), ui(new Ui::DialogReferences) {
	ui->setupUi(this);
	connect(this, SIGNAL(updateProgress(int)), ui->progressBar, SLOT(setValue(int)));
}

//------------------------------------------------------------------------------
// Name: ~DialogReferences()
// Desc:
//------------------------------------------------------------------------------
DialogReferences::~DialogReferences() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: showEvent(QShowEvent *)
// Desc:
//------------------------------------------------------------------------------
void DialogReferences::showEvent(QShowEvent *) {
	ui->listWidget->clear();
	ui->progressBar->setValue(0);
}

//------------------------------------------------------------------------------
// Name: do_find()
// Desc:
//------------------------------------------------------------------------------
void DialogReferences::do_find() {
	bool ok;
	const yad64::address_t address   = yad64::v1::string_to_address(ui->txtAddress->text(), ok);
	const yad64::address_t page_size = yad64::v1::debugger_core->page_size();

	if(ok) {
		yad64::v1::memory_regions().sync();
		const QList<MemoryRegion> regions = yad64::v1::memory_regions().regions();

		int i = 0;
		Q_FOREACH(const MemoryRegion &region, regions) {
			// a short circut for speading things up
			if(region.accessible() || !ui->chkSkipNoAccess->isChecked()) {

				const yad64::address_t size_in_pages = region.size() / page_size;

				try {

					QVector<quint8> pages(size_in_pages * page_size);
					const quint8 *const pages_end = &pages[0] + region.size();

					if(yad64::v1::debugger_core->read_pages(region.start(), &pages[0], size_in_pages)) {
						const quint8 *p = &pages[0];
						while(p != pages_end) {

							if(static_cast<std::size_t>(pages_end - p) < sizeof(yad64::address_t)) {
								break;
							}

							const yad64::address_t addr = p - &pages[0] + region.start();

							yad64::address_t test_address;
							memcpy(&test_address, p, sizeof(yad64::address_t));

							if(test_address == address) {

								QListWidgetItem *const item = new QListWidgetItem(yad64::v1::format_pointer(addr));
								item->setData(Qt::UserRole, 'D');
								ui->listWidget->addItem(item);
							}

							yad64::Instruction insn(p, pages_end, addr, std::nothrow);
							if(insn.valid()) {
								switch(insn.type()) {
								case yad64::Instruction::OP_JMP:
								case yad64::Instruction::OP_CALL:
								case yad64::Instruction::OP_JCC:
									if(insn.operand(0).general_type() == yad64::Operand::TYPE_REL) {
										if(insn.operand(0).relative_target() == address) {
											QListWidgetItem *const item = new QListWidgetItem(yad64::v1::format_pointer(addr));
											item->setData(Qt::UserRole, 'C');
											ui->listWidget->addItem(item);
										}
									}
									break;
								default:
									break;
								}
							}

							emit updateProgress(util::percentage(i, regions.size(), p - &pages[0], region.size()));
							++p;
						}
					}
				} catch(const std::bad_alloc &) {
					QMessageBox::information(
						0,
						tr("Memroy Allocation Error"),
						tr("Unable to satisfy memory allocation request for requested region."));
				}
			} else {
				emit updateProgress(util::percentage(i, regions.size()));
			}
			++i;
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_btnFind_clicked()
// Desc: find button event handler
//------------------------------------------------------------------------------
void DialogReferences::on_btnFind_clicked() {
	ui->btnFind->setEnabled(false);
	ui->progressBar->setValue(0);
	ui->listWidget->clear();
	do_find();
	ui->progressBar->setValue(100);
	ui->btnFind->setEnabled(true);
}

//------------------------------------------------------------------------------
// Name: on_listWidget_itemDoubleClicked(QListWidgetItem *item)
// Desc: follows the foudn item in the data view
//------------------------------------------------------------------------------
void DialogReferences::on_listWidget_itemDoubleClicked(QListWidgetItem *item) {
	bool ok;
	const yad64::address_t addr = yad64::v1::string_to_address(item->text(), ok);
	if(ok) {
		if(item->data(Qt::UserRole).toChar() == 'D') {
			yad64::v1::dump_data(addr, false);
		} else {
			yad64::v1::jump_to_address(addr);
		}
	}
}
