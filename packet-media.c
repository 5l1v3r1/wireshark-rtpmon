/* packet-media.c
 * Routines for displaying an undissected media type (default case),
 * based on the generic "data" dissector.
 *
 * (C) Olivier Biot, 2004
 *
 * $Id$
 *
 * Refer to the AUTHORS file or the AUTHORS section in the man page
 * for contacting the author(s) of this file.
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@ethereal.com>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glib.h>
#include <epan/packet.h>

/* proto_media cannot be static because it's referenced in the
 * print routines
 */
int proto_media = -1;

static void
dissect_media(tvbuff_t *tvb, packet_info *pinfo , proto_tree *tree)
{
    int bytes;

    /* Add media type to the INFO column if it is visible */
    if (check_col(pinfo->cinfo, COL_INFO)) {
	col_append_fstr(pinfo->cinfo, COL_INFO, " (%s)", pinfo->match_string);
    }

    if (tree) {
	bytes = tvb_length_remaining(tvb, 0);
	if (bytes > 0) {
	    if (pinfo->private_data) {
		/* The media type has parameters */
		proto_tree_add_protocol_format(tree, proto_media, tvb, 0, bytes,
		    "Media Type: %s; %s (%d byte%s)",
		    pinfo->match_string, (char *)pinfo->private_data,
		    bytes, plurality(bytes, "", "s"));
	    } else {
		/* The media type has no parameters */
		proto_tree_add_protocol_format(tree, proto_media, tvb, 0, bytes,
		    "Media Type: %s (%d byte%s)",
		    pinfo->match_string,
		    bytes, plurality(bytes, "", "s"));
	    }
	}
    }
}

void
proto_register_media(void)
{
    proto_media = proto_register_protocol (
	    "Media Type",	/* name */
	    "Media",		/* short name */
	    "media"		/* abbrev */
	    );
    register_dissector("media", dissect_media, proto_media);

    /*
     * "Media" is used to dissect something whose normal dissector
     * is disabled, so it cannot itself be disabled.
     */
    proto_set_cant_toggle(proto_media);
}
