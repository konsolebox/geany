/*
 *      sidebar.c - this file is part of Geany, a fast and lightweight IDE
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

/*
 * Sidebar related code for the Symbol list and Open files GtkTreeViews.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "sidebar.h"

#include "app.h"
#include "callbacks.h" /* FIXME: for ignore_callback */
#include "dialogs.h"
#include "documentprivate.h"
#include "filetypesprivate.h"
#include "geanyobject.h"
#include "keyfile.h"
#include "nav.h"
#include "stash.h"
#include "support.h"
#include "symbols.h"
#include "ui_utils.h"
#include "utils.h"
#include "keybindings.h"

#include <string.h>

#include <gdk/gdkkeysyms.h>

#include <errno.h>

#include <glib/gstdio.h>

SidebarTreeviews tv = {NULL, NULL, NULL};
/* while typeahead searching, editor should not get focus */
static gboolean may_steal_focus = FALSE;

static struct
{
	GtkWidget *close;
	GtkWidget *close_recursively;
	GtkWidget *new;
	GtkWidget *open;
	GtkWidget *open_dir;
	GtkWidget *favorite;
	GtkWidget *clear_favorites;
	GtkWidget *readonly;
	GtkWidget *save;
	GtkWidget *save_as;
	GtkWidget *reload;
	GtkWidget *rename;
	GtkWidget *clone;
	GtkWidget *delete;
	GtkWidget *show_paths;
	GtkWidget *find_in_files;
	GtkWidget *expand_all;
	GtkWidget *collapse_all;
}
doc_items = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL
};

enum
{
	TREEVIEW_SYMBOL = 0,
	TREEVIEW_OPENFILES
};

enum
{
	OPENFILES_ACTION_REMOVE = 0,
	OPENFILES_ACTION_REMOVE_RECURSIVE,
	OPENFILES_ACTION_NEW,
	OPENFILES_ACTION_OPEN,
	OPENFILES_ACTION_OPEN_DIR,
	OPENFILES_ACTION_SAVE,
	OPENFILES_ACTION_SAVE_AS,
	OPENFILES_ACTION_RELOAD,
	OPENFILES_ACTION_RELOAD_NO_PROMPT,
	OPENFILES_ACTION_RENAME,
	OPENFILES_ACTION_CLONE,
	OPENFILES_ACTION_DELETE,
	OPENFILES_ACTION_FAVORITE,
	OPENFILES_ACTION_CLEAR_FAVORITES,
	OPENFILES_ACTION_READONLY
};

/* documents tree model columns */
enum
{
	DOCUMENTS_TYPE,
	DOCUMENTS_ICON,
	DOCUMENTS_SHORTNAME,	/* dirname for parents, basename for children */
	DOCUMENTS_DOCUMENT,
	DOCUMENTS_COLOR,
	DOCUMENTS_FILENAME,		/* full filename */
	DOCUMENTS_WEIGHT
};

enum {
	DOCUMENTS_TOTAL_COLUMNS = DOCUMENTS_WEIGHT + 1
};

enum {
	TYPE_FILE,
	TYPE_FOLDER,
	TYPE_FAVORITES_FOLDER,
};

typedef enum
{
	FOLDER_FAVORITE_STATUS_INACTIVE = 0,
	FOLDER_FAVORITE_STATUS_ACTIVE = 1,
	FOLDER_FAVORITE_STATUS_INCONSISTENT = -1
} FolderFavoriteStatus;

typedef enum
{
	FOLDER_READONLY_STATUS_INACTIVE = 0,
	FOLDER_READONLY_STATUS_ACTIVE = 1,
	FOLDER_READONLY_STATUS_INCONSISTENT = -1
} FolderReadOnlyStatus;

static GtkTreeStore	*store_openfiles;
static GtkWidget *openfiles_popup_menu;
static gboolean documents_show_paths;
static GtkWidget *tag_window;	/* scrolled window that holds the symbol list GtkTreeView */
static gboolean updating_menu_items = FALSE;
static GtkTreeIter iter_favorites = {0};

/* callback prototypes */
static void on_openfiles_document_action(GtkMenuItem *menuitem, gpointer user_data);
static gboolean sidebar_button_press_cb(GtkWidget *widget, GdkEventButton *event,
		gpointer user_data);
static gboolean sidebar_key_press_cb(GtkWidget *widget, GdkEventKey *event,
		gpointer user_data);
static void on_list_document_activate(GtkCheckMenuItem *item, gpointer user_data);
static void on_list_symbol_activate(GtkCheckMenuItem *item, gpointer user_data);
static void documents_menu_update(GtkTreeSelection *selection);
static void sidebar_tabs_show_hide(GtkNotebook *notebook, GtkWidget *child,
								   guint page_num, gpointer data);
static void on_openfiles_renamed(GtkCellRenderer *renderer, const gchar *path_string,
		const gchar *name_new, gpointer user_data);

/* the prepare_* functions are document-related, but I think they fit better here than in document.c */
static void prepare_taglist(GtkWidget *tree, GtkTreeStore *store)
{
	GtkCellRenderer *text_renderer, *icon_renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	text_renderer = gtk_cell_renderer_text_new();
	icon_renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new();

	gtk_tree_view_column_pack_start(column, icon_renderer, FALSE);
  	gtk_tree_view_column_set_attributes(column, icon_renderer, "pixbuf", SYMBOLS_COLUMN_ICON, NULL);
  	g_object_set(icon_renderer, "xalign", 0.0, NULL);

  	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
  	gtk_tree_view_column_set_attributes(column, text_renderer, "text", SYMBOLS_COLUMN_NAME, NULL);
  	g_object_set(text_renderer, "yalign", 0.5, NULL);
  	gtk_tree_view_column_set_title(column, _("Symbols"));

	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);

	ui_widget_modify_font_from_string(tree, interface_prefs.tagbar_font);

	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));
	g_object_unref(store);

	g_signal_connect(tree, "button-press-event",
		G_CALLBACK(sidebar_button_press_cb), NULL);
	g_signal_connect(tree, "key-press-event",
		G_CALLBACK(sidebar_key_press_cb), NULL);

	gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(tree), interface_prefs.show_symbol_list_expanders);
	if (! interface_prefs.show_symbol_list_expanders)
		gtk_tree_view_set_level_indentation(GTK_TREE_VIEW(tree), 10);
	/* Tooltips */
	ui_tree_view_set_tooltip_text_column(GTK_TREE_VIEW(tree), SYMBOLS_COLUMN_TOOLTIP);

	/* selection handling */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	/* callback for changed selection not necessary, will be handled by button-press-event */
}

static gboolean
on_default_tag_tree_button_press_event(GtkWidget *widget, GdkEventButton *event,
		gpointer user_data)
{
	if (event->button == 3)
	{
		gtk_menu_popup(GTK_MENU(tv.popup_taglist), NULL, NULL, NULL, NULL,
			event->button, event->time);
		return TRUE;
	}
	return FALSE;
}

static void create_default_tag_tree(void)
{
	GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW(tag_window);
	GtkWidget *label;

	/* default_tag_tree is a GtkViewPort with a GtkLabel inside it */
	tv.default_tag_tree = gtk_viewport_new(
		gtk_scrolled_window_get_hadjustment(scrolled_window),
		gtk_scrolled_window_get_vadjustment(scrolled_window));
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(tv.default_tag_tree), GTK_SHADOW_NONE);
	label = gtk_label_new(_("No symbols found"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.1f, 0.01f);
	gtk_container_add(GTK_CONTAINER(tv.default_tag_tree), label);
	gtk_widget_show_all(tv.default_tag_tree);
	g_signal_connect(tv.default_tag_tree, "button-press-event",
		G_CALLBACK(on_default_tag_tree_button_press_event), NULL);
	g_object_ref((gpointer)tv.default_tag_tree);	/* to hold it after removing */
}

