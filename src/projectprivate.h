/*
 *      projectprivate.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2008-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

#ifndef GEANY_PROJECTPRIVATE_H
#define GEANY_PROJECTPRIVATE_H 1

#include "project.h"

#include <glib.h>

G_BEGIN_DECLS

typedef struct GeanyProjectPrivate
{
	// file prefs
	gboolean	final_new_line_unless_empty;
	gboolean	strip_trailing_spaces;
	gboolean	replace_tabs;
	gboolean	ensure_convert_new_lines;

	// editor prefs
	struct GeanyIndentPrefs *indentation;
	gboolean	line_wrapping;
	gint		line_break_column;
	gboolean	auto_continue_multiline;
	gint		long_line_behaviour; /* 0 - disabled, 1 - follow global settings, 2 - enabled (custom) */
	gint		long_line_column; /* Long line marker position. */

	GPtrArray *build_filetypes_list; /* Project has custom filetype builds for these. */
	GPtrArray *session_files;
}
GeanyProjectPrivate;

G_END_DECLS

#endif /* GEANY_PROJECT_H */
