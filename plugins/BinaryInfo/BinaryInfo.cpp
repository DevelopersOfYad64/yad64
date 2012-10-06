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

#include "BinaryInfo.h"
#include "Debugger.h"
#if defined(Q_OS_UNIX)
#include "ELF32.h"
#include "ELF64.h"
#endif
#if defined(Q_OS_WIN)
#include "PE.h"
#endif
#include "IBinary.h"

namespace {
#if defined(Q_OS_UNIX)
	IBinary *create_binary_info_elf32(const MemoryRegion &region) {
		return new ELF32(region);
	}

	IBinary *create_binary_info_elf64(const MemoryRegion &region) {
		return new ELF64(region);
	}
#endif

#if defined(Q_OS_WIN)
#if defined(Q_OS_WIN64)
    IBinary *create_binary_info_pe64(const MemoryRegion &region) {
        return new PE64(region);
    }
#else
    IBinary *create_binary_info_pe32(const MemoryRegion &region) {
        return new PE32(region);
    }
#endif
#endif
}

//------------------------------------------------------------------------------
// Name: BinaryInfo()
// Desc:
//------------------------------------------------------------------------------
BinaryInfo::BinaryInfo() {
}

//------------------------------------------------------------------------------
// Name: BinaryInfo()
// Desc:
//------------------------------------------------------------------------------
void BinaryInfo::private_init() {
#if defined(Q_OS_UNIX)
	yad64::v1::register_binary_info(create_binary_info_elf32);
	yad64::v1::register_binary_info(create_binary_info_elf64);
#endif
#if defined(Q_OS_WIN)
#if defined(Q_OS_WIN64)
    yad64::v1::register_binary_info(create_binary_info_pe64);
#else
    yad64::v1::register_binary_info(create_binary_info_pe32);
#endif
#endif
}

//------------------------------------------------------------------------------
// Name: menu(QWidget *parent)
// Desc:
//------------------------------------------------------------------------------
QMenu *BinaryInfo::menu(QWidget *parent) {
	Q_UNUSED(parent);
	return 0;
}

Q_EXPORT_PLUGIN2(BinaryInfo, BinaryInfo)
