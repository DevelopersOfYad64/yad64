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

#ifndef PLATFORMSTATE_20110330_H_
#define PLATFORMSTATE_20110330_H_

#include "IState.h"
#include "Types.h"
#include <sys/user.h>

class PlatformState : public IState {
	friend class DebuggerCore;

public:
	PlatformState();

public:
	virtual IState *clone() const;

public:
	virtual QString flags_to_string() const;
	virtual QString flags_to_string(yad64::reg_t flags) const;
	virtual Register value(const QString &reg) const;
	virtual yad64::address_t frame_pointer() const;
	virtual yad64::address_t instruction_pointer() const;
	virtual yad64::address_t stack_pointer() const;
	virtual yad64::reg_t debug_register(int n) const;
	virtual yad64::reg_t flags() const;
	virtual long double fpu_register(int n) const;
	virtual void adjust_stack(int bytes);
	virtual void clear();
	virtual void set_debug_register(int n, yad64::reg_t value);
	virtual void set_flags(yad64::reg_t flags);
	virtual void set_instruction_pointer(yad64::address_t value);
	virtual void set_register(const QString &name, yad64::reg_t value);
	virtual quint64 mmx_register(int n) const;
	virtual QByteArray xmm_register(int n) const;

private:
	struct user_regs_struct   regs_;
	struct user_fpregs_struct fpregs_;
	yad64::reg_t                dr_[8];
#if defined(YAD64_X86)
	yad64::address_t            fs_base;
	yad64::address_t            gs_base;
#endif
};

#endif

