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

#ifndef IANALYZER_20080630_H_
#define IANALYZER_20080630_H_

#include <QMap>
#include <QSet>
#include "Types.h"

class MemoryRegion;

class IAnalyzer {
public:
	virtual ~IAnalyzer() {}

public:
	struct Function {
		yad64::address_t entry_address;
		yad64::address_t end_address;
		yad64::address_t last_instruction;
		int            reference_count;

		enum Type {
			FUNCTION_STANDARD,
			FUNCTION_THUNK
		} type;
		
		int size() const { return end_address - entry_address; }
	};

	// TODO: can this be a QHash?
	typedef QMap<yad64::address_t, Function> FunctionMap;

public:
	enum AddressCategory {
		ADDRESS_FUNC_UNKNOWN     = 0x00,
		ADDRESS_FUNC_START       = 0x01,
		ADDRESS_FUNC_BODY        = 0x02,
		ADDRESS_FUNC_END         = 0x04
	};

public:
	virtual void analyze(const MemoryRegion &region) = 0;
	virtual FunctionMap functions(const MemoryRegion &region) const = 0;
	virtual AddressCategory category(yad64::address_t address) const = 0;
	virtual void invalidate_analysis(const MemoryRegion &region) = 0;
	virtual void invalidate_analysis() = 0;
	virtual QSet<yad64::address_t> specified_functions() const { return QSet<yad64::address_t>(); }
};

#endif
