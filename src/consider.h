/*
 *      consider.h - this file is part of Geany, a fast and lightweight IDE
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

/**
 *  @file consider.h
 *  Entry points for queuing delayed procedures or immediately executing them
 **/

#ifndef GEANY_CONSIDER_H
#define GEANY_CONSIDER_H 1

void consider_handling_switch_page_after(gpointer page);
void consider_saving_session_files(void);
void consider_showing_document_tab(GeanyDocument *doc);
void consider_trying_document_focus(GeanyDocument *doc, GtkWidget *source_widget);
void consider_calling_ui_document_show_hide(GeanyDocument *doc);

#endif // GEANY_CONSIDER_H
