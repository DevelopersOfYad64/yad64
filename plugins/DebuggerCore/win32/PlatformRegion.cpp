
#include "PlatformRegion.h"
#include "MemoryRegion.h"
#include "MemoryRegions.h"
#include "Debugger.h"
#include "IDebuggerCore.h"
#include "State.h"
#include "IDebugEventHandler.h"
#include <QMessageBox>

namespace {
	const IRegion::permissions_t KNOWN_PERMISSIONS = (PAGE_NOACCESS | PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
}

PlatformRegion::PlatformRegion(yad64::address_t start, yad64::address_t end, yad64::address_t base, const QString &name, permissions_t permissions) : start_(start), end_(end), base_(base), name_(name), permissions_(permissions) {
}

PlatformRegion::~PlatformRegion() {
}

IRegion *PlatformRegion::clone() const {
	return new PlatformRegion(start_, end_, base_, name_, permissions_);
}

bool PlatformRegion::accessible() const {
	return readable() || writable() || executable();
}

bool PlatformRegion::readable() const {
	switch(permissions_ & KNOWN_PERMISSIONS) { // ignore modifiers
		case PAGE_EXECUTE_READ:
		case PAGE_EXECUTE_READWRITE:
		case PAGE_READONLY:
		case PAGE_READWRITE:
			return true;
		default:
			return false;
	}
}

bool PlatformRegion::writable() const {
	switch(permissions_ & KNOWN_PERMISSIONS) { // ignore modifiers
		case PAGE_EXECUTE_READWRITE:
		case PAGE_EXECUTE_WRITECOPY:
		case PAGE_READWRITE:
		case PAGE_WRITECOPY:
			return true;
		default:
			return false;
	}
}

bool PlatformRegion::executable() const {
	switch(permissions_ & KNOWN_PERMISSIONS) { // ignore modifiers
		case PAGE_EXECUTE:
		case PAGE_EXECUTE_READ:
		case PAGE_EXECUTE_READWRITE:
		case PAGE_EXECUTE_WRITECOPY:
			return true;
		default:
			return false;
	}
}

yad64::address_t PlatformRegion::size() const {
	return end_ - start_;
}

void PlatformRegion::set_permissions(bool read, bool write, bool execute) {
	if(HANDLE ph = OpenProcess(PROCESS_VM_OPERATION, FALSE, yad64::v1::debugger_core->pid())) {
		DWORD prot = PAGE_NOACCESS;

		switch((static_cast<int>(read) << 2) | (static_cast<int>(write) << 1) | static_cast<int>(execute)) {
		case 0x0: prot = PAGE_NOACCESS;          break;
		case 0x1: prot = PAGE_EXECUTE;           break;
		case 0x2: prot = PAGE_WRITECOPY;         break;
		case 0x3: prot = PAGE_EXECUTE_WRITECOPY; break;
		case 0x4: prot = PAGE_READONLY;          break;
		case 0x5: prot = PAGE_EXECUTE_READ;      break;
		case 0x6: prot = PAGE_READWRITE;         break;
		case 0x7: prot = PAGE_EXECUTE_READWRITE; break;
		}

		prot |= permissions_ & ~KNOWN_PERMISSIONS; // keep modifiers

		DWORD prev_prot;
		if(VirtualProtectEx(ph, reinterpret_cast<LPVOID>(start()), size(), prot, &prev_prot)) {
			permissions_ = prot;
		}
		
		CloseHandle(ph);
	}
}

yad64::address_t PlatformRegion::start() const {
	return start_;
}

yad64::address_t PlatformRegion::end() const {
	return end_;
}

yad64::address_t PlatformRegion::base() const {
	return base_;
}

QString PlatformRegion::name() const {
	return name_;
}

IRegion::permissions_t PlatformRegion::permissions() const {
	return permissions_;
}