/* update = rescan the tags for doc->filename */
void sidebar_update_tag_list(GeanyDocument *doc, gboolean update)
{
	GtkWidget *child = gtk_bin_get_child(GTK_BIN(tag_window));

	g_return_if_fail(doc == NULL || doc->is_valid);

	if (update)
		doc->priv->tag_tree_dirty = TRUE;

	if (gtk_notebook_get_current_page(GTK_NOTEBOOK(main_widgets.sidebar_notebook)) != TREEVIEW_SYMBOL)
		return; /* don't bother updating symbol tree if we don't see it */

	/* changes the tree view to the given one, trying not to do useless changes */
	#define CHANGE_TREE(new_child) \
		G_STMT_START { \
			/* only change the tag tree if it's actually not the same (to avoid flickering) and if
			 * it's the one of the current document (to avoid problems when e.g. reloading
			 * configuration files */ \
			if (child != new_child && doc == document_get_current()) \
			{ \
				if (child) \
					gtk_container_remove(GTK_CONTAINER(tag_window), child); \
				gtk_container_add(GTK_CONTAINER(tag_window), new_child); \
			} \
		} G_STMT_END

	if (tv.default_tag_tree == NULL)
		create_default_tag_tree();

	/* show default empty tag tree if there are no tags */
	if (doc == NULL || doc->file_type == NULL || ! filetype_has_tags(doc->file_type))
	{
		CHANGE_TREE(tv.default_tag_tree);
		return;
	}

	if (doc->priv->tag_tree_dirty)
	{	/* updating the tag list in the left tag window */
		if (doc->priv->tag_tree == NULL)
		{
			doc->priv->tag_store = gtk_tree_store_new(
				SYMBOLS_N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, TM_TYPE_TAG, G_TYPE_STRING);
			doc->priv->tag_tree = gtk_tree_view_new();
			prepare_taglist(doc->priv->tag_tree, doc->priv->tag_store);
			gtk_widget_show(doc->priv->tag_tree);
			g_object_ref((gpointer)doc->priv->tag_tree);	/* to hold it after removing */
		}

		doc->has_tags = symbols_recreate_tag_list(doc, SYMBOLS_SORT_USE_PREVIOUS);
		doc->priv->tag_tree_dirty = FALSE;
	}

	if (doc->has_tags)
	{
		CHANGE_TREE(doc->priv->tag_tree);
	}
	else
	{
		CHANGE_TREE(tv.default_tag_tree);
	}

	#undef CHANGE_TREE
}

static gint documents_sort_func(GtkTreeModel *model, GtkTreeIter *iter_a,
								GtkTreeIter *iter_b, gpointer data)
{
	gchar *key_a, *key_b, *name_a, *name_b;
	gint cmp, type_a, type_b;

	gtk_tree_model_get(model, iter_a, DOCUMENTS_TYPE, &type_a, DOCUMENTS_SHORTNAME, &name_a, -1);
	gtk_tree_model_get(model, iter_b, DOCUMENTS_TYPE, &type_b, DOCUMENTS_SHORTNAME, &name_b, -1);

	if (type_a == TYPE_FAVORITES_FOLDER)
		cmp = -1;
	else if (type_b == TYPE_FAVORITES_FOLDER)
		cmp = 1;
	else
	{
		key_a = g_utf8_collate_key_for_filename(name_a, -1);
		key_b = g_utf8_collate_key_for_filename(name_b, -1);
		cmp = strcmp(key_a, key_b);
		g_free(key_b);
		g_free(key_a);
	}

	g_free(name_a);
	g_free(name_b);
	return cmp;
}

/* does some preparing things to the open files list widget */
static void prepare_openfiles(void)
{
	GtkCellRenderer *icon_renderer;
	GtkCellRenderer *text_renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkTreeSortable *sortable;

	tv.tree_openfiles = ui_lookup_widget(main_widgets.window, "treeview6");

	/* store the icon and the short filename to show, and the index as reference,
	 * the colour (black/red/green) and the full name for the tooltip */
	store_openfiles = gtk_tree_store_new(DOCUMENTS_TOTAL_COLUMNS, G_TYPE_INT, G_TYPE_ICON,
			G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_COLOR, G_TYPE_STRING, G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tv.tree_openfiles), GTK_TREE_MODEL(store_openfiles));

	/* set policy settings for the scolledwindow around the treeview again, because glade
	 * doesn't keep the settings */
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(ui_lookup_widget(main_widgets.window, "scrolledwindow7")),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	icon_renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(icon_renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);
	text_renderer = gtk_cell_renderer_text_new();
	g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_MIDDLE, NULL);
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_pack_start(column, icon_renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, icon_renderer, "gicon", DOCUMENTS_ICON, NULL);
	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, text_renderer, "text", DOCUMENTS_SHORTNAME,
		"foreground-gdk", DOCUMENTS_COLOR, "weight", DOCUMENTS_WEIGHT, NULL);
	g_signal_connect(G_OBJECT(text_renderer), "edited", G_CALLBACK(on_openfiles_renamed), GTK_TREE_VIEW(tv.tree_openfiles));

	gtk_tree_view_append_column(GTK_TREE_VIEW(tv.tree_openfiles), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tv.tree_openfiles), FALSE);

	gtk_tree_view_set_search_column(GTK_TREE_VIEW(tv.tree_openfiles),
		DOCUMENTS_SHORTNAME);

	/* sort opened filenames in the store_openfiles treeview */
	sortable = GTK_TREE_SORTABLE(GTK_TREE_MODEL(store_openfiles));
	gtk_tree_sortable_set_sort_func(sortable, DOCUMENTS_SHORTNAME, documents_sort_func, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(sortable, DOCUMENTS_SHORTNAME, GTK_SORT_ASCENDING);

	ui_widget_modify_font_from_string(tv.tree_openfiles, interface_prefs.tagbar_font);

	/* tooltips */
	ui_tree_view_set_tooltip_text_column(GTK_TREE_VIEW(tv.tree_openfiles), DOCUMENTS_FILENAME);

	/* selection handling */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	g_object_unref(store_openfiles);

	g_signal_connect(GTK_TREE_VIEW(tv.tree_openfiles), "button-press-event",
		G_CALLBACK(sidebar_button_press_cb), NULL);
	g_signal_connect(GTK_TREE_VIEW(tv.tree_openfiles), "key-press-event",
		G_CALLBACK(sidebar_key_press_cb), NULL);
}

static gboolean utils_filename_has_prefix(const gchar *str, const gchar *prefix)
{
	gchar *head = g_strndup(str, strlen(prefix));
	gboolean ret = utils_filenamecmp(head, prefix) == 0;

	g_free(head);
	return ret;
}

