/*
 *      nav.h - this file is part of Geany, a fast and lightweight IDE
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

/**
 *  @file nav.h
 * Not so simple code navigation
 **/

#ifndef GEANY_NAV_H
#define GEANY_NAV_H 1

#include "document.h"

#include <glib.h>

G_BEGIN_DECLS

#ifdef GEANY_PRIVATE

typedef struct
{
	GeanyDocument *doc;
	gint pos;
} NavPosition;

void nav_init(void);

void nav_free(void);

NavPosition nav_create_position(GeanyDocument *doc, gint pos);

NavPosition nav_create_position_from_line(GeanyDocument *doc, gint line);

NavPosition nav_get_current_position(GeanyDocument *doc);

gboolean nav_goto_position(NavPosition new_npos, NavPosition old_npos);

gboolean nav_goto_line(GeanyDocument *doc, gint line, NavPosition old_npos);

void nav_go_back(void);

void nav_go_forward(void);

void nav_remove_document(const GeanyDocument *doc);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_NAV_H */
