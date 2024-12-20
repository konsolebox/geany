/*
 *      main.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef GEANY_MAIN_H
#define GEANY_MAIN_H 1

#include <glib.h>

G_BEGIN_DECLS

void main_reload_configuration(void);

void main_locale_init(const gchar *locale_dir, const gchar *gettext_package);

gboolean main_is_realized(void);

#ifdef GEANY_PRIVATE

typedef enum NewInstanceMode
{
	NEW_INSTANCE_EXPLICITLY_DISABLED,
	NEW_INSTANCE_DISABLED,
	NEW_INSTANCE_ENABLED
} NewInstanceMode;

typedef struct
{
	NewInstanceMode new_instance_mode;
	gchar	   *socket_filename;
	gboolean	load_session;
	gint		goto_line;
	gint		goto_column;
	gboolean	ignore_global_tags;
	gboolean	list_documents;
	gboolean 	readonly;
	gboolean	no_projects;
	gboolean	favorite;
}
CommandLineOptions;

extern CommandLineOptions cl_options;

typedef struct GeanyStatus
{
	gint	opening_session_files;	/* >0 indicates batch-opening of session files */
	gboolean	closing_all; /* the state while closing all tabs
							  * (used to prevent notebook switch page signals) */
	gboolean	quitting;	/* state when Geany is quitting completely */
	gboolean	main_window_realized;
	gboolean	opening_files_recursively;
	gboolean	reloading_all_files;
	gboolean	opening_cl_files;
	gboolean	handling_input_filenames;
	gboolean	manually_closing_all;
}
GeanyStatus;

extern GeanyStatus main_status;

const gchar *main_get_version_string(void);

gchar *main_get_argv_filename(const gchar *filename);

gboolean main_quit(void);

gboolean main_handle_filename(const gchar *locale_filename, GError **error);

void main_load_project_from_command_line(const gchar *locale_filename, gboolean use_session);

gint main_lib(gint argc, gchar **argv);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_MAIN_H */
