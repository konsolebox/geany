/*
 *      vte.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <<nick(dot)treleaven(at)btinternet(dot)com>
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

#ifndef GEANY_VTE_H
#define GEANY_VTE_H 1

#ifdef HAVE_VTE

#include "gtkcompat.h"

#include "document.h"

G_BEGIN_DECLS

typedef struct
{
	gboolean load_vte;			/* this is the preference, NOT the current instance VTE state */
	gboolean load_vte_cmdline;	/* this is the command line option */
	gboolean have_vte;			/* use this field to check if the current instance has VTE */
	gchar	*lib_vte;
	gchar	*dir;
} VteInfo;

extern VteInfo vte_info;

typedef struct
{
	GtkWidget *vte;
	GtkWidget *menu;
	GtkWidget *im_submenu;
	gboolean scroll_on_key;
	gboolean scroll_on_out;
	gboolean ignore_menu_bar_accel;
	gboolean follow_path;
	gboolean run_in_vte;
	gboolean skip_run_script;
	gboolean enable_bash_keys;
	gboolean ctrl_c_d_resets_vte;
	gboolean cursor_blinks;
	gboolean send_selection_unsafe;
	gboolean allow_bold;
	gboolean cwd_real_path;
	gint scrollback_lines;
	gchar *shell;
	gchar *font;
	gchar *send_cmd_prefix;
	GdkColor colour_fore;
	GdkColor colour_back;
} VteConfig;

extern VteConfig vte_config;

typedef enum
{
	VTE_IGNORE_FOLLOW_PATH			= 1,
	VTE_IGNORE_DIRTY				= 2,
	VTE_SHOW_DIR_NOT_CHANGED		= 4,
	VTE_SHOW_DIR_ALREADY_CORRECT	= 8
} VteCwdFlags;

void vte_init(void);

void vte_close(void);

void vte_apply_user_settings(void);

gboolean vte_send_cmd(const gchar *cmd, gboolean ignore_dirty);

const gchar *vte_get_working_directory(void);

gboolean vte_cwd(const gchar *path, VteCwdFlags flags);

gboolean vte_cwd_document(GeanyDocument *doc, VteCwdFlags flags);

gboolean vte_cwd_current_document(VteCwdFlags flags);

void vte_append_preferences_tab(void);

void vte_send_selection_to_vte(void);

void vte_select_all(void);

G_END_DECLS

#endif /* HAVE_VTE */

#endif /* GEANY_VTE_H */
