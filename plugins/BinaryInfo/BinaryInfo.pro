
include(../plugins.pri)

# Input
HEADERS += BinaryInfo.h
FORMS += 
SOURCES += BinaryInfo.cpp

unix {
	HEADERS += ELF32.h ELF64.h
	SOURCES += ELF32.cpp ELF64.cpp
}

win32 {
	HEADERS += PE.h
	SOURCES += PE32.cpp PE64.cpp
}

