/*
 *      sidebar.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

#ifndef GEANY_SIDEBAR_H
#define GEANY_SIDEBAR_H 1

#include "document.h"

#include "gtkcompat.h"

G_BEGIN_DECLS

typedef struct SidebarTreeviews
{
	GtkWidget		*tree_openfiles;
	GtkWidget		*default_tag_tree;
	GtkWidget		*popup_taglist;
	GtkWidget		*tree_highlights;
}
SidebarTreeviews;

extern SidebarTreeviews tv;

enum
{
	SYMBOLS_COLUMN_ICON,
	SYMBOLS_COLUMN_NAME,
	SYMBOLS_COLUMN_TAG,
	SYMBOLS_COLUMN_TOOLTIP,
	SYMBOLS_N_COLUMNS
};

void sidebar_init(void);

void sidebar_finalize(void);

void sidebar_update_tag_list(GeanyDocument *doc, gboolean update);

gchar *sidebar_get_doc_folder(GeanyDocument *doc, gchar **out_path);

void sidebar_openfiles_add(GeanyDocument *doc);

void sidebar_openfiles_update(GeanyDocument *doc);

void sidebar_openfiles_update_all(void);

void sidebar_select_openfiles_item(GeanyDocument *doc);

void sidebar_remove_document(GeanyDocument *doc);

void sidebar_add_common_menu_items(GtkMenu *menu);

void sidebar_focus_openfiles_tab(void);

void sidebar_focus_symbols_tab(void);

void sidebar_rename_file_cb(guint key_id);

void sidebar_openfiles_scroll_to_row(GeanyDocument *doc);

G_END_DECLS

#endif /* GEANY_SIDEBAR_H */
