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

#ifndef DEBUGGER_20061101_H_
#define DEBUGGER_20061101_H_

#include "API.h"
#include "IBinary.h"
#include "IBreakpoint.h"
#include "MemoryRegion.h"
#include "Module.h"
#include "Types.h"

#include <QString>
#include <QStringList>
#include <QHash>
#include <QPointer>
#include <QList>

class Configuration;
class FunctionInfo;
class IAnalyzer;
class IArchProcessor;
class IBinary;
class IDebugEventHandler;
class IDebuggerCore;
class IPlugin;
class ISessionFile;
class ISymbolManager;
class MemoryRegions;
class State;

class QByteArray;
class QDialog;
class QFileInfo;
class QWidget;

struct ExpressionError;

namespace yad64 {
	namespace v1 {

		// some useful objects
        YAD64_EXPORT extern IDebuggerCore *debugger_core;
        YAD64_EXPORT extern QWidget       *debugger_ui;

		// the symbol mananger
        YAD64_EXPORT ISymbolManager &symbol_manager();

		// the memory region manager
        YAD64_EXPORT MemoryRegions &memory_regions();

		// the current arch processor
        YAD64_EXPORT IArchProcessor &arch_processor();

		// widgets
        YAD64_EXPORT QWidget *disassembly_widget();


		// breakpoint managment
        YAD64_EXPORT IBreakpoint::pointer find_breakpoint(yad64::address_t address);
        YAD64_EXPORT QString get_breakpoint_condition(yad64::address_t address);
        YAD64_EXPORT yad64::address_t disable_breakpoint(yad64::address_t address);
        YAD64_EXPORT yad64::address_t enable_breakpoint(yad64::address_t address);
        YAD64_EXPORT void create_breakpoint(yad64::address_t address);
        YAD64_EXPORT void remove_breakpoint(yad64::address_t address);
        YAD64_EXPORT void set_breakpoint_condition(yad64::address_t address, const QString &condition);
        YAD64_EXPORT void toggle_breakpoint(yad64::address_t address);

        YAD64_EXPORT yad64::address_t current_data_view_address();

		// change what the various views show
        YAD64_EXPORT bool dump_data_range(yad64::address_t address, yad64::address_t end_address, bool new_tab);
        YAD64_EXPORT bool dump_data_range(yad64::address_t address, yad64::address_t end_address);
        YAD64_EXPORT bool dump_data(yad64::address_t address, bool new_tab);
        YAD64_EXPORT bool dump_data(yad64::address_t address);
        YAD64_EXPORT bool dump_stack(yad64::address_t address, bool scroll_to);
        YAD64_EXPORT bool dump_stack(yad64::address_t address);
        YAD64_EXPORT bool jump_to_address(yad64::address_t address);

		// ask the user for a value in an expression form
        YAD64_EXPORT bool get_expression_from_user(const QString &title, const QString prompt, yad64::address_t &value);
        YAD64_EXPORT bool eval_expression(const QString &expression, yad64::address_t &value);

		// ask the user for a value suitable for a register via an input box
        YAD64_EXPORT bool get_value_from_user(yad64::reg_t &value, const QString &title);
        YAD64_EXPORT bool get_value_from_user(yad64::reg_t &value);

		// ask the user for a binary string via an input box
        YAD64_EXPORT bool get_binary_string_from_user(QByteArray &value, const QString &title, int max_length);
        YAD64_EXPORT bool get_binary_string_from_user(QByteArray &value, const QString &title);

		// determine if the given address is the starting point of an string, if so, s will contain it
		// (formatted with C-style escape chars, so foundLength will have the original length of the string in chars).
        YAD64_EXPORT bool get_ascii_string_at_address(yad64::address_t address, QString &s, int min_length, int max_length, int &found_length);
        YAD64_EXPORT bool get_utf16_string_at_address(yad64::address_t address, QString &s, int min_length, int max_length, int &found_length);

