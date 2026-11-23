/*
 *      goto_document.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2026 konsolebox <konsolebox@gmail.com>
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
 * Dialog for filtering and switching between open documents
 */

#include "geany.h"
#include "document.h"
#include "filetypes.h"
#include "ui_utils.h"
#include "utils.h"
#include "support.h"
#include <gdk/gdkkeysyms.h>

enum
{
	COL_ICON,
	COL_NAME,
	COL_PATH,
	COL_DOCUMENT,
	N_COLS
};

typedef struct
{
	GtkWidget *dialog;
	GtkWidget *entry;
	GtkWidget *treeview;
	GtkListStore *store;
	GtkTreeModelFilter *filter;
} GotoDocumentDialog;

static GotoDocumentDialog *gd = NULL;

static gboolean filter_visible_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(gd->entry));

	if (EMPTY(text))
		return TRUE;

	gchar **tokens, *name;
	tokens = g_str_tokenize_and_fold(text, NULL, NULL);
	gtk_tree_model_get(model, iter, COL_NAME, &name, -1);

	gboolean match = name && utils_tokens_match(name, tokens);
	g_free(name);
	g_strfreev(tokens);
	return match;
}

static void on_text_changed(GtkEntry *entry, gpointer user_data)
{
	const gchar *text = gtk_entry_get_text(entry);

	if (EMPTY(text))
		gtk_entry_set_icon_from_stock(entry, GTK_ENTRY_ICON_SECONDARY, NULL);
	else
	{
		gtk_entry_set_icon_from_stock(entry, GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_CLEAR);
		gtk_entry_set_icon_sensitive(entry, GTK_ENTRY_ICON_SECONDARY, TRUE);
	}

	gtk_tree_model_filter_refilter(gd->filter);

	GtkTreeIter iter;

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(gd->filter), &iter))
	{
		GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gd->treeview));
		gtk_tree_selection_select_iter(sel, &iter);
	}
}

static gboolean on_entry_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_Up)
	{
		GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gd->treeview));
		GtkTreeModel *model = GTK_TREE_MODEL(gd->filter);
		GtkTreeIter iter;

		if (gtk_tree_selection_get_selected(sel, &model, &iter))
		{
			GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

			if (event->keyval == GDK_KEY_Down)
				gtk_tree_path_next(path);
			else
				gtk_tree_path_prev(path);

			if (gtk_tree_model_get_iter(model, &iter, path))
			{
				gtk_tree_selection_select_iter(sel, &iter);
				gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(gd->treeview), path, NULL, FALSE, 0.0,
						0.0);
			}

			gtk_tree_path_free(path);
		}

		return TRUE;
	}

	return FALSE;
}

static void on_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
		GtkTreeViewColumn *column, gpointer user_data)
{
	gtk_dialog_response(GTK_DIALOG(gd->dialog), GTK_RESPONSE_OK);
}

static void on_icon_press(GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event,
		gpointer user_data)
{
	if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
		gtk_entry_set_text(entry, "");
}

static void goto_document_init(void)
{
	GtkWidget *content_area, *vbox, *scrolled;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	gd = g_new0(GotoDocumentDialog, 1);

	gd->dialog = gtk_dialog_new_with_buttons(_("Go to Document"),
			GTK_WINDOW(main_widgets.window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	gtk_window_set_default_size(GTK_WINDOW(gd->dialog), 500, 350);
	gtk_dialog_set_default_response(GTK_DIALOG(gd->dialog), GTK_RESPONSE_OK);

	content_area = gtk_dialog_get_content_area(GTK_DIALOG(gd->dialog));
	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
	gtk_box_pack_start(GTK_BOX(content_area), vbox, TRUE, TRUE, 0);

	gd->entry = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(gd->entry), TRUE);
	gtk_entry_set_icon_from_stock(GTK_ENTRY(gd->entry), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_FIND);
	gtk_entry_set_icon_sensitive(GTK_ENTRY(gd->entry), GTK_ENTRY_ICON_PRIMARY, FALSE);
	g_signal_connect(gd->entry, "icon-press", G_CALLBACK(on_icon_press), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), gd->entry, FALSE, FALSE, 0);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

	gd->treeview = gtk_tree_view_new();
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(gd->treeview), FALSE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(gd->treeview), FALSE);
	gtk_container_add(GTK_CONTAINER(scrolled), gd->treeview);

	gd->store = gtk_list_store_new(N_COLS, G_TYPE_ICON, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_POINTER);
	gd->filter = GTK_TREE_MODEL_FILTER(gtk_tree_model_filter_new(GTK_TREE_MODEL(gd->store), NULL));
	gtk_tree_model_filter_set_visible_func(gd->filter, filter_visible_func, NULL, NULL);
	gtk_tree_view_set_model(GTK_TREE_VIEW(gd->treeview), GTK_TREE_MODEL(gd->filter));

	column = gtk_tree_view_column_new();
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "gicon", COL_ICON);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", COL_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(gd->treeview), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_START, "sensitive", FALSE, NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Path"), renderer, "text", COL_PATH, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(gd->treeview), column);

	g_signal_connect(gd->entry, "changed", G_CALLBACK(on_text_changed), NULL);
	g_signal_connect(gd->entry, "key-press-event", G_CALLBACK(on_entry_key_press), NULL);
	g_signal_connect(gd->treeview, "row-activated", G_CALLBACK(on_row_activated), NULL);

	gtk_widget_show_all(vbox);
}

void show_goto_document_dialog(void)
{
	GeanyDocument *doc;
	GtkTreeIter iter;

	if (!gd)
		goto_document_init();

	gtk_list_store_clear(gd->store);

	foreach_ordered_document(doc)
	{
		gchar *basename = DOC_FILENAME(doc) ? g_path_get_basename(DOC_FILENAME(doc)) :
				g_strdup(document_get_basename_for_display(doc, -1));
		const gchar *full_path = DOC_FILENAME(doc) ? DOC_FILENAME(doc) : "";
		GIcon *icon = (doc->file_type && doc->file_type->icon) ? doc->file_type->icon : NULL;

		gtk_list_store_append(gd->store, &iter);
		gtk_list_store_set(gd->store, &iter, COL_ICON, icon, COL_NAME, basename,
				COL_PATH, full_path, COL_DOCUMENT, doc, -1);
		g_free(basename);
	}

	gtk_entry_set_text(GTK_ENTRY(gd->entry), "");
	gtk_widget_grab_focus(gd->entry);
	on_text_changed(GTK_ENTRY(gd->entry), NULL);

	if (gtk_dialog_run(GTK_DIALOG(gd->dialog)) == GTK_RESPONSE_OK)
	{
		GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gd->treeview));
		GtkTreeModel *model;

		if (gtk_tree_selection_get_selected(sel, &model, &iter))
		{
			GeanyDocument *selected_doc = NULL;

			gtk_tree_model_get(model, &iter, COL_DOCUMENT, &selected_doc, -1);

			if (DOC_VALID(selected_doc))
				document_show_tab(selected_doc);
		}
	}

	gtk_widget_hide(gd->dialog);
}
