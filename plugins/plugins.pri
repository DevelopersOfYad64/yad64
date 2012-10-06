
YAD64_ROOT = ../..

TEMPLATE = lib
CONFIG   += plugin
#CONFIG  += silent
DESTDIR  = $$YAD64_ROOT
INSTALLS += target

DEFINES += YAD64_PLUGIN

# ignore missing symbols, they'll be found when linked into yad64
win32 {
	win32-msvc*:contains(QMAKE_HOST.arch, x86_64):{
		INCLUDEPATH += $$YAD64_ROOT/include/os/win64 $$YAD64_ROOT/include $$YAD64_ROOT/include/arch/x86_64 "D:\\MinGW\\boost_1_50_0"
		DEPENDPATH  += $$YAD64_ROOT/include/os/win64 $$YAD64_ROOT/include $$YAD64_ROOT/include/arch/x86_64	
		LIBS        += $$YAD64_ROOT/yad64.lib
	}
}
