/*
 *      nav.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Dave Moore <wrex006(at)gmail(dot)com>
 *      Copyright 2007-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 * Not so simple code navigation
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "nav.h"

#include "document.h"
#include "geanyobject.h"
#include "sciwrappers.h"
#include "toolbar.h"
#include "utils.h"
#include "ui_utils.h"

#include "gtkcompat.h"

static GList *nav_list = NULL;
static GList *nav_current = NULL;
static gint nav_count = 0;

static GtkAction *nav_buttons[2];
static GtkWidget *nav_menu_items[2];

typedef struct
{
	NavPosition npos;
	gboolean one_time;
} NavData;

#define NAV_POSITION(npos) \
	((NavPosition *) npos)

#define NAV_POSITION_VALID(npos) \
	(DOC_VALID(npos.doc) && npos.pos >= 0)

#define NAV_INVALID_POSITION \
	((NavPosition) { NULL, -1 })

#define NAV_LINK_DATA(link) \
	((NavData *) link->data)

#define NAV_LINK_POSITION(link) \
	NAV_LINK_DATA(link)->npos

#define NAV_LINK_ONE_TIME(link) \
	NAV_LINK_DATA(link)->one_time

#define NAV_CURRENT_NEXT \
	(nav_current ? nav_current->next : nav_list)

static void set_sensitive(gint index, gboolean sensitive)
{
	gtk_action_set_sensitive(nav_buttons[index], sensitive);
	gtk_widget_set_sensitive(nav_menu_items[index], sensitive);
}

static void update_buttons(void)
{
	set_sensitive(0, nav_current != NULL);
	set_sensitive(1, NAV_CURRENT_NEXT != NULL);
}

void nav_init(void)
{
	nav_buttons[0] = toolbar_get_action_by_name("NavBack");
	nav_buttons[1] = toolbar_get_action_by_name("NavFor");

	nav_menu_items[0] = ui_lookup_widget(main_widgets.window, "go_to_previous_location1");
	nav_menu_items[1] = ui_lookup_widget(main_widgets.window, "go_to_next_location1");

	set_sensitive(0, FALSE);
	set_sensitive(1, FALSE);
}

void nav_free(void)
{
	g_list_free_full(g_steal_pointer(&nav_list), g_free);
}

static GList *list_create_new_link(NavPosition npos, gboolean one_time)
{
	NavData *data = g_new0(NavData, 1);
	data->npos = npos;
	data->one_time = one_time;
	return g_list_append(NULL, data);
}

static void list_delete_link(GList *link)
{
	g_return_if_fail(link != NULL);

	if (link == nav_current)
		nav_current = link->prev;

	g_free(g_steal_pointer(&link->data));
	nav_list = g_list_delete_link(nav_list, link);
}

static void list_remove_tail(GList *link)
{
	if (link == NULL)
		return;
	if (link->prev)
		link->prev = link->prev->next = NULL;
	if (link == nav_list)
		nav_list = NULL;

	g_list_free_full(link, g_free);
}

NavPosition nav_create_position(GeanyDocument *doc, gint pos)
{
	return (NavPosition) { doc, pos };
}

NavPosition nav_create_position_from_line(GeanyDocument *doc, gint line)
{
	g_return_val_if_fail(DOC_VALID(doc), NAV_INVALID_POSITION);
	g_return_val_if_fail(line > 0, NAV_INVALID_POSITION);

	return nav_create_position(doc, sci_get_position_from_line(doc->editor->sci, line) - 1);
}

NavPosition nav_get_current_position(GeanyDocument *doc)
{
	if (doc == NULL)
		doc = document_get_current();

	g_return_val_if_fail(DOC_VALID(doc), NAV_INVALID_POSITION);

	return nav_create_position(doc, sci_get_current_position(doc->editor->sci));
}

static gboolean positions_match(NavPosition a, NavPosition b)
{
	return a.doc == b.doc && a.pos == b.pos;
}

static gboolean link_position_matches(GList *link, NavPosition npos)
{
	return link && positions_match(NAV_LINK_POSITION(link), npos);
}

static void add_new_position(NavPosition npos, gboolean one_time, gboolean discard_forward_links)
{
	if (nav_current && positions_match(NAV_LINK_POSITION(nav_current), npos))
		return;

	if (discard_forward_links)
	{
		list_remove_tail(NAV_CURRENT_NEXT);
		g_return_if_fail(NAV_CURRENT_NEXT == NULL);
	}

	GList *new_link = list_create_new_link(npos, one_time);
	nav_list = g_list_insert_before_link(nav_list, NAV_CURRENT_NEXT, new_link);
	nav_current = new_link;
}

static gboolean goto_position(NavPosition npos)
{
	return editor_goto_pos(npos.doc->editor, npos.pos, TRUE);
}

gboolean nav_goto_position(NavPosition new_npos, NavPosition old_npos)
{
	g_return_val_if_fail(NAV_POSITION_VALID(new_npos), FALSE);
	g_return_val_if_fail(NAV_POSITION_VALID(old_npos), FALSE);

	if (goto_position(new_npos))
	{
		add_new_position(old_npos, TRUE, TRUE);
		add_new_position(new_npos, FALSE, TRUE);
		update_buttons();
		return TRUE;
	}

	return FALSE;
}

gboolean nav_goto_line(GeanyDocument *doc, gint line, NavPosition old_npos)
{
	g_return_val_if_fail(DOC_VALID(doc), FALSE);
	g_return_val_if_fail(line > 0, FALSE);

	return nav_goto_position(nav_create_position_from_line(doc, line), old_npos);
}

static void goto_link(GList *target_link, NavPosition orig_npos, gboolean orig_npos_is_linked)
{
	if (goto_position(NAV_LINK_POSITION(target_link)))
	{
		if (orig_npos_is_linked == FALSE)
			add_new_position(orig_npos, TRUE, FALSE);

		nav_current = target_link;

		if (NAV_LINK_ONE_TIME(target_link))
			list_delete_link(target_link);
	}
	else
		list_delete_link(target_link);
}

static void go_back_internal(GList *target_link, NavPosition orig_npos,
		gboolean orig_npos_is_linked)
{
	if (positions_match(NAV_LINK_POSITION(target_link), orig_npos) == FALSE)
		goto_link(target_link, orig_npos, orig_npos_is_linked);
	else if (target_link->prev)
		go_back_internal(target_link->prev, orig_npos, TRUE);
}

void nav_go_back(void)
{
	if (nav_current == NULL)
		return;

	NavPosition orig_npos = nav_get_current_position(NULL);
	g_return_if_fail(NAV_POSITION_VALID(orig_npos));

	go_back_internal(nav_current, orig_npos, link_position_matches(nav_current->next, orig_npos));
	update_buttons();
}

static void go_forward_internal(GList *target_link, NavPosition orig_npos,
		gboolean orig_npos_is_linked)
{
	if (positions_match(NAV_LINK_POSITION(target_link), orig_npos) == FALSE)
		goto_link(target_link, orig_npos, orig_npos_is_linked);
	else if (target_link->next)
		go_forward_internal(target_link->next, orig_npos, TRUE);
}

void nav_go_forward(void)
{
	if (NAV_CURRENT_NEXT == NULL)
		return;

	NavPosition orig_npos = nav_get_current_position(NULL);
	g_return_if_fail(NAV_POSITION_VALID(orig_npos));

	go_forward_internal(NAV_CURRENT_NEXT, orig_npos, link_position_matches(nav_current, orig_npos));
	update_buttons();
}

void nav_remove_document(const GeanyDocument *doc)
{
	g_return_if_fail(doc != NULL);

	GList *link, *next;

	for (link = nav_list; link && (next = link->next, TRUE); link = next)
		if (NAV_LINK_POSITION(link).doc == doc)
			list_delete_link(link);

	update_buttons();
}