gchar *sidebar_get_doc_folder(GeanyDocument *doc, gchar **out_path)
{
	if (out_path)
		*out_path = NULL;

	g_return_val_if_fail(doc != NULL, NULL);

	if (doc->priv->folder && doc->priv->folder_path)
		goto exit;

	gchar *folder = NULL;
	gchar *path = NULL;

	if (interface_prefs.sidebar_folders_use_real_path)
	{
		gchar *real_path_locale = NULL;

		if (doc->real_path)
			real_path_locale = tm_get_real_path(doc->real_path);
		else if (doc->file_name)
		{
			gchar *dirname = g_path_get_dirname(doc->file_name);

			if (strcmp(dirname, ".") != 0)
			{
				gchar *filename_locale = utils_get_locale_from_utf8(doc->file_name);
				real_path_locale = tm_get_real_path(filename_locale);
				g_free(filename_locale);
			}

			g_free(dirname);
		}

		if (real_path_locale)
		{
			gchar *real_path_utf8 = utils_get_utf8_from_locale(real_path_locale);
			path = g_path_get_dirname(real_path_utf8);
			g_free(real_path_utf8);
			g_free(real_path_locale);
		}
	}

	if (path == NULL)
	{
		path = g_path_get_dirname(DOC_FILENAME(doc));
		g_return_val_if_fail(path != NULL, NULL);

		if (strcmp(path, ".") == 0)
		{
			folder = g_strdup(GEANY_STRING_UNTITLED);
			goto save_and_exit;
		}
	}

	/* replace the project base path with the project name */
	gchar *project_base_path = project_get_base_path();

	if (project_base_path)
	{
		if (interface_prefs.sidebar_folders_use_real_path)
		{
			gchar *real_path = tm_get_real_path(project_base_path);

			if (real_path)
				SETPTR(project_base_path, real_path);
		}

		gsize len = strlen(project_base_path);

		/* remove trailing separator so we can match base path exactly */
		if (project_base_path[len-1] == G_DIR_SEPARATOR)
			project_base_path[--len] = '\0';

		/* check whether the dir name matches or uses the project base path */
		if (utils_filename_has_prefix(path, project_base_path))
		{
			const gchar *rest = path + len;

			if (*rest == G_DIR_SEPARATOR || *rest == '\0')
				folder = g_strdup_printf("%s%s", app->project->name, rest);
		}

		g_free(project_base_path);
	}

	if (folder == NULL)
	{
		const gchar *home_dir = g_get_home_dir();

		/* If matches home dir, replace with tilde */
		if (!EMPTY(home_dir) && utils_filename_has_prefix(path, home_dir))
		{
			const gchar *rest = path + strlen(home_dir);

			if (*rest == G_DIR_SEPARATOR || *rest == '\0')
			{
				folder = g_strdup_printf("~%s", rest);
				goto save_and_exit;
			}
		}

		folder = g_strdup(path);
	}

save_and_exit:
	SETPTR(doc->priv->folder, folder);
	SETPTR(doc->priv->folder_path, path);

exit:
	if (out_path)
		*out_path = g_strdup(doc->priv->folder_path);

	return g_strdup(doc->priv->folder);
}

static GIcon *get_file_icon(GeanyDocument *doc, gboolean use_default)
{
	static GIcon *default_file_icon = NULL;
	static gboolean default_file_icon_tried = FALSE;

	if (doc->file_type && doc->file_type->icon)
		return doc->file_type->icon;

	if (use_default)
	{
		if (default_file_icon == NULL && ! default_file_icon_tried)
		{
			default_file_icon = ui_get_mime_icon("text/plain");
			default_file_icon_tried = TRUE;
		}

		return default_file_icon;
	}

	return NULL;
}

static GIcon *get_directory_icon(void)
{
	static GIcon *dir_icon = NULL;
	static gboolean directory_icon_tried = FALSE;

	if (dir_icon == NULL && ! directory_icon_tried)
	{
		dir_icon = ui_get_mime_icon("inode/directory");
		directory_icon_tried = TRUE;
	}

	return dir_icon;
}

static GIcon *get_favorites_icon(void)
{
	static GIcon *favorites_icon = NULL;
	static gboolean favorites_icon_tried = FALSE;

	if (favorites_icon == NULL && ! favorites_icon_tried)
	{
		if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), "folder-favorites"))
			favorites_icon = g_themed_icon_new("folder-favorites");

		if (favorites_icon == NULL)
			favorites_icon = get_directory_icon();

		favorites_icon_tried = TRUE;
	}

	return favorites_icon;
}

static GtkTreeIter *get_doc_parent(GeanyDocument *doc)
{
	g_return_val_if_fail(doc != NULL, NULL);

	if (!documents_show_paths)
		return NULL;

	GtkTreeIter parent;
	GtkTreeModel *model = GTK_TREE_MODEL(store_openfiles);
	gchar *path, *stored_path = NULL, *stored_folder = NULL;
	gchar *folder = sidebar_get_doc_folder(doc, &path);
	gboolean parent_found = FALSE;

	if (gtk_tree_model_get_iter_first(model, &parent))
	{
		do
		{
			stored_folder = stored_path = NULL;
			gtk_tree_model_get(model, &parent, DOCUMENTS_FILENAME, &stored_path,
					DOCUMENTS_SHORTNAME, &stored_folder, -1);

			if (utils_filenamecmp(stored_path, path) == 0)
				if (utils_filenamecmp(stored_folder, folder) == 0)
					parent_found = TRUE;

			g_free(stored_folder);
			g_free(stored_path);
		}
		while (!parent_found && gtk_tree_model_iter_next(model, &parent));
	}

	if (!parent_found)
	{
		gtk_tree_store_append(store_openfiles, &parent, NULL);
		gtk_tree_store_set(store_openfiles, &parent, DOCUMENTS_TYPE, TYPE_FOLDER,
				DOCUMENTS_ICON, get_directory_icon(), DOCUMENTS_FILENAME, path,
				DOCUMENTS_SHORTNAME, folder, -1);
	}

	g_free(folder);
	g_free(path);
	return gtk_tree_iter_copy(&parent);
}

static void openfiles_update_entry_values(GeanyDocument *doc, GtkTreeIter *iter, gboolean partial)
{
	gboolean use_default_file_icon = partial ? FALSE : TRUE;
	GIcon *icon = get_file_icon(doc, use_default_file_icon);
	const GdkColor *color = document_get_status_color(doc);
	gint weight = ui_document_label_font_weight(doc);

	if (partial)
	{
		gtk_tree_store_set(store_openfiles, iter, DOCUMENTS_COLOR, color,
				DOCUMENTS_WEIGHT, weight, -1);

		if (icon)
			gtk_tree_store_set(store_openfiles, iter, DOCUMENTS_ICON, icon, -1);
	}
	else
	{
		gchar *basename = g_path_get_basename(DOC_FILENAME(doc));

		gtk_tree_store_set(store_openfiles, iter, DOCUMENTS_TYPE, TYPE_FILE, DOCUMENTS_ICON, icon,
				DOCUMENTS_SHORTNAME, basename, DOCUMENTS_DOCUMENT, doc, DOCUMENTS_COLOR, color,
				DOCUMENTS_FILENAME, DOC_FILENAME(doc), DOCUMENTS_WEIGHT, weight, -1);

		g_free(basename);
	}
}

static void expand_row(GtkTreeIter *iter)
{
	GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(store_openfiles), iter);
	gtk_tree_view_expand_row(GTK_TREE_VIEW(tv.tree_openfiles), path, TRUE);
	gtk_tree_path_free(path);
}

static gboolean openfiles_has_no_child(GtkTreeIter *iter)
{
	return gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store_openfiles), iter) == 0;
}

static gboolean openfiles_has_one_child(GtkTreeIter *iter)
{
	return gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store_openfiles), iter) == 1;
}

static void openfiles_remove_favorite_entry(GeanyDocument *doc)
{
	GtkTreeIter *iter = &doc->priv->iter_favorite;

	g_return_if_fail(iter_favorites.stamp != 0);
	g_return_if_fail(gtk_tree_store_is_ancestor(store_openfiles, &iter_favorites, iter));

	gtk_tree_store_remove(store_openfiles, iter);
	iter->stamp = 0;

	if (openfiles_has_no_child(&iter_favorites))
	{
		gtk_tree_store_remove(store_openfiles, &iter_favorites);
		iter_favorites.stamp = 0;
	}
}

static void openfiles_update_favorite_entry(GeanyDocument *doc)
{
	gboolean favorite = document_get_favorite(doc);
	GtkTreeIter *iter = &doc->priv->iter_favorite;

	if (favorite)
	{
		if (iter->stamp)
			openfiles_update_entry_values(doc, iter, TRUE);
		else
		{
			gboolean new_folder = iter_favorites.stamp == 0;

			if (new_folder)
			{
				GIcon *favorites_icon = get_favorites_icon();

				gtk_tree_store_append(store_openfiles, &iter_favorites, NULL);
				gtk_tree_store_set(store_openfiles, &iter_favorites,
						DOCUMENTS_TYPE, TYPE_FAVORITES_FOLDER, DOCUMENTS_ICON, favorites_icon,
						DOCUMENTS_FILENAME, "Favorites", DOCUMENTS_SHORTNAME, "Favorites", -1);
				g_return_if_fail(iter_favorites.stamp != 0);
			}

			gtk_tree_store_append(store_openfiles, iter, &iter_favorites);
			g_return_if_fail(iter->stamp != 0);
			openfiles_update_entry_values(doc, iter, FALSE);

			if (new_folder)
				expand_row(&iter_favorites);

			ui_update_tab_status(doc);
			ui_document_show_hide(doc);
		}
	}
	else if (iter->stamp)
	{
		openfiles_remove_favorite_entry(doc);
		ui_update_tab_status(doc);
		ui_document_show_hide(doc);
	}
}

