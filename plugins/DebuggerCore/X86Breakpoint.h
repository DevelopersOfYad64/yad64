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

#ifndef X86BREAKPOINT_20060720_H_
#define X86BREAKPOINT_20060720_H_

#include "IBreakpoint.h"

class X86Breakpoint : public IBreakpoint {
public:
	X86Breakpoint(yad64::address_t address);
	~X86Breakpoint();

public:
	virtual yad64::address_t address() const    { return address_; }
	virtual unsigned int hit_count() const    { return hit_count_; }
	virtual bool enabled() const              { return enabled_; }
	virtual bool one_time() const             { return one_time_; }
	virtual bool internal() const             { return internal_; }
	virtual QByteArray original_bytes() const { return original_bytes_; }

public:
	virtual bool enable();
	virtual bool disable();
	virtual void hit()                    { hit_count_++; }
	virtual void set_one_time(bool value) { one_time_ = value; }
	virtual void set_internal(bool value) { internal_ = value; }

public:
	static const int size = 1;

private:
	QByteArray     original_bytes_;
	yad64::address_t address_;
	unsigned int   hit_count_;
	bool           enabled_ ;
	bool           one_time_;
	bool           internal_;
};

#endif

