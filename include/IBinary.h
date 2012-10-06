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

#ifndef IBINARY_20070718_H_
#define IBINARY_20070718_H_

#include "Types.h"
#include "API.h"
#include "MemoryRegion.h"

class EDB_EXPORT IBinary {
public:
	explicit IBinary(const MemoryRegion &region);
	virtual ~IBinary() {}

public:
	virtual bool validate_header() = 0;
	virtual edb::address_t entry_point() = 0;
	virtual edb::address_t calculate_main() = 0;
	virtual bool native() const = 0;
	virtual size_t header_size() const = 0;

public:
	// optional, and platform specific:
	// if there is a structure in the target process which has debug data
	// this should return a pointer to it
	virtual edb::address_t debug_pointer() { return 0; }

protected:
	MemoryRegion region_;

public:
	typedef IBinary *(*create_func_ptr_t)(const MemoryRegion &);
};

#endif

