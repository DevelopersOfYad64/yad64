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

#include "Configuration.h"
#include "IDebuggerCore.h"
#include "IPlugin.h"
#include "DebuggerMain.h"
#include "Debugger.h"
#include "DebuggerInternal.h"
#include "version.h"

#include <QApplication>
#include <QDir>
#include <QLibrary>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QPluginLoader>
#include <QTranslator>
#include <QtDebug>

#include <ctime>
#include <iostream>

namespace {
	//--------------------------------------------------------------------------
	// Name: load_plugins(const QString &directory)
	// Desc: attempts to load all plugins in a given directory
	//--------------------------------------------------------------------------
	void load_plugins(const QString &directory) {

		QDir plugins_dir(qApp->applicationDirPath());

		// TODO: attempt to detect the same plugin being loaded twice

		plugins_dir.cd(directory);

		Q_FOREACH(const QString &file_name, plugins_dir.entryList(QDir::Files)) {
			if(QLibrary::isLibrary(file_name)) {
				const QString full_path = plugins_dir.absoluteFilePath(file_name);
				QPluginLoader loader(full_path);
				loader.setLoadHints(QLibrary::ExportExternalSymbolsHint);

				if(QObject *const plugin = loader.instance()) {

					// TODO: handle the case where we find more than one core plugin...
					if(IDebuggerCore *const core_plugin = qobject_cast<IDebuggerCore *>(plugin)) {
                        if(yad64::v1::debugger_core == 0) {
							qDebug("[load_plugins] Loading Core Plugin: %-25s : %p", qPrintable(file_name), static_cast<void *>(plugin));
                            yad64::v1::debugger_core = core_plugin;
						}
					}

					if(IPlugin *const generic_plugin = qobject_cast<IPlugin *>(plugin)) {
                        if(yad64::internal::register_plugin(full_path, plugin)) {
							// we have a general purpose plugin
							qDebug("[load_plugins] Loading Plugin: %-30s : %p", qPrintable(file_name), static_cast<void *>(plugin));
						}
					}
				} else {
					qDebug() << "[load_plugins]" << qPrintable(loader.errorString());
				}
			}
		}
	}

	//--------------------------------------------------------------------------
    // Name: start_debugger(yad64::pid_t attach_pid)
	// Desc: starts the main debugger code
	//--------------------------------------------------------------------------
    int start_debugger(yad64::pid_t attach_pid, const QString &program, const QList<QByteArray> &programArgs) {

        qDebug() << "Starting yad64 version:" << yad64::version;
		qDebug("Please Report Bugs & Requests At: http://bugs.codef00.com/");

		// look for some plugins..
        load_plugins(yad64::v1::config().plugin_path);
		
        yad64::internal::load_function_db();

		// create the main window object
		DebuggerMain debugger;

		// let the plugins setup their menus
        debugger.finish_plugin_setup(yad64::v1::plugin_list());

		// ok things are initialized to a reasonable degree, let's show the main window
		debugger.show();

		// have we been asked to attach to a given program?
		if(attach_pid != 0) {
			debugger.attach(attach_pid);
		} else if(!program.isEmpty()) {
			debugger.execute(program, programArgs);
		}

        if(yad64::v1::debugger_core == 0) {
			QMessageBox::warning(
				0,
                QT_TRANSLATE_NOOP("yad64", "yad64 Failed To Load A Necessary Plugin"),
                QT_TRANSLATE_NOOP("yad64",
					"Failed to successfully load the debugger core plugin. Please make sure it exists and that the plugin path is correctly configured.\n"
                    "This is normal if yad64 has not been previously run or the configuration file has been removed."));

            yad64::v1::dialog_options()->exec();

			QMessageBox::warning(
				0,
                QT_TRANSLATE_NOOP("yad64", "yad64"),
                QT_TRANSLATE_NOOP("yad64", "yad64 will now close. If you were successful in specifying the location of the debugger core plugin, please run yad64 again.")
				);

			// TODO: detect if they corrected the issue and try again
			return -1;
		} else {
	    	return qApp->exec();
		}
	}

	void load_translations() {
		// load some translations
		QTranslator qtTranslator;
		qtTranslator.load("qt_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath));
		qApp->installTranslator(&qtTranslator);

		QTranslator myappTranslator;
        myappTranslator.load("yad64_" + QLocale::system().name());
		qApp->installTranslator(&myappTranslator);
	}
}


//------------------------------------------------------------------------------
// Name: main(int argc, char *argv[])
// Desc: entry point
//------------------------------------------------------------------------------
int main(int argc, char *argv[]) {

	QT_REQUIRE_VERSION(argc, argv, "4.5.0");

	QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon(":/debugger/images/yad6448-logo.png"));

	qsrand(std::time(0));

	// setup organization info so settings go in right place
	QApplication::setOrganizationName("codef00.com");
	QApplication::setOrganizationDomain("codef00.com");
    QApplication::setApplicationName("yad64");

	load_translations();

	const QStringList args = app.arguments();
    yad64::pid_t        attach_pid = 0;
	QList<QByteArray> run_args;
	QString           run_app;

	if(args.size() > 1) {
		if(args.size() == 3 && args[1] == "--attach") {
			attach_pid = args[2].toUInt();
		} else if(args.size() >= 3 && args[1] == "--run") {
			run_app = args[2];

			for(int i = 3; i < args.size(); ++i) {
				run_args.push_back(argv[i]);
			}
		} else if(args.size() == 2 && args[1] == "--version") {
            std::cout << "yad64 version: " << yad64::version << std::endl;
			return 0;
		} else if(args.size() == 2 && args[1] == "--dump-version") {
            std::cout << yad64::version << std::endl;
			return 0;
		} else {
            std::cerr << "usage: " << qPrintable(args[0]) << " [ --attach <pid> ] [ --run <program> (args...) ] [ --version ] [ --dump-version ]" << std::endl;
			return -1;
		}
	}

	return start_debugger(attach_pid, run_app, run_args);
}