/* Also sets doc->priv->iter.
 * This is called recursively in sidebar_openfiles_update_all(). */
void sidebar_openfiles_add(GeanyDocument *doc)
{
	g_return_if_fail(doc != NULL);

	GtkTreeIter *parent = get_doc_parent(doc);
	GtkTreeIter *iter = &doc->priv->iter;

	gtk_tree_store_append(store_openfiles, iter, parent);

	/* Expand parent if new */
	if (parent && openfiles_has_one_child(parent))
		expand_row(parent);

	openfiles_update_entry_values(doc, iter, FALSE);
	openfiles_update_favorite_entry(doc);
	gtk_tree_iter_free(parent);
}

static void openfiles_remove(GeanyDocument *doc)
{
	g_return_if_fail(doc != NULL);

	GtkTreeIter parent;
	gtk_tree_model_iter_parent(GTK_TREE_MODEL(store_openfiles), &parent, &doc->priv->iter);

	gtk_tree_store_remove(store_openfiles, &doc->priv->iter);

	if (openfiles_has_no_child(&parent))
		gtk_tree_store_remove(store_openfiles, &parent);

	if (doc->priv->iter_favorite.stamp != 0)
		openfiles_remove_favorite_entry(doc);
}

static void openfiles_scroll_to_row(GtkTreeIter *iter)
{
	GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(store_openfiles), iter);
	g_return_if_fail(path != NULL);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(tv.tree_openfiles), path, NULL, FALSE, 0, 0);
	gtk_tree_path_free(path);
}

static void openfiles_remove_and_readd(GeanyDocument *doc)
{
	GtkTreeSelection *treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));

	gboolean sel = gtk_tree_selection_iter_is_selected(treesel, &doc->priv->iter);
	gboolean sel_favorite = document_get_favorite(doc) &&
			gtk_tree_selection_iter_is_selected(treesel, &doc->priv->iter_favorite);

	openfiles_remove(doc);
	sidebar_openfiles_add(doc);

	if (sel)
	{
		gtk_tree_selection_select_iter(treesel, &doc->priv->iter);

		GtkTreeIter parent;
		gtk_tree_model_iter_parent(GTK_TREE_MODEL(store_openfiles), &parent, &doc->priv->iter);

		if (openfiles_has_one_child(&parent))
			openfiles_scroll_to_row(&parent);
	}
	else if (sel_favorite)
	{
		gtk_tree_selection_select_iter(treesel, &doc->priv->iter_favorite);

		if (openfiles_has_one_child(&iter_favorites))
			openfiles_scroll_to_row(&iter_favorites);
	}
}

void sidebar_openfiles_update(GeanyDocument *doc)
{
	g_return_if_fail(doc != NULL);

	GtkTreeIter *iter = &doc->priv->iter;
	gchar *fname = NULL;

	gtk_tree_model_get(GTK_TREE_MODEL(store_openfiles), iter, DOCUMENTS_FILENAME, &fname, -1);

	if (utils_str_equal(fname, DOC_FILENAME(doc)))
	{
		openfiles_update_entry_values(doc, iter, TRUE);
		openfiles_update_favorite_entry(doc);
	}
	else
		/* Path has changed, so remove and re-add */
		openfiles_remove_and_readd(doc);

	g_free(fname);
}

void sidebar_openfiles_update_all(void)
{
	guint i;

	gtk_tree_store_clear(store_openfiles);
	iter_favorites.stamp = 0;

	foreach_document (i)
	{
		GeanyDocument *doc = documents[i];
		doc->priv->iter_favorite.stamp = 0;
		sidebar_openfiles_add(doc);
	}
}

void sidebar_remove_document(GeanyDocument *doc)
{
	openfiles_remove(doc);

	if (GTK_IS_WIDGET(doc->priv->tag_tree))
	{
		gtk_widget_destroy(doc->priv->tag_tree); /* make GTK release its references, if any */
		/* Because it was ref'd in sidebar_update_tag_list, it needs unref'ing */
		g_object_unref(doc->priv->tag_tree);
		doc->priv->tag_tree = NULL;
	}
}

static void on_hide_sidebar(void)
{
	ui_prefs.sidebar_visible = FALSE;
	ui_sidebar_show_hide();
}

static gboolean on_sidebar_display_symbol_list_show(GtkWidget *item)
{
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
		interface_prefs.sidebar_symbol_visible);
	return FALSE;
}

static gboolean on_sidebar_display_open_files_show(GtkWidget *item)
{
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
		interface_prefs.sidebar_openfiles_visible);
	return FALSE;
}

void sidebar_add_common_menu_items(GtkMenu *menu)
{
	GtkWidget *item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_check_menu_item_new_with_mnemonic(_("Show S_ymbol List"));
	gtk_container_add(GTK_CONTAINER(menu), item);
#if GTK_CHECK_VERSION(3, 0, 0)
	g_signal_connect(item, "draw", G_CALLBACK(on_sidebar_display_symbol_list_show), NULL);
#else
	g_signal_connect(item, "expose-event",
			G_CALLBACK(on_sidebar_display_symbol_list_show), NULL);
#endif
	gtk_widget_show(item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_list_symbol_activate), NULL);

	item = gtk_check_menu_item_new_with_mnemonic(_("Show _Document List"));
	gtk_container_add(GTK_CONTAINER(menu), item);
#if GTK_CHECK_VERSION(3, 0, 0)
	g_signal_connect(item, "draw", G_CALLBACK(on_sidebar_display_open_files_show), NULL);
#else
	g_signal_connect(item, "expose-event",
			G_CALLBACK(on_sidebar_display_open_files_show), NULL);
#endif
	gtk_widget_show(item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_list_document_activate), NULL);

	item = gtk_image_menu_item_new_with_mnemonic(_("H_ide Sidebar"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_hide_sidebar), NULL);
}

static void on_openfiles_show_paths_activate(GtkCheckMenuItem *item, gpointer user_data)
{
	documents_show_paths = gtk_check_menu_item_get_active(item);
	sidebar_openfiles_update_all();
}

static void on_list_document_activate(GtkCheckMenuItem *item, gpointer user_data)
{
	interface_prefs.sidebar_openfiles_visible = gtk_check_menu_item_get_active(item);
	ui_sidebar_show_hide();
	sidebar_tabs_show_hide(GTK_NOTEBOOK(main_widgets.sidebar_notebook), NULL, 0, NULL);
}

static void on_list_symbol_activate(GtkCheckMenuItem *item, gpointer user_data)
{
	interface_prefs.sidebar_symbol_visible = gtk_check_menu_item_get_active(item);
	ui_sidebar_show_hide();
	sidebar_tabs_show_hide(GTK_NOTEBOOK(main_widgets.sidebar_notebook), NULL, 0, NULL);
}

static void on_find_in_files(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkTreeSelection *treesel;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GeanyDocument *doc = NULL;
	gchar *dir = NULL;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));
	if (!gtk_tree_selection_get_selected(treesel, &model, &iter))
		return;
	gtk_tree_model_get(model, &iter, DOCUMENTS_DOCUMENT, &doc, -1);

	if (!doc)
	{
		gtk_tree_model_get(model, &iter, DOCUMENTS_FILENAME, &dir, -1);
	}
	else
		dir = g_path_get_dirname(DOC_FILENAME(doc));

	search_show_find_in_files_dialog(dir);
	g_free(dir);
}

static void on_openfiles_expand_collapse(GtkMenuItem *menuitem, gpointer user_data)
{
	gboolean expand = GPOINTER_TO_INT(user_data);

	if (expand)
		gtk_tree_view_expand_all(GTK_TREE_VIEW(tv.tree_openfiles));
	else
		gtk_tree_view_collapse_all(GTK_TREE_VIEW(tv.tree_openfiles));
}