		// list of loaded librarys
        YAD64_EXPORT QList<Module> loaded_libraries();

        YAD64_EXPORT MemoryRegion current_cpu_view_region();
        YAD64_EXPORT MemoryRegion primary_code_region();
        YAD64_EXPORT MemoryRegion primary_data_region();

		// configuration
        YAD64_EXPORT QPointer<QDialog> dialog_options();
        YAD64_EXPORT Configuration &config();

		// a numeric version of the current version suitable for integer comparison
        YAD64_EXPORT quint32 yad64_version();
        YAD64_EXPORT quint32 int_version(const QString &s);

		// symbol resolution
        YAD64_EXPORT QString find_function_symbol(yad64::address_t address);
        YAD64_EXPORT QString find_function_symbol(yad64::address_t address, const QString &default_value);
        YAD64_EXPORT QString find_function_symbol(yad64::address_t address, const QString &default_value, int *offset);

		// ask the user for either a value or a variable (register name and such)
        YAD64_EXPORT yad64::address_t get_value(yad64::address_t address, bool &ok, ExpressionError &err);
        YAD64_EXPORT yad64::address_t get_variable(const QString &s, bool &ok, ExpressionError &err);

		// hook the debug event system
        YAD64_EXPORT IDebugEventHandler *set_debug_event_handler(IDebugEventHandler *p);
        YAD64_EXPORT IDebugEventHandler *debug_event_handler();

        YAD64_EXPORT IAnalyzer *set_analyzer(IAnalyzer *p);
        YAD64_EXPORT IAnalyzer *analyzer();

        YAD64_EXPORT ISessionFile *set_session_file_handler(ISessionFile *p);
        YAD64_EXPORT ISessionFile *session_file_handler();

		// reads up to size bytes from address (stores how many it could read in size)
        YAD64_EXPORT bool get_instruction_bytes(yad64::address_t address, quint8 *buf, int &size);

        YAD64_EXPORT IBinary *get_binary_info(const MemoryRegion &region);
        YAD64_EXPORT const FunctionInfo *get_function_info(const QString &function);

        YAD64_EXPORT yad64::address_t locate_main_function();

        YAD64_EXPORT const QHash<QString, QObject *> &plugin_list();
        YAD64_EXPORT IPlugin *find_plugin_by_name(const QString &name);

        YAD64_EXPORT void reload_symbols();
        YAD64_EXPORT void repaint_cpu_view();

		// these are here and not members of state because
		// they may require using the debugger core plugin and
		// we don't want to force a dependancy between the two
        YAD64_EXPORT void pop_value(State &state);
        YAD64_EXPORT void push_value(State &state, yad64::reg_t value);

        YAD64_EXPORT void register_binary_info(IBinary::create_func_ptr_t fptr);

        YAD64_EXPORT bool overwrite_check(yad64::address_t address, unsigned int size);
        YAD64_EXPORT void modify_bytes(yad64::address_t address, unsigned int size, QByteArray &bytes, quint8 fill);

        YAD64_EXPORT QByteArray get_file_md5(const QString &s);
        YAD64_EXPORT QByteArray get_md5(const void *p, size_t n);

        YAD64_EXPORT QString basename(const QString &s);
        YAD64_EXPORT QString symlink_target(const QString &s);
        YAD64_EXPORT QStringList parse_command_line(const QString &cmdline);
        YAD64_EXPORT yad64::address_t string_to_address(const QString &s, bool &ok);
        YAD64_EXPORT QString format_bytes(const QByteArray &x);
        YAD64_EXPORT QString format_pointer(yad64::address_t p);

        YAD64_EXPORT yad64::address_t cpu_selected_address();

        YAD64_EXPORT void set_status(const QString &message);

        YAD64_EXPORT int pointer_size();
		
        YAD64_EXPORT QByteArray serialize_object(const QObject *object);
	}
}
#endif
