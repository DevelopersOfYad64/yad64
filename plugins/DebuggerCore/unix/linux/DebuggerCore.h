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
#include <QSet>

class DebuggerCore : public DebuggerCoreUNIX {
	Q_OBJECT
	Q_INTERFACES(IDebuggerCore)
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	DebuggerCore();
	virtual ~DebuggerCore();

public:
	virtual yad64::address_t page_size() const;
	virtual bool has_extension(quint64 ext) const;
	virtual bool wait_debug_event(DebugEvent &event, int msecs);
	virtual bool attach(yad64::pid_t pid);
	virtual void detach();
	virtual void kill();
	virtual void pause();
	virtual void resume(yad64::EVENT_STATUS status);
	virtual void step(yad64::EVENT_STATUS status);
	virtual void get_state(State &state);
	virtual void set_state(const State &state);
	virtual bool open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty);

public:
	// thread support stuff (optional)
	virtual QList<yad64::tid_t> thread_ids() const { return threads_.keys(); }
	virtual yad64::tid_t active_thread() const     { return active_thread_; }
	virtual void set_active_thread(yad64::tid_t);

public:
	virtual QList<MemoryRegion> memory_regions() const;

public:
	virtual IState *create_state() const;
	virtual IRegion *create_region(yad64::address_t start, yad64::address_t end, yad64::address_t base, const QString &name, IRegion::permissions_t permissions) const;

public:
	// process properties
	virtual QList<QByteArray> process_args(yad64::pid_t pid) const;
	virtual QString process_cwd(yad64::pid_t pid) const;
	virtual QString process_exe(yad64::pid_t pid) const;
	virtual yad64::pid_t parent_pid(yad64::pid_t pid) const;

private:
	virtual QMap<yad64::pid_t, Process> enumerate_processes() const;

private:
	virtual long read_data(yad64::address_t address, bool &ok);
	virtual bool write_data(yad64::address_t address, long value);

private:
	long ptrace_continue(yad64::tid_t tid, long status);
	long ptrace_step(yad64::tid_t tid, long status);
	long ptrace_set_options(yad64::tid_t tid, long options);
	long ptrace_get_event_message(yad64::tid_t tid, unsigned long *message);
	long ptrace_traceme();

private:
	void reset();
	void stop_threads();
	bool handle_event(DebugEvent &event, yad64::tid_t tid, int status);
	bool attach_thread(yad64::tid_t tid);

private:
	struct thread_info {
		thread_info() : status(0) {}
		explicit thread_info(int s) : status(s) {}
		int status;
	};

	typedef QHash<yad64::tid_t, thread_info> threadmap_t;

	yad64::address_t   page_size_;
	threadmap_t      threads_;
	QSet<yad64::tid_t> waited_threads_;
	yad64::tid_t       event_thread_;
};

#endif