static void create_openfiles_popup_menu(void)
{
	GtkWidget *item;

	openfiles_popup_menu = gtk_menu_new();

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_CLOSE, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_REMOVE));
	doc_items.close = item;

	item = gtk_image_menu_item_new_with_mnemonic(_("Close Recursi_vely"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
			gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_REMOVE_RECURSIVE));
	doc_items.close_recursively = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_NEW, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_NEW));
	doc_items.new = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_OPEN, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_OPEN));
	doc_items.open = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Open Directory"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
			gtk_image_new_from_stock(GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_OPEN_DIR));
	doc_items.open_dir = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_SAVE, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_SAVE));
	doc_items.save = item;

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_SAVE_AS, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_SAVE_AS));
	doc_items.save_as = item;

	item = gtk_image_menu_item_new_with_mnemonic(_("_Reload"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock(GTK_STOCK_REVERT_TO_SAVED, GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_RELOAD));
	doc_items.reload = item;

	item = gtk_image_menu_item_new_with_mnemonic(_("R_ename"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
			gtk_image_new_from_stock(GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_RENAME));
	doc_items.rename = item;

	item = gtk_image_menu_item_new_with_mnemonic(_("_Clone"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
			gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_CLONE));
	doc_items.clone = item;

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_DELETE));
	doc_items.delete = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);

	item = ui_image_menu_item_new(GTK_STOCK_FIND, _("_Find in Files..."));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_find_in_files), NULL);
	doc_items.find_in_files = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);

	doc_items.favorite = gtk_check_menu_item_new_with_mnemonic(_("Fa_vorite"));
	gtk_widget_show(doc_items.favorite);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), doc_items.favorite);
	g_signal_connect(doc_items.favorite, "activate", G_CALLBACK(on_openfiles_document_action),
			GINT_TO_POINTER(OPENFILES_ACTION_FAVORITE));

	item = gtk_image_menu_item_new_with_mnemonic(_("Clear Favori_tes"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
			gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_openfiles_document_action),
			GINT_TO_POINTER(OPENFILES_ACTION_CLEAR_FAVORITES));
	doc_items.clear_favorites = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);

	doc_items.readonly = gtk_check_menu_item_new_with_mnemonic(_("Read _Only"));
	gtk_widget_show(doc_items.readonly);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), doc_items.readonly);
	g_signal_connect(doc_items.readonly, "activate", G_CALLBACK(on_openfiles_document_action),
			GINT_TO_POINTER(OPENFILES_ACTION_READONLY));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);

	doc_items.show_paths = gtk_check_menu_item_new_with_mnemonic(_("Show _Paths"));
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(doc_items.show_paths), documents_show_paths);
	gtk_widget_show(doc_items.show_paths);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), doc_items.show_paths);
	g_signal_connect(doc_items.show_paths, "activate",
			G_CALLBACK(on_openfiles_show_paths_activate), NULL);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), item);

	doc_items.expand_all = ui_image_menu_item_new(GTK_STOCK_ADD, _("E_xpand All"));
	gtk_widget_show(doc_items.expand_all);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), doc_items.expand_all);
	g_signal_connect(doc_items.expand_all, "activate",
					 G_CALLBACK(on_openfiles_expand_collapse), GINT_TO_POINTER(TRUE));

	doc_items.collapse_all = ui_image_menu_item_new(GTK_STOCK_REMOVE, _("_Collapse All"));
	gtk_widget_show(doc_items.collapse_all);
	gtk_container_add(GTK_CONTAINER(openfiles_popup_menu), doc_items.collapse_all);
	g_signal_connect(doc_items.collapse_all, "activate",
					 G_CALLBACK(on_openfiles_expand_collapse), GINT_TO_POINTER(FALSE));

	sidebar_add_common_menu_items(GTK_MENU(openfiles_popup_menu));
}

static void unfold_parent(GtkTreeIter *iter)
{
	GtkTreeIter parent;

	if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(store_openfiles), &parent, iter))
		expand_row(&parent);
}

/* compares the given data with the doc pointer from the selected row of openfiles
 * treeview, in case of a match the row is selected and TRUE is returned
 * (called indirectly from gtk_tree_model_foreach()) */
static gboolean tree_model_find_node(GtkTreeModel *model, GtkTreePath *path,
		GtkTreeIter *iter, gpointer data)
{
	GeanyDocument *doc = NULL;

	gtk_tree_model_get(GTK_TREE_MODEL(store_openfiles), iter, DOCUMENTS_DOCUMENT, &doc, -1);

	if (doc == data)
	{
		/* unfolding also prevents a strange bug where the selection gets stuck on the parent
		 * when it is collapsed and then switching documents */
		unfold_parent(iter);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(tv.tree_openfiles), path, NULL, FALSE);
		return TRUE;
	}
	else return FALSE;
}

static gboolean node_selected(GeanyDocument *doc)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));
	GtkTreeIter iter;
	GtkTreeModel *model;
	GeanyDocument *selected;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		if (gtk_tree_model_get(model, &iter, DOCUMENTS_DOCUMENT, &selected, -1), selected == doc)
			return TRUE;

	return FALSE;
}

void sidebar_select_openfiles_item(GeanyDocument *doc)
{
	if (! node_selected(doc))
		gtk_tree_model_foreach(GTK_TREE_MODEL(store_openfiles), tree_model_find_node, doc);
}

static void rename_file_inplace(GtkTreeIter *iter)
{
	g_return_if_fail(gtk_tree_store_iter_is_valid(store_openfiles, iter));

	GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(store_openfiles), iter);
	GtkTreeView *tree_view = GTK_TREE_VIEW(tv.tree_openfiles);
	GtkTreeViewColumn *column = gtk_tree_view_get_column(tree_view, 0);
	GList *renderers = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(column));
	GtkCellRenderer *renderer = g_list_nth_data(renderers, 1);

	g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
	gtk_tree_view_set_cursor_on_cell(tree_view, path, column, renderer, TRUE);
	gtk_tree_path_free(path);
	g_list_free(renderers);
}

gboolean rename_file_inplace_idle(gpointer data)
{
	GtkTreeIter *iter = data;

	rename_file_inplace(iter);
	gtk_tree_iter_free(iter);
	return G_SOURCE_REMOVE;
}

static void set_favorite(GeanyDocument *doc, gboolean favorite)
{
	g_return_if_fail(doc != NULL);

	if (document_get_favorite(doc) != favorite)
		document_set_favorite(doc, favorite, TRUE);
}

static void set_readonly(GeanyDocument *doc, gboolean readonly)
{
	g_return_if_fail(doc != NULL);

	if (doc->readonly != readonly)
	{
		doc->readonly = readonly;
		sidebar_openfiles_update(doc);
		ui_document_show_hide(doc);
		ui_update_tab_status(doc);
	}
}

/* callbacks */

static void document_action(GeanyDocument *doc, gint action)
{
	if (! DOC_VALID(doc))
		return;

	switch (action)
	{
		case OPENFILES_ACTION_REMOVE:
		{
			document_close(doc);
			break;
		}
		case OPENFILES_ACTION_CLEAR_FAVORITES:
		{
			set_favorite(doc, FALSE);
			break;
		}
		case OPENFILES_ACTION_SAVE:
		{
			document_save_file(doc, FALSE);
			break;
		}
		case OPENFILES_ACTION_SAVE_AS:
		{
			document_show_tab(doc);
			dialogs_show_save_as();
			break;
		}
		case OPENFILES_ACTION_RELOAD_NO_PROMPT:
		{
			if (doc->real_path)
				document_reload_force(doc, NULL);

			break;
		}
		case OPENFILES_ACTION_CLONE:
		{
			document_clone(doc);
			break;
		}
		case OPENFILES_ACTION_DELETE:
		{
			document_delete_prompt(doc);
			break;
		}
	}
}

