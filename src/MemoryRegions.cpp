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

#include "MemoryRegions.h"
#include "Debugger.h"
#include "IDebuggerCore.h"
#include "ISymbolManager.h"
#include <QDebug>

//------------------------------------------------------------------------------
// Name: MemoryRegions()
// Desc: constructor
//------------------------------------------------------------------------------
MemoryRegions::MemoryRegions() : QAbstractItemModel(0) {
}

//------------------------------------------------------------------------------
// Name: ~MemoryRegions()
// Desc: destructor
//------------------------------------------------------------------------------
MemoryRegions::~MemoryRegions() {
}

//------------------------------------------------------------------------------
// Name: clear()
// Desc:
//------------------------------------------------------------------------------
void MemoryRegions::clear() {
	regions_.clear();
}

//------------------------------------------------------------------------------
// Name: sync()
// Desc: reads a memory map file line by line
//------------------------------------------------------------------------------
void MemoryRegions::sync() {

	QList<MemoryRegion> regions;
	
	if(yad64::v1::debugger_core) {
		regions = yad64::v1::debugger_core->memory_regions();
		Q_FOREACH(const MemoryRegion &region, regions) {
			// if the region has a name, is mapped starting
			// at the beginning of the file, and is executable, sounds
			// like a module mapping!
			if(!region.name().isEmpty()) {
				if(region.base() == 0) {
					if(region.executable()) {
						yad64::v1::symbol_manager().load_symbol_file(region.name(), region.start());
					}
				}
			}
		}
		
		if(regions.isEmpty()) {
			qDebug() << "[MemoryRegions] warning: empty memory map";
		}
	}


	qSwap(regions_, regions);
	reset();
}

//------------------------------------------------------------------------------
// Name: find_region(yad64::address_t address) const
// Desc:
//------------------------------------------------------------------------------
bool MemoryRegions::find_region(yad64::address_t address) const {
	Q_FOREACH(const MemoryRegion &i, regions_) {
		if(i.contains(address)) {
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: find_region(yad64::address_t address, MemoryRegion &region) const
// Desc:
//------------------------------------------------------------------------------
bool MemoryRegions::find_region(yad64::address_t address, MemoryRegion &region) const {
	Q_FOREACH(const MemoryRegion &i, regions_) {
		if(i.contains(address)) {
			region = i;
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: data(const QModelIndex &index, int role) const
// Desc:
//------------------------------------------------------------------------------
QVariant MemoryRegions::data(const QModelIndex &index, int role) const {

	if(index.isValid() && role == Qt::DisplayRole) {

		const MemoryRegion &region = regions_[index.row()];

		switch(index.column()) {
		case 0: return yad64::v1::format_pointer(region.start());
		case 1: return yad64::v1::format_pointer(region.end());
		case 2: return QString("%1%2%3").arg(region.readable() ? 'r' : '-').arg(region.writable() ? 'w' : '-').arg(region.executable() ? 'x' : '-');
		case 3: return region.name();
		}
	}

	return QVariant();
}

//------------------------------------------------------------------------------
// Name: index(int row, int column, const QModelIndex &parent) const
// Desc:
//------------------------------------------------------------------------------
QModelIndex MemoryRegions::index(int row, int column, const QModelIndex &parent) const {
	Q_UNUSED(parent);

	if(row >= rowCount(parent) || column >= columnCount(parent)) {
		return QModelIndex();
	}

	return createIndex(row, column, const_cast<MemoryRegion *>(&regions_[row]));
}

//------------------------------------------------------------------------------
// Name: parent(const QModelIndex &index) const
// Desc:
//------------------------------------------------------------------------------
QModelIndex MemoryRegions::parent(const QModelIndex &index) const {
	Q_UNUSED(index);
	return QModelIndex();
}

//------------------------------------------------------------------------------
// Name: rowCount(const QModelIndex &parent) const
// Desc:
//------------------------------------------------------------------------------
int MemoryRegions::rowCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return regions_.size();
}

//------------------------------------------------------------------------------
// Name: columnCount(const QModelIndex &parent) const
// Desc:
//------------------------------------------------------------------------------
int MemoryRegions::columnCount(const QModelIndex &parent) const {
	Q_UNUSED(parent);
	return 4;
}

//------------------------------------------------------------------------------
// Name: headerData(int section, Qt::Orientation orientation, int role) const
// Desc:
//------------------------------------------------------------------------------
QVariant MemoryRegions::headerData(int section, Qt::Orientation orientation, int role) const {
	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch(section) {
		case 0:
			return tr("Start Address");
		case 1:
			return tr("End Address");
		case 2:
			return tr("Permissions");
		case 3:
			return tr("Name");
		}
	}

	return QVariant();
}

