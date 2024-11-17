/*
 *      consider.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2024 konsolebox <konsolebox@gmail.com>
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
 * Entry points for queuing delayed procedures or immediately executing them
 */

#include "document.h"
#include "keyfile.h"
#include "main.h"
#include "ui_utils.h"

typedef struct
{
	guint event_source_id;
} SaveSessionFilesData;

typedef struct
{
	guint event_source_id;
	GtkWidget *page;
} HandleSwitchPageAfterData;

typedef struct
{
	guint event_source_id;
	GeanyDocument *doc;
} ShowDocumentTabData;

typedef struct
{
	guint event_source_id;
	GeanyDocument *doc;
	GtkWidget *source_widget;
} TryDocumentFocusData;

typedef struct
{
	guint event_source_id;
	GeanyDocument *doc;
} CallUIDocumentShowHideData;

SaveSessionFilesData save_session_files_data = {0};
HandleSwitchPageAfterData handle_switch_page_after_data = {0};
ShowDocumentTabData show_document_tab_data = {0};
TryDocumentFocusData try_document_focus_data = {0};
CallUIDocumentShowHideData call_ui_document_show_hide_data = {0};

#define IDLE_ADD(f) g_idle_add(f, NULL);

static gboolean opening_multiple_files()
{
	return main_status.opening_files_recursively || main_status.opening_session_files ||
			main_status.reloading_all_files || main_status.opening_cl_files ||
			main_status.handling_input_filenames;
}

/*
 * Saving session files
 */

static void save_session_files(gboolean remove_event)
{
	if (save_session_files_data.event_source_id)
	{
		if (remove_event)
			g_source_remove(save_session_files_data.event_source_id);

		save_session_files_data.event_source_id = 0;
	}

	if (! main_status.quitting)
	{
		if (app->project != NULL)
			project_write_config();
		else
			configuration_save_default_session();
	}
}

static gboolean save_session_files_idle_inner(void)
{
	g_return_val_if_fail(cl_options.load_session, G_SOURCE_REMOVE);
	g_return_val_if_fail(! main_status.opening_session_files, G_SOURCE_REMOVE);
	g_return_val_if_fail(! main_status.closing_all, G_SOURCE_REMOVE);
	return ! main_status.quitting && opening_multiple_files() ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

static gboolean save_session_files_idle(G_GNUC_UNUSED gpointer data)
{
	gboolean result = save_session_files_idle_inner();

	if (result == G_SOURCE_REMOVE)
		save_session_files(FALSE);

	return result;
}

void consider_saving_session_files(void)
{
	if (cl_options.load_session && ! main_status.quitting && ! main_status.opening_session_files &&
			! main_status.closing_all)
	{
		if (opening_multiple_files())
		{
			if (save_session_files_data.event_source_id == 0)
				save_session_files_data.event_source_id = IDLE_ADD(save_session_files_idle);
		}
		else
			save_session_files(TRUE);
	}
}

/*
 * Switch-page-after event handling
 */

static void handle_switch_page_after(GtkWidget *page, gboolean remove_event)
{
	if (handle_switch_page_after_data.event_source_id)
	{
		if (remove_event)
			g_source_remove(handle_switch_page_after_data.event_source_id);

		handle_switch_page_after_data.event_source_id = 0;
	}

	/* Guard against the unlikely case where we didn't run yet but are already
	 * closing all documents */
	if (! main_status.closing_all)
		document_handle_switch_page_after(page);
}

static gboolean handle_switch_page_after_idle(G_GNUC_UNUSED gpointer data)
{
	if (opening_multiple_files() || show_document_tab_data.event_source_id)
		return G_SOURCE_CONTINUE;

	handle_switch_page_after(handle_switch_page_after_data.page, FALSE);
	return G_SOURCE_REMOVE;
}

void consider_handling_switch_page_after(gpointer page)
{
	if (opening_multiple_files() || show_document_tab_data.event_source_id)
	{
		handle_switch_page_after_data.page = page;

		/* Delay the handling */
		if (handle_switch_page_after_data.event_source_id == 0)
			handle_switch_page_after_data.event_source_id = IDLE_ADD(handle_switch_page_after_idle);
	}
	else
		handle_switch_page_after(page, TRUE);
}

/*
 * Showing document tab
 */

static void show_document_tab(GeanyDocument *doc, gboolean remove_event)
{
	if (show_document_tab_data.event_source_id)
	{
		if (remove_event)
			g_source_remove(show_document_tab_data.event_source_id);

		show_document_tab_data.event_source_id = 0;
	}

	if (! main_status.closing_all)
		document_show_tab_real(doc);
}

static gboolean show_document_tab_idle(G_GNUC_UNUSED gpointer data)
{
	if (opening_multiple_files())
		return G_SOURCE_CONTINUE;

	show_document_tab(show_document_tab_data.doc, FALSE);
	return G_SOURCE_REMOVE;
}

void consider_showing_document_tab(GeanyDocument *doc)
{
	if (opening_multiple_files() && handle_switch_page_after_data.event_source_id)
	{
		show_document_tab_data.doc = doc;

		if (show_document_tab_data.event_source_id == 0)
			show_document_tab_data.event_source_id = IDLE_ADD(show_document_tab_idle);
	}
	else
		show_document_tab(doc, TRUE);
}

/*
 * Document focus
 */

static gboolean try_document_focus_idle(G_GNUC_UNUSED gpointer data)
{
	if (show_document_tab_data.event_source_id || handle_switch_page_after_data.event_source_id)
		return G_SOURCE_CONTINUE;

	try_document_focus_data.event_source_id = 0;

	if (! main_status.closing_all)
		document_try_focus_real(try_document_focus_data.doc, try_document_focus_data.source_widget);

	return G_SOURCE_REMOVE;
}

void consider_trying_document_focus(GeanyDocument *doc, GtkWidget *source_widget)
{
	try_document_focus_data.doc = doc;
	try_document_focus_data.source_widget = source_widget;

	if (try_document_focus_data.event_source_id == 0)
		try_document_focus_data.event_source_id = IDLE_ADD(try_document_focus_idle);
}

/*
 * ui_document_show_hide
 */

static gboolean call_ui_document_show_hide_idle(G_GNUC_UNUSED gpointer data)
{
	if (opening_multiple_files() || show_document_tab_data.event_source_id ||
			handle_switch_page_after_data.event_source_id ||
			try_document_focus_data.event_source_id)
		return G_SOURCE_CONTINUE;

	call_ui_document_show_hide_data.event_source_id = 0;

	if (! main_status.closing_all)
		ui_document_show_hide_real(call_ui_document_show_hide_data.doc);

	return G_SOURCE_REMOVE;
}

void consider_calling_ui_document_show_hide(GeanyDocument *doc)
{
	call_ui_document_show_hide_data.doc = doc;

	if (call_ui_document_show_hide_data.event_source_id == 0)
		call_ui_document_show_hide_data.event_source_id = IDLE_ADD(call_ui_document_show_hide_idle);
}