#define foreach_tree_model_child(model, parent, child) \
	for (gint i = gtk_tree_model_iter_n_children(model, parent); \
			i > 0 && gtk_tree_model_iter_nth_child(model, child, parent, --i); )

static void document_action_recursive(GtkTreeModel *model, GtkTreeIter *parent, gint action)
{
	GtkTreeIter child;

	foreach_tree_model_child(model, parent, &child)
	{
		GeanyDocument *doc = NULL;
		gtk_tree_model_get(model, &child, DOCUMENTS_DOCUMENT, &doc, -1);
		document_action(doc, action);
	}
}

static FolderFavoriteStatus get_folder_favorite_status(GtkTreeModel *model, GtkTreeIter *folder)
{
	GtkTreeIter child;
	gint total_child = 0;
	gint total_favorited = 0;

	foreach_tree_model_child(model, folder, &child)
	{
		++total_child;

		GeanyDocument *doc = NULL;
		gtk_tree_model_get(model, &child, DOCUMENTS_DOCUMENT, &doc, -1);

		if (DOC_VALID(doc) && document_get_favorite(doc))
			++total_favorited;
	}

	return total_favorited == 0 ? FOLDER_FAVORITE_STATUS_INACTIVE :
			total_favorited == total_child ? FOLDER_FAVORITE_STATUS_ACTIVE :
			FOLDER_FAVORITE_STATUS_INCONSISTENT;
}

static FolderReadOnlyStatus get_folder_readonly_status(GtkTreeModel *model, GtkTreeIter *folder)
{
	GtkTreeIter child;
	gint total_child = 0;
	gint total_readonlies = 0;

	foreach_tree_model_child(model, folder, &child)
	{
		++total_child;

		GeanyDocument *doc = NULL;
		gtk_tree_model_get(model, &child, DOCUMENTS_DOCUMENT, &doc, -1);

		if (DOC_VALID(doc) && doc->readonly)
			++total_readonlies;
	}

	return total_readonlies == 0 ? FOLDER_READONLY_STATUS_INACTIVE :
			total_readonlies == total_child ? FOLDER_READONLY_STATUS_ACTIVE :
			FOLDER_READONLY_STATUS_INCONSISTENT;
}

static gboolean is_favorites_folder(GtkTreeModel *model, GtkTreeIter *iter)
{
	gint type;
	gtk_tree_model_get(model, iter, DOCUMENTS_TYPE, &type, -1);
	return type == TYPE_FAVORITES_FOLDER;
}

static void on_openfiles_document_action(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));
	GtkTreeModel *model;
	GeanyDocument *doc = NULL;
	gint action = GPOINTER_TO_INT(user_data);

	if (updating_menu_items)
		return;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, DOCUMENTS_DOCUMENT, &doc, -1);

		switch (action)
		{
			case OPENFILES_ACTION_NEW:
			{
				gchar *dir = NULL;

				if (doc)
				{
					if (doc->file_name)
						dir = g_path_get_dirname(doc->file_name);
				}
				else
					gtk_tree_model_get(model, &iter, DOCUMENTS_FILENAME, &dir, -1);

				if (!dir || !g_path_is_absolute(dir))
					if (app->project && !EMPTY(app->project->base_path))
						SETPTR(dir, g_strdup(app->project->base_path));

				if (dir && g_path_is_absolute(dir))
					document_new_file_in_dir(dir, NULL, NULL, NULL, TRUE);
				else
					document_new_file(NULL, NULL, NULL);

				g_free(dir);
				break;
			}
			case OPENFILES_ACTION_OPEN:
			{
				gchar *dir = NULL;

				if (doc)
				{
					if (doc->file_name)
						dir = g_path_get_dirname(doc->file_name);
				}
				else
					gtk_tree_model_get(model, &iter, DOCUMENTS_FILENAME, &dir, -1);

				if (!dir || !g_path_is_absolute(dir))
				{
					const gchar *tmp = utils_get_default_dir_utf8();
					SETPTR(dir, tmp ? g_strdup(tmp) : NULL);
				}

				if (dir && g_path_is_absolute(dir))
					dialogs_show_open_file(FALSE, dir);
				else
					dialogs_show_open_file(FALSE, NULL);

				g_free(dir);
				break;
			}
			case OPENFILES_ACTION_OPEN_DIR:
			{
				gchar *dir = NULL;

				if (doc)
					dir = document_get_dirname(doc);
				else
					gtk_tree_model_get(model, &iter, DOCUMENTS_FILENAME, &dir, -1);

				if (dir && g_path_is_absolute(dir))
					utils_open_local_path(dir);

				g_free(dir);
				break;
			}
			case OPENFILES_ACTION_FAVORITE:
			{
				gboolean active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(
							doc_items.favorite));
				gboolean inconsistent = gtk_check_menu_item_get_inconsistent(GTK_CHECK_MENU_ITEM(
							doc_items.favorite));

				if (doc)
					set_favorite(doc, active);
				else if (! is_favorites_folder(model, &iter))
				{
					GtkTreeIter child;

					foreach_tree_model_child(model, &iter, &child)
					{
						gtk_tree_model_get(model, &child, DOCUMENTS_DOCUMENT, &doc, -1);
						set_favorite(doc, active || inconsistent);
					}
				}

				break;
			}
			case OPENFILES_ACTION_CLEAR_FAVORITES:
			{
				if (documents_show_paths)
				{
					if (doc)
					{
						GtkTreeIter *parent = get_doc_parent(doc);

						if (parent)
						{
							document_action_recursive(model, parent, action);
							gtk_tree_iter_free(parent);
						}
					}
					else
						document_action_recursive(model, &iter, action);
				}
				else
				{
					guint i;

					foreach_document(i)
						document_set_favorite(documents[i], FALSE, TRUE);
				}

				break;
			}
			case OPENFILES_ACTION_READONLY:
			{
				gboolean active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(
							doc_items.readonly));
				gboolean inconsistent = gtk_check_menu_item_get_inconsistent(GTK_CHECK_MENU_ITEM(
							doc_items.readonly));

				if (doc)
					set_readonly(doc, active);
				else if (! is_favorites_folder(model, &iter))
				{
					GtkTreeIter child;

					foreach_tree_model_child(model, &iter, &child)
					{
						gtk_tree_model_get(model, &child, DOCUMENTS_DOCUMENT, &doc, -1);
						set_readonly(doc, active || inconsistent);
					}
				}

				break;
			}
			case OPENFILES_ACTION_REMOVE_RECURSIVE:
			{
				g_return_if_fail(doc == NULL);

				gchar *short_name_a = NULL;
				gtk_tree_model_get(model, &iter, DOCUMENTS_SHORTNAME, &short_name_a, -1);
				g_return_if_fail(short_name_a);

				gint len = strlen(short_name_a);

				foreach_tree_model_child(model, NULL, &iter)
				{
					gchar *short_name_b = NULL;
					gtk_tree_model_get(model, &iter, DOCUMENTS_SHORTNAME, &short_name_b, -1);

					if (short_name_b && g_ascii_strncasecmp(short_name_a, short_name_b, len) == 0 &&
							(short_name_b[len] == '\0' || short_name_b[len] == G_DIR_SEPARATOR))
						document_action_recursive(model, &iter, OPENFILES_ACTION_REMOVE);

					g_free(short_name_b);
				}

				g_free(short_name_a);
				break;
			}
			case OPENFILES_ACTION_RELOAD:
			{
				if (doc)
					document_reload_prompt(doc, NULL);
				else
				{
					gchar *short_name = NULL;
					gtk_tree_model_get(model, &iter, DOCUMENTS_SHORTNAME, &short_name, -1);

					if (short_name)
					{
						if (dialogs_show_question_full(NULL, _("_Reload"), GTK_STOCK_CANCEL,
								_("Any unsaved changes and undo history will be lost."),
								_("Reload all documents under '%s'?"), short_name))
							document_action_recursive(model, &iter,
									OPENFILES_ACTION_RELOAD_NO_PROMPT);

						g_free(short_name);
					}
				}

				break;
			}
			case OPENFILES_ACTION_RENAME:
			{
				g_return_if_fail(doc && doc->file_name);
				g_idle_add(rename_file_inplace_idle, gtk_tree_iter_copy(&iter));
				break;
			}
			default:
			{
				if (doc)
					document_action(doc, action);
				else
					document_action_recursive(model, &iter, action);
			}
		}
	}
}

