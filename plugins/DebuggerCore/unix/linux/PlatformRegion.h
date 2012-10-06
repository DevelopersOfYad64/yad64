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

#ifndef PLATFORM_REGION_20120330_H_
#define PLATFORM_REGION_20120330_H_

#include "IRegion.h"
#include <QString>
#include <QCoreApplication>

class PlatformRegion : public IRegion {
	Q_DECLARE_TR_FUNCTIONS(PlatformRegion)
	
	template <size_t N>
	friend class BackupInfo;
	
public:
	PlatformRegion(yad64::address_t start, yad64::address_t end, yad64::address_t base, const QString &name, permissions_t permissions);
	virtual ~PlatformRegion();

public:
	virtual IRegion *clone() const;

public:
	virtual bool accessible() const;
	virtual bool readable() const;
	virtual bool writable() const;
	virtual bool executable() const;
	virtual yad64::address_t size() const;
	virtual void set_permissions(bool read, bool write, bool execute);
	
public:
	virtual yad64::address_t start() const;
	virtual yad64::address_t end() const;
	virtual yad64::address_t base() const;
	virtual QString name() const;
	virtual permissions_t permissions() const;
	
private:
	void set_permissions(bool read, bool write, bool execute, yad64::address_t temp_address);
	
private:
	yad64::address_t start_;
	yad64::address_t end_;
	yad64::address_t base_;
	QString        name_;
	permissions_t  permissions_;
};

#endif

