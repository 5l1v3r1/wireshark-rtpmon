/* packet-x509sat.c
 * Routines for X.509 Selected Attribute Types packet dissection
 *
 * $Id: packet-x509sat-template.c,v 1.2 2004/05/25 21:07:43 guy Exp $
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
#include <epan/conversation.h>

#include <stdio.h>
#include <string.h>

#include "packet-ber.h"
#include "packet-x509sat.h"
#include "packet-x509if.h"

#define PNAME  "X.509 Selected Attribute Types"
#define PSNAME "X509SAT"
#define PFNAME "x509sat"

/* Initialize the protocol and registered fields */
int proto_x509sat = -1;
int hf_x509sat_countryName = -1;
#include "packet-x509sat-hf.c"

/* Initialize the subtree pointers */
#include "packet-x509sat-ett.c"

#include "packet-x509sat-fn.c"


static void
dissect_x509sat_countryName_callback(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
	dissect_x509sat_CountryName(FALSE, tvb, 0, pinfo, tree, hf_x509sat_countryName);
}

/*--- proto_register_x509sat ----------------------------------------------*/
void proto_register_x509sat(void) {

  /* List of fields */
  static hf_register_info hf[] = {
    { &hf_x509sat_countryName,
      { "countryName", "x509sat.countryName",
        FT_STRING, BASE_NONE, NULL, 0,
        "Country Name", HFILL }},
#include "packet-x509sat-hfarr.c"
  };

  /* List of subtrees */
  static gint *ett[] = {
#include "packet-x509sat-ettarr.c"
  };

  /* Register protocol */
  proto_x509sat = proto_register_protocol(PNAME, PSNAME, PFNAME);

  /* Register fields and subtrees */
  proto_register_field_array(proto_x509sat, hf, array_length(hf));
  proto_register_subtree_array(ett, array_length(ett));

}


/*--- proto_reg_handoff_x509sat -------------------------------------------*/
void proto_reg_handoff_x509sat(void) {
	dissector_handle_t dissector_handle;

	dissector_handle=create_dissector_handle(dissect_x509sat_countryName_callback, proto_x509sat);
	dissector_add_string("ber.oid", "2.5.4.6", dissector_handle);
}