static void change_focus_to_editor(GeanyDocument *doc, GtkWidget *source_widget)
{
	if (may_steal_focus)
		document_try_focus(doc, source_widget);
	may_steal_focus = FALSE;
}

static gboolean openfiles_go_to_selection(GtkTreeSelection *selection, guint keyval)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GeanyDocument *doc = NULL;

	/* use switch_notebook_page to ignore changing the notebook page because it is already done */
	if (gtk_tree_selection_get_selected(selection, &model, &iter) && ! ignore_callback)
	{
		gtk_tree_model_get(model, &iter, DOCUMENTS_DOCUMENT, &doc, -1);
		if (! doc)
			return FALSE;	/* parent */

		/* switch to the doc and grab the focus */
		document_show_tab(doc);
		if (keyval != GDK_space)
			change_focus_to_editor(doc, tv.tree_openfiles);
	}
	return FALSE;
}

static gboolean taglist_go_to_selection(GtkTreeSelection *selection, guint keyval, guint state)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gint line = 0;
	gboolean handled = TRUE;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		TMTag *tag = NULL;

		gtk_tree_model_get(model, &iter, SYMBOLS_COLUMN_TAG, &tag, -1);
		if (! tag)
			return FALSE;

		line = tag->line;
		if (line > 0)
		{
			GeanyDocument *doc = document_get_current();

			if (doc != NULL)
			{
				nav_goto_line(doc, line, nav_get_current_position(doc));
				state = keybindings_get_modifiers(state);
				if (keyval != GDK_space && ! (state & GEANY_PRIMARY_MOD_MASK))
					change_focus_to_editor(doc, NULL);
				else
					handled = FALSE;
			}
		}
		tm_tag_unref(tag);
	}
	return handled;
}

static gboolean sidebar_key_press_cb(GtkWidget *widget, GdkEventKey *event,
											 gpointer user_data)
{
	may_steal_focus = FALSE;
	if (ui_is_keyval_enter_or_return(event->keyval) || event->keyval == GDK_space)
	{
		GtkWidgetClass *widget_class = GTK_WIDGET_GET_CLASS(widget);
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		may_steal_focus = TRUE;

		/* force the TreeView handler to run before us for it to do its job (selection & stuff).
		 * doing so will prevent further handlers to be run in most cases, but the only one is our
		 * own, so guess it's fine. */
		if (widget_class->key_press_event)
			widget_class->key_press_event(widget, event);

		if (widget == tv.tree_openfiles) /* tag and doc list have separate handlers */
			openfiles_go_to_selection(selection, event->keyval);
		else
			taglist_go_to_selection(selection, event->keyval, event->state);

		return TRUE;
	}
	return FALSE;
}

static gboolean sidebar_button_press_cb(GtkWidget *widget, GdkEventButton *event,
		G_GNUC_UNUSED gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkWidgetClass *widget_class = GTK_WIDGET_GET_CLASS(widget);
	gboolean handled = FALSE;

	/* force the TreeView handler to run before us for it to do its job (selection & stuff).
	 * doing so will prevent further handlers to be run in most cases, but the only one is our own,
	 * so guess it's fine. */
	if (widget_class->button_press_event)
		handled = widget_class->button_press_event(widget, event);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	may_steal_focus = TRUE;

	if (event->type == GDK_2BUTTON_PRESS)
	{	/* double click on parent node(section) expands/collapses it */
		GtkTreeModel *model;
		GtkTreeIter iter;

		if (gtk_tree_selection_get_selected(selection, &model, &iter))
		{
			if (gtk_tree_model_iter_has_child(model, &iter))
			{
				GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

				if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
					gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
				else
					gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);

				gtk_tree_path_free(path);
				return TRUE;
			}
		}
	}
	else if (event->button == 1)
	{	/* allow reclicking of taglist treeview item */
		if (widget == tv.tree_openfiles)
		{
			openfiles_go_to_selection(selection, 0);
			handled = TRUE;
		}
		else
			handled = taglist_go_to_selection(selection, 0, event->state);
	}
	else if (event->button == 2)
	{
		if (widget == tv.tree_openfiles)
			on_openfiles_document_action(NULL, GINT_TO_POINTER(OPENFILES_ACTION_REMOVE));
	}
	else if (event->button == 3)
	{
		if (widget == tv.tree_openfiles)
		{
			if (!openfiles_popup_menu)
				create_openfiles_popup_menu();

			/* update menu item sensitivity */
			documents_menu_update(selection);
			gtk_menu_popup(GTK_MENU(openfiles_popup_menu), NULL, NULL, NULL, NULL,
					event->button, event->time);
		}
		else
		{
			gtk_menu_popup(GTK_MENU(tv.popup_taglist), NULL, NULL, NULL, NULL,
					event->button, event->time);
		}
		handled = TRUE;
	}
	return handled;
}

static void update_favorite_check_menu_item(GeanyDocument *doc, gboolean sel, GtkTreeModel *model,
		GtkTreeIter *iter)
{
	gboolean active = FALSE, inconsistent = FALSE, favorites_folder = FALSE;

	if (sel)
	{
		if (doc)
			active = document_get_favorite(doc);
		else
		{
			favorites_folder = is_favorites_folder(model, iter);

			if (! favorites_folder)
			{
				FolderFavoriteStatus status = get_folder_favorite_status(model, iter);
				active = status != FOLDER_FAVORITE_STATUS_INACTIVE;
				inconsistent = status == FOLDER_FAVORITE_STATUS_INCONSISTENT;
			}
		}
	}

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(doc_items.favorite), active);
	gtk_check_menu_item_set_inconsistent(GTK_CHECK_MENU_ITEM(doc_items.favorite), inconsistent);
	gtk_widget_set_sensitive(doc_items.favorite, ! favorites_folder);
}

static void update_readonly_check_menu_item(GeanyDocument *doc, gboolean sel, GtkTreeModel *model,
		GtkTreeIter *iter)
{
	gboolean active = FALSE, inconsistent = FALSE, favorites_folder = FALSE;

	if (sel)
	{
		if (doc)
			active = doc->readonly;
		else
		{
			favorites_folder = is_favorites_folder(model, iter);

			if (! favorites_folder)
			{
				FolderFavoriteStatus status = get_folder_readonly_status(model, iter);
				active = status != FOLDER_READONLY_STATUS_INACTIVE;
				inconsistent = status == FOLDER_READONLY_STATUS_INCONSISTENT;
			}
		}
	}

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(doc_items.readonly), active);
	gtk_check_menu_item_set_inconsistent(GTK_CHECK_MENU_ITEM(doc_items.readonly), inconsistent);
	gtk_widget_set_sensitive(doc_items.readonly, ! favorites_folder);
}

