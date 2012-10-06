TEMPLATE    = app
#CONFIG     += silent
TARGET      = yad64
DEPENDPATH  += ./widgets ../include
INCLUDEPATH += ./widgets ../include D:/MinGW/boost_1_50_0
RESOURCES   = debugger.qrc
DESTDIR     = ../
target.path = /bin/
INSTALLS    += target 
QT          += xml

TRANSLATIONS += \
        lang/yad64_en.ts

HEADERS += \
	API.h \
	ArchProcessor.h \
	ArchTypes.h \
	BinaryString.h \
	ByteShiftArray.h \
	CommentServer.h \
	Configuration.h \
	DataViewInfo.h \
	DebugEvent.h \
	Debugger.h \
	DebuggerMain.h \
	DebuggerOps.h \
	DebuggerUI.h \
	DialogArguments.h \
	DialogAttach.h \
	DialogInputBinaryString.h \
	DialogInputValue.h \
	DialogMemoryRegions.h \
	DialogOptions.h \
	DialogPlugins.h \
	DialogThreads.h \
	Expression.h \
	FunctionInfo.h \
	IAnalyzer.h \
	IArchProcessor.h \
	IBinary.h \
	IBreakpoint.h \
	IDebugEventHandler.h \
	IDebuggerCore.h \
	IPlugin.h \
	IRegion.h \
	ISessionFile.h \
	IState.h \
	LineEdit.h \
	MD5.h \
	MemoryRegion.h \
	MemoryRegions.h \
	OSTypes.h \
	QCategoryList.h \
	QDisassemblyView.h \
	QLongValidator.h \
	QULongValidator.h \
	RecentFileManager.h \
	RegionBuffer.h \
	Register.h \
	RegisterViewDelegate.h \
	ScopedPointer.h \
	State.h \
	SymbolManager.h \
	SyntaxHighlighter.h \
	TabWidget.h \
	Types.h \
	Util.h \
	version.h

FORMS += \
	binarystring.ui \
	debuggerui.ui \
	dialog_arguments.ui \
	dialog_attach.ui \
	dialog_inputbinarystring.ui \
	dialog_inputvalue.ui \
	dialog_memoryregions.ui \
	dialog_options.ui \
	dialog_plugins.ui \
	dialog_threads.ui 

SOURCES += \
	ArchProcessor.cpp \
	BinaryString.cpp \
	ByteShiftArray.cpp \
	CommentServer.cpp \
	Configuration.cpp \
	DataViewInfo.cpp \
	DebugEvent.cpp \
	Debugger.cpp \
	DebuggerMain.cpp \
	DebuggerOS.cpp \
	DebuggerUI.cpp \
	DialogArguments.cpp \
	DialogAttach.cpp \
	DialogInputBinaryString.cpp \
	DialogInputValue.cpp \
	DialogMemoryRegions.cpp \
	DialogOptions.cpp \
	DialogPlugins.cpp \
	DialogThreads.cpp \
	IBinary.cpp \
	Instruction.cpp \
	LineEdit.cpp \
	MD5.cpp \
	MemoryRegion.cpp \
	MemoryRegions.cpp \
	QCategoryList.cpp \
	QDisassemblyView.cpp \
	QLongValidator.cpp \
	QULongValidator.cpp \
	RecentFileManager.cpp \
	RegionBuffer.cpp \
	Register.cpp \
	RegisterViewDelegate.cpp \
	State.cpp \
	SymbolManager.cpp \
	SyntaxHighlighter.cpp \
	TabWidget.cpp \
	main.cpp

DEPENDPATH  += ./qhexview
INCLUDEPATH += ./qhexview
SOURCES     += qhexview.cpp
HEADERS     += qhexview.h QHexView

# qjson stuff

DEPENDPATH   += ./qjson 
INCLUDEPATH  += ./qjson 

HEADERS += json_parser.hh \
           json_scanner.h \
           location.hh \
           parser.h \
           parser_p.h \
           parserrunnable.h \
           position.hh \
           qjson_debug.h \
           qjson_export.h \
           qobjecthelper.h \
           serializer.h \
           serializerrunnable.h \
           stack.hh
		   
SOURCES += json_parser.cc \
           json_scanner.cpp \
           parser.cpp \
           parserrunnable.cpp \
           qobjecthelper.cpp \
           serializer.cpp \
           serializerrunnable.cpp
win32 {
	win32-msvc*:contains(QMAKE_HOST.arch, x86_64):{
		DEPENDPATH  += os/win64 ../include/os/win64 arch/x86_64 ../include/arch/x86_64 edisassm
                INCLUDEPATH += os/win64 ../include/os/win64 arch/x86_64 ../include/arch/x86_64 edisassm "D:\\MinGW\\boost_1_50_0"
		DEFINES     += _CRT_SECURE_NO_WARNINGS QJSON_MAKEDLL
                RC_FILE     = yad64.rc
	}
}
