
#ifndef PROCESS_20120728_H_
#define PROCESS_20120728_H_

#include "Types.h"
#include <QString>

struct Process {
	yad64::pid_t pid;
	yad64::uid_t uid;
	QString    user;
	QString    name;
};

#endif
