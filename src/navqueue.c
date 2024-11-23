/*
 *      navqueue.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Dave Moore <wrex006(at)gmail(dot)com>
 *      Copyright 2007-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 * Simple code navigation
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "nav.h"
#include "sciwrappers.h"

/**
 *  Adds old file position and new file position to the navqueue, then goes to the new position.
 *
 *  @param old_doc The document of the previous position, if set as invalid (@c NULL) then no old
 *         position is set
 *  @param new_doc The document of the new position, must be valid.
 *  @param line the line number of the new position. It is counted with 1 as the first line, not 0.
 *
 *  @return @c TRUE if the cursor has changed the position to @a line or @c FALSE otherwise.
 **/
GEANY_API_SYMBOL
gboolean navqueue_goto_line(GeanyDocument *old_doc, GeanyDocument *new_doc, gint line)
{
	g_return_val_if_fail(old_doc == NULL || old_doc->is_valid, FALSE);
	g_return_val_if_fail(DOC_VALID(new_doc), FALSE);
	g_return_val_if_fail(line >= 1, FALSE);

	NavPosition new_npos = nav_create_position_from_line(new_doc, line);
	NavPosition old_ndoc = nav_get_current_position(old_doc);

	return nav_goto_position(new_npos, old_ndoc);
}
