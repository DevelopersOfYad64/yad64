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

#ifndef IDEBUGGER_CORE_20061101_H_
#define IDEBUGGER_CORE_20061101_H_

#include "IBreakpoint.h"
#include "IRegion.h"
#include "MemoryRegion.h"
#include "Process.h"
#include <QByteArray>
#include <QHash>
#include <QMap>
#include <QStringList>
#include <QtPlugin>

class DebugEvent;
class IState;
class QString;
class State;

class IDebuggerCore {
public:
	virtual ~IDebuggerCore() {}

public:
	// system information
	virtual yad64::address_t page_size() const = 0;
	virtual int pointer_size() const = 0;

public:
	typedef QHash<yad64::address_t, IBreakpoint::pointer > BreakpointState;

public:
	virtual bool has_extension(quint64 ext) const = 0;

public:
	// returns true on success, false on failure, all bytes must be successfully
	// read/written in order for a success. The debugged application should be stopped
	// or this will return false immediately.
	virtual bool write_bytes(yad64::address_t address, const void *buf, std::size_t len) = 0;
	virtual bool read_bytes(yad64::address_t address, void *buf, std::size_t len) = 0;
	virtual bool read_pages(yad64::address_t address, void *buf, std::size_t count) = 0;

public:
	// thread support stuff (optional)
	virtual QList<yad64::tid_t> thread_ids() const { return QList<yad64::tid_t>(); }
	virtual yad64::tid_t active_thread() const     { return static_cast<yad64::tid_t>(-1); }
	virtual void set_active_thread(yad64::tid_t)   {}

public:
	virtual bool attach(yad64::pid_t pid) = 0;
	virtual bool open(const QString &path, const QString &cwd, const QList<QByteArray> &args) = 0;
	virtual bool open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty) = 0;
	virtual bool wait_debug_event(DebugEvent &event, int msecs) = 0;
	virtual void detach() = 0;
	virtual void get_state(State &state) = 0;
	virtual void kill() = 0;
	virtual void pause() = 0;
	virtual void resume(yad64::EVENT_STATUS status) = 0;
	virtual void set_state(const State &state) = 0;
	virtual void step(yad64::EVENT_STATUS status) = 0;

public:
	// basic breakpoint managment
	virtual BreakpointState backup_breakpoints() const = 0;
	virtual IBreakpoint::pointer add_breakpoint(yad64::address_t address) = 0;
	virtual IBreakpoint::pointer find_breakpoint(yad64::address_t address) = 0;
	virtual int breakpoint_size() const = 0;
	virtual void clear_breakpoints() = 0;
	virtual void remove_breakpoint(yad64::address_t address) = 0;

public:
	virtual QList<MemoryRegion> memory_regions() const = 0;

public:
	// process properties
	virtual QList<QByteArray> process_args(yad64::pid_t pid) const = 0;
	virtual QString process_cwd(yad64::pid_t pid) const = 0;
	virtual QString process_exe(yad64::pid_t pid) const = 0;
	virtual yad64::pid_t parent_pid(yad64::pid_t pid) const = 0;

public:
	virtual IState *create_state() const = 0;
	virtual IRegion *create_region(yad64::address_t start, yad64::address_t end, yad64::address_t base, const QString &name, IRegion::permissions_t permissions) const = 0;

public:
	// what is the PID of the process we are currently debugging
	// equal to "(yad64::pid_t)0" if we are not attached
	virtual yad64::pid_t pid() const = 0;
	virtual QMap<yad64::pid_t, Process> enumerate_processes() const = 0;
};

Q_DECLARE_INTERFACE(IDebuggerCore, "YAD64.IDebuggerCore/1.0")

#endif