static void documents_menu_update(GtkTreeSelection *selection)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean sel, path;
	gchar *shortname = NULL;
	GeanyDocument *doc = NULL;
	gchar *filename = NULL;

	updating_menu_items = TRUE;

	/* maybe no selection e.g. if ctrl-click deselected */
	sel = gtk_tree_selection_get_selected(selection, &model, &iter);
	if (sel)
	{
		gtk_tree_model_get(model, &iter, DOCUMENTS_DOCUMENT, &doc, DOCUMENTS_SHORTNAME, &shortname,
				DOCUMENTS_FILENAME, &filename, -1);
	}

	/* can close all, save all (except shortname), but only reload individually ATM */
	gtk_widget_set_sensitive(doc_items.close, sel);
	gtk_widget_set_sensitive(doc_items.close_recursively, sel && !doc);
	gtk_widget_set_sensitive(doc_items.new, sel);
	gtk_widget_set_sensitive(doc_items.open, sel);
	gtk_widget_set_sensitive(doc_items.open_dir, doc ? document_has_dirname(doc) : filename && g_path_is_absolute(filename));
	update_favorite_check_menu_item(doc, sel, model, &iter);
	gtk_widget_set_sensitive(doc_items.clear_favorites, sel);
	update_readonly_check_menu_item(doc, sel, model, &iter);
	gtk_widget_set_sensitive(doc_items.save, doc ? doc->changed && doc->file_name && g_path_is_absolute(doc->file_name) : sel);
	gtk_widget_set_sensitive(doc_items.save_as, doc != NULL);
	gtk_widget_set_sensitive(doc_items.reload, sel && (!doc || doc->real_path));
	gtk_widget_set_sensitive(doc_items.rename, doc && doc->file_name);
	gtk_widget_set_sensitive(doc_items.clone, doc != NULL);
	gtk_widget_set_sensitive(doc_items.delete, doc && doc->real_path);
	gtk_widget_set_sensitive(doc_items.find_in_files, sel);
	g_free(shortname);
	g_free(filename);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(doc_items.show_paths), documents_show_paths);
	gtk_widget_set_sensitive(doc_items.expand_all, documents_show_paths);
	gtk_widget_set_sensitive(doc_items.collapse_all, documents_show_paths);

	updating_menu_items = FALSE;
}

static StashGroup *stash_group = NULL;

static void on_load_settings(void)
{
	tag_window = ui_lookup_widget(main_widgets.window, "scrolledwindow2");

	prepare_openfiles();

	/* note: ui_prefs.sidebar_page is reapplied after plugins are loaded */
	stash_group_display(stash_group, NULL);
	sidebar_tabs_show_hide(GTK_NOTEBOOK(main_widgets.sidebar_notebook), NULL, 0, NULL);
}

static void on_save_settings(void)
{
	stash_group_update(stash_group, NULL);
	sidebar_tabs_show_hide(GTK_NOTEBOOK(main_widgets.sidebar_notebook), NULL, 0, NULL);
}

static void on_sidebar_switch_page(GtkNotebook *notebook,
	gpointer page, guint page_num, gpointer user_data)
{
	if (page_num == TREEVIEW_SYMBOL)
		sidebar_update_tag_list(document_get_current(), FALSE);
}

void sidebar_init(void)
{
	StashGroup *group;

	group = stash_group_new(PACKAGE);
	stash_group_add_boolean(group, &documents_show_paths, "documents_show_paths", TRUE);
	stash_group_add_widget_property(group, &ui_prefs.sidebar_page, "sidebar_page", GINT_TO_POINTER(0),
		main_widgets.sidebar_notebook, "page", 0);
	configuration_add_pref_group(group, FALSE);
	stash_group = group;

	/* delay building documents treeview until sidebar font has been read */
	g_signal_connect(geany_object, "load-settings", on_load_settings, NULL);
	g_signal_connect(geany_object, "save-settings", on_save_settings, NULL);

	g_signal_connect(main_widgets.sidebar_notebook, "page-added",
		G_CALLBACK(sidebar_tabs_show_hide), NULL);
	g_signal_connect(main_widgets.sidebar_notebook, "page-removed",
		G_CALLBACK(sidebar_tabs_show_hide), NULL);
	/* tabs may have changed when sidebar is reshown */
	g_signal_connect(main_widgets.sidebar_notebook, "show",
		G_CALLBACK(sidebar_tabs_show_hide), NULL);
	g_signal_connect_after(main_widgets.sidebar_notebook, "switch-page",
		G_CALLBACK(on_sidebar_switch_page), NULL);

	sidebar_tabs_show_hide(GTK_NOTEBOOK(main_widgets.sidebar_notebook), NULL, 0, NULL);
}

#define WIDGET(w) w && GTK_IS_WIDGET(w)

void sidebar_finalize(void)
{
	if (WIDGET(tv.default_tag_tree))
	{
		gtk_widget_destroy(tv.default_tag_tree); /* make GTK release its references, if any... */
		g_object_unref(tv.default_tag_tree); /* ...and release our own */
	}
	if (WIDGET(tv.popup_taglist))
		gtk_widget_destroy(tv.popup_taglist);
	if (WIDGET(openfiles_popup_menu))
		gtk_widget_destroy(openfiles_popup_menu);
}

void sidebar_focus_openfiles_tab(void)
{
	if (ui_prefs.sidebar_visible && interface_prefs.sidebar_openfiles_visible)
	{
		GtkNotebook *notebook = GTK_NOTEBOOK(main_widgets.sidebar_notebook);

		gtk_notebook_set_current_page(notebook, TREEVIEW_OPENFILES);
		gtk_widget_grab_focus(tv.tree_openfiles);
	}
}

void sidebar_focus_symbols_tab(void)
{
	if (ui_prefs.sidebar_visible && interface_prefs.sidebar_symbol_visible)
	{
		GtkNotebook *notebook = GTK_NOTEBOOK(main_widgets.sidebar_notebook);
		GtkWidget *symbol_list_scrollwin = gtk_notebook_get_nth_page(notebook, TREEVIEW_SYMBOL);

		gtk_notebook_set_current_page(notebook, TREEVIEW_SYMBOL);
		gtk_widget_grab_focus(gtk_bin_get_child(GTK_BIN(symbol_list_scrollwin)));
	}
}

static void sidebar_tabs_show_hide(GtkNotebook *notebook, GtkWidget *child,
								   guint page_num, gpointer data)
{
	gint tabs = gtk_notebook_get_n_pages(notebook);

	if (interface_prefs.sidebar_symbol_visible == FALSE)
		tabs--;
	if (interface_prefs.sidebar_openfiles_visible == FALSE)
		tabs--;

	gtk_notebook_set_show_tabs(notebook, (tabs > 1));
}

static void on_openfiles_renamed(GtkCellRenderer *renderer, const gchar *path_string,
		const gchar *new_basename, gpointer user_data)
{
	GeanyDocument *doc = NULL;
	GtkTreeIter iter;

	g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);

	g_return_if_fail(new_basename != NULL);

	if (*new_basename == '\0')
		return;

	if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(store_openfiles), &iter, path_string))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(store_openfiles), &iter, DOCUMENTS_DOCUMENT, &doc, -1);

		if (doc && doc->file_name)
		{
			gchar *dirname = g_path_get_dirname(doc->file_name);
			gchar *new_file_name = g_strconcat(dirname, G_DIR_SEPARATOR_S, new_basename, NULL);

			if (strcmp(doc->file_name, new_file_name) != 0)
				if (document_rename_and_save(doc, new_file_name, TRUE) == FALSE)
					openfiles_remove_and_readd(doc);

			g_free(new_file_name);
			g_free(dirname);
		}
	}
}

void sidebar_rename_file_cb(G_GNUC_UNUSED guint key_id)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GeanyDocument *doc = NULL;

	if (ui_prefs.sidebar_visible && interface_prefs.sidebar_openfiles_visible)
	{
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));

		if (selection && gtk_tree_selection_get_selected(selection, &model, &iter))
		{
			gtk_tree_model_get(model, &iter, DOCUMENTS_DOCUMENT, &doc, -1);

			if (doc && doc->file_name || (doc = document_get_current()) && doc->file_name)
			{
				gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.sidebar_notebook),
						TREEVIEW_OPENFILES);

				rename_file_inplace(&iter);
			}
		}
	}
}

void sidebar_openfiles_scroll_to_row(GeanyDocument *doc)
{
	g_return_if_fail(tv.tree_openfiles != NULL);
	g_return_if_fail(store_openfiles != NULL);

	if (DOC_VALID(doc))
	{
		GtkTreeIter *iter = &doc->priv->iter;

		if (doc->priv->iter_favorite.stamp)
		{
			GtkTreeSelection *selection =
					gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));

			if (gtk_tree_selection_iter_is_selected(selection, &doc->priv->iter) == FALSE)
				iter = &doc->priv->iter_favorite;
		}

		openfiles_scroll_to_row(iter);
	}
}
