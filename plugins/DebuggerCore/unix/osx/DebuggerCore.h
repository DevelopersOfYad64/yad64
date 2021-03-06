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

#ifndef DEBUGGERCORE_20090529_H_
#define DEBUGGERCORE_20090529_H_

#include "DebuggerCoreUNIX.h"
#include <QHash>

class DebuggerCore : public DebuggerCoreUNIX {
	Q_OBJECT
	Q_INTERFACES(IDebuggerCore)
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	DebuggerCore();
	virtual ~DebuggerCore();

public:
	virtual edb::address_t page_size() const;
	virtual bool has_extension(quint64 ext) const;
	virtual bool wait_debug_event(DebugEvent &event, int msecs);
	virtual bool attach(edb::pid_t pid);
	virtual void detach();
	virtual void kill();
	virtual void pause();
	virtual void resume(edb::EVENT_STATUS status);
	virtual void step(edb::EVENT_STATUS status);
	virtual void get_state(State &state);
	virtual void set_state(const State &state);
	virtual bool open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty);

public:
	// thread support stuff (optional)
	virtual QList<edb::tid_t> thread_ids() const { return threads_.keys(); }
	virtual edb::tid_t active_thread() const     { return active_thread_; }
	virtual void set_active_thread(edb::tid_t);

public:
	virtual QList<MemoryRegion> memory_regions() const;

public:
	// process properties
	virtual QList<QByteArray> process_args(edb::pid_t pid) const;
	virtual QString process_exe(edb::pid_t pid) const;
	virtual QString process_cwd(edb::pid_t pid) const;
	virtual edb::pid_t parent_pid(edb::pid_t pid) const;

public:
	virtual IState *create_state() const;
	virtual IRegion *create_region(edb::address_t start, edb::address_t end, edb::address_t base, const QString &name, IRegion::permissions_t permissions) const;

private:
	virtual QMap<edb::pid_t, Process> enumerate_processes() const;

private:
	virtual long read_data(edb::address_t address, bool &ok);
	virtual bool write_data(edb::address_t address, long value);

private:
	struct thread_info {
	public:
		thread_info() : status(0) {
		}

		thread_info(int s) : status(s) {
		}

		int status;
	};

	typedef QHash<edb::tid_t, thread_info> threadmap_t;

	edb::address_t page_size_;
	threadmap_t    threads_;
};

#endif
