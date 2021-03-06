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

#include "DebuggerCoreBase.h"
#include <QSet>

class DebuggerCore : public DebuggerCoreBase {
	Q_OBJECT
	Q_INTERFACES(IDebuggerCore)
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")
public:
	DebuggerCore();
	virtual ~DebuggerCore();

public:
	virtual bool has_extension(quint64 ext) const;
	virtual yad64::address_t page_size() const;
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
	virtual bool read_pages(yad64::address_t address, void *buf, std::size_t count);
	virtual bool read_bytes(yad64::address_t address, void *buf, std::size_t len);
	virtual bool write_bytes(yad64::address_t address, const void *buf, std::size_t len);
	virtual int pointer_size() const;

public:
	// thread support stuff (optional)
	virtual QList<yad64::tid_t> thread_ids() const   { return threads_.toList(); }
	virtual yad64::tid_t active_thread() const       { return active_thread_; }
	virtual void set_active_thread(yad64::tid_t tid) { Q_ASSERT(threads_.contains(tid)); active_thread_ = tid; }

public:
	virtual QList<MemoryRegion> memory_regions() const;

public:
	// process properties
	virtual QList<QByteArray> process_args(yad64::pid_t pid) const;
	virtual QString process_exe(yad64::pid_t pid) const;
	virtual QString process_cwd(yad64::pid_t pid) const;
	virtual yad64::pid_t parent_pid(yad64::pid_t pid) const;

public:
	virtual IState *create_state() const;
	virtual IRegion *create_region(yad64::address_t start, yad64::address_t end, yad64::address_t base, const QString &name, IRegion::permissions_t permissions) const;

private:
	virtual QMap<yad64::pid_t, Process> enumerate_processes() const;
	
public:
	// NOTE: win32 only stuff here!
	yad64::address_t start_address;
	yad64::address_t image_base;

private:
	bool attached() { return DebuggerCoreBase::attached() && process_handle_ != 0; }

private:
	yad64::address_t   page_size_;
	HANDLE           process_handle_;
	QSet<yad64::tid_t> threads_;

private:
	// Checks if an addition would cause overflow (wraparound)
	template<typename T>
	bool overflows(const T& v1, const T& v2, const T& max = std::numeric_limits<T>::max()) {
		return (v1 > 0) && (v2 > 0) && (v1 > (max - v2));
	}
	
	bool set_debug_privilege(HANDLE process, bool set);
};

#endif
