/* packet-gre.c
 * Routines for the Generic Routing Encapsulation (GRE) protocol
 * Brad Robel-Forrest <brad.robel-forrest@watchguard.com>
 *
 * $Id: packet-gre.c,v 1.33 2001/01/03 06:55:28 guy Exp $
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@zing.org>
 * Copyright 1998 Gerald Combs
 *
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

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <glib.h>
#include "packet.h"
#include "packet-ip.h"
#include "packet-ipx.h"
#include "packet-wccp.h"
#include "in_cksum.h"

static int proto_gre = -1;
static int hf_gre_proto = -1;

static gint ett_gre = -1;
static gint ett_gre_flags = -1;
static gint ett_gre_wccp2_redirect_header = -1;

/* bit positions for flags in header */
#define GH_B_C		0x8000
#define GH_B_R		0x4000
#define GH_B_K		0x2000
#define GH_B_S		0x1000
#define GH_B_s		0x0800
#define GH_B_RECUR	0x0700
#define GH_P_A		0x0080	/* only in special PPTPized GRE header */
#define GH_P_FLAGS	0x0078	/* only in special PPTPized GRE header */
#define GH_R_FLAGS	0x00F8
#define GH_B_VER	0x0007

#define GRE_PPP		0x880B
#define	GRE_IP		0x0800
#define GRE_WCCP	0x883E
#define GRE_IPX		0x8137

static void add_flags_and_ver(proto_tree *, guint16, tvbuff_t *, int, int);
static void dissect_gre_wccp2_redirect_header(tvbuff_t *, int, proto_tree *);

static const value_string typevals[] = {
	{ GRE_PPP,  "PPP" },
	{ GRE_IP,   "IP" },
	{ GRE_WCCP, "WCCP"},
	{ GRE_IPX,  "IPX"},
	{ 0,        NULL  }
};

static dissector_handle_t ip_handle;
static dissector_handle_t ppp_handle;

static void
dissect_gre(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
  int		offset = 0;
  guint16	flags_and_ver;
  guint16	type;
  guint16	sre_af;
  guint8	sre_length;
  tvbuff_t	*next_tvb;

  CHECK_DISPLAY_AS_DATA(proto_gre, tvb, pinfo, tree);

  pinfo->current_proto = "GRE";

  flags_and_ver = tvb_get_ntohs(tvb, offset);
  type = tvb_get_ntohs(tvb, offset + sizeof(flags_and_ver));

  if (check_col(pinfo->fd, COL_PROTOCOL))
    col_set_str(pinfo->fd, COL_PROTOCOL, "GRE");

  if (check_col(pinfo->fd, COL_INFO)) {
    col_add_fstr(pinfo->fd, COL_INFO, "Encapsulated %s",
        val_to_str(type, typevals, "0x%04X (unknown)"));
  }

  if (tree) {
    gboolean		is_ppp = FALSE;
    gboolean		is_wccp2 = FALSE;
    proto_item *	ti;
    proto_tree *	gre_tree;
    guint 		len = 4;

    if (flags_and_ver & GH_B_C || flags_and_ver & GH_B_R)
      len += 4;
    if (flags_and_ver & GH_B_K)
      len += 4;
    if (flags_and_ver & GH_B_S)
      len += 4;
    switch (type) {

    case GRE_PPP:
      if (flags_and_ver & GH_P_A)
        len += 4;
      is_ppp = TRUE;
      break;

    case GRE_WCCP:
      /* WCCP2 puts an extra 4 octets into the header, but uses the same
         encapsulation type; if it looks as if the first octet of the packet
         isn't the beginning of an IPv4 header, assume it's WCCP2. */
      if ((tvb_get_guint8(tvb, offset + sizeof(flags_and_ver) + sizeof(type)) & 0xF0) != 0x40) {
	len += 4;
	is_wccp2 = TRUE;
      }
      break;
    }

    ti = proto_tree_add_protocol_format(tree, proto_gre, tvb, offset, len,
      "Generic Routing Encapsulation (%s)",
      val_to_str(type, typevals, "0x%04X - unknown"));
    gre_tree = proto_item_add_subtree(ti, ett_gre);
    add_flags_and_ver(gre_tree, flags_and_ver, tvb, offset, is_ppp);
    offset += sizeof(flags_and_ver);

    proto_tree_add_uint(gre_tree, hf_gre_proto, tvb, offset, sizeof(type), type);
    offset += sizeof(type);

    if (flags_and_ver & GH_B_C || flags_and_ver & GH_B_R) {
      guint length, reported_length;
      vec_t cksum_vec[1];
      guint16 cksum, computed_cksum;

      cksum = tvb_get_ntohs(tvb, offset);
      length = tvb_length(tvb);
      reported_length = tvb_reported_length(tvb);
      if ((flags_and_ver & GH_B_C) && !pinfo->fragmented
		&& length >= reported_length) {
	/* The Checksum Present bit is set, and the packet isn't part of a
	   fragmented datagram and isn't truncated, so we can checksum it. */

	cksum_vec[0].ptr = tvb_get_ptr(tvb, 0, reported_length);
	cksum_vec[0].len = reported_length;
	computed_cksum = in_cksum(cksum_vec, 1);
	if (computed_cksum == 0) {
	  proto_tree_add_text(gre_tree, tvb, offset, sizeof(cksum),
			"Checksum: 0x%04x (correct)", cksum);
	} else {
	  proto_tree_add_text(gre_tree, tvb, offset, sizeof(cksum),
			"Checksum: 0x%04x (incorrect, should be 0x%04x)",
			cksum, in_cksum_shouldbe(cksum, computed_cksum));
	}
      } else {
	proto_tree_add_text(gre_tree, tvb, offset, sizeof(cksum),
			  "Checksum: 0x%04x", cksum);
      }
      offset += sizeof(cksum);
    }
    
    if (flags_and_ver & GH_B_C || flags_and_ver & GH_B_R) {
      guint16 rtoffset = tvb_get_ntohs(tvb, offset);
      proto_tree_add_text(gre_tree, tvb, offset, sizeof(rtoffset),
			  "Offset: %u", rtoffset);
      offset += sizeof(rtoffset);
    }

    if (flags_and_ver & GH_B_K) {
      if (is_ppp) {
	guint16	paylen;
	guint16 callid;

	paylen = tvb_get_ntohs(tvb, offset);
	proto_tree_add_text(gre_tree, tvb, offset, sizeof(paylen),
			    "Payload length: %u", paylen);
	offset += sizeof(paylen);

	callid = tvb_get_ntohs(tvb, offset);
	proto_tree_add_text(gre_tree, tvb, offset, sizeof(callid),
			    "Call ID: %u", callid);
	offset += sizeof(callid);
      }
      else {
	guint32 key = tvb_get_ntohl(tvb, offset);
	proto_tree_add_text(gre_tree, tvb, offset, sizeof(key),
			    "Key: %u", key);
	offset += sizeof(key);
      }
    }
    
    if (flags_and_ver & GH_B_S) {
      guint32 seqnum = tvb_get_ntohl(tvb, offset);
      proto_tree_add_text(gre_tree, tvb, offset, sizeof(seqnum),
			  "Sequence number: %u", seqnum);
      offset += sizeof(seqnum);
    }

    if (is_ppp && flags_and_ver & GH_P_A) {
      guint32 acknum = tvb_get_ntohl(tvb, offset);
      proto_tree_add_text(gre_tree, tvb, offset, sizeof(acknum),
			  "Acknowledgement number: %u", acknum);
      offset += sizeof(acknum);
    }

    if (flags_and_ver & GH_B_R) {
      for (;;) {
      	sre_af = tvb_get_ntohs(tvb, offset);
        proto_tree_add_text(gre_tree, tvb, offset, sizeof(guint16),
  			  "Address family: %u", sre_af);
        offset += sizeof(guint16);
        proto_tree_add_text(gre_tree, tvb, offset, 1,
			  "SRE offset: %u", tvb_get_guint8(tvb, offset));
	offset += sizeof(guint8);
	sre_length = tvb_get_guint8(tvb, offset);
        proto_tree_add_text(gre_tree, tvb, offset, sizeof(guint8),
			  "SRE length: %u", sre_length);
	offset += sizeof(guint8);
	if (sre_af == 0 && sre_length == 0)
	  break;
	offset += sre_length;
      }
    }

    next_tvb = tvb_new_subset(tvb, offset, -1, -1);
    switch (type) {
      case GRE_PPP:
        call_dissector(ppp_handle, next_tvb, pinfo, tree);
 	break;
      case GRE_IP:
        call_dissector(ip_handle, next_tvb, pinfo, tree);
        break;
      case GRE_WCCP:
        if (is_wccp2) {
          dissect_gre_wccp2_redirect_header(tvb, offset, gre_tree);
          offset += 4;
        }
        call_dissector(ip_handle, next_tvb, pinfo, tree);
        break;
      case GRE_IPX:
        dissect_ipx(next_tvb, pinfo, tree);
        break;
      default:
	dissect_data(next_tvb, 0, pinfo, gre_tree);
	break;
    }
  }
}

static void
add_flags_and_ver(proto_tree *tree, guint16 flags_and_ver, tvbuff_t *tvb,
    int offset, int is_ppp)
{
  proto_item *	ti;
  proto_tree *	fv_tree;
  int		nbits = sizeof(flags_and_ver) * 8;
  
  ti = proto_tree_add_text(tree, tvb, offset, 2, 
			   "Flags and version: %#04x", flags_and_ver);
  fv_tree = proto_item_add_subtree(ti, ett_gre_flags);
  
  proto_tree_add_text(fv_tree, tvb, offset, sizeof(flags_and_ver), "%s",
		      decode_boolean_bitfield(flags_and_ver, GH_B_C, nbits,
					      "Checksum", "No checksum"));
  proto_tree_add_text(fv_tree, tvb, offset, sizeof(flags_and_ver), "%s",
		      decode_boolean_bitfield(flags_and_ver, GH_B_R, nbits,
					      "Routing", "No routing"));
  proto_tree_add_text(fv_tree, tvb, offset, sizeof(flags_and_ver), "%s",
		      decode_boolean_bitfield(flags_and_ver, GH_B_K, nbits,
					      "Key", "No key"));
  proto_tree_add_text(fv_tree, tvb, offset, sizeof(flags_and_ver), "%s",
		      decode_boolean_bitfield(flags_and_ver, GH_B_S, nbits,
					      "Sequence number", "No sequence number"));
  proto_tree_add_text(fv_tree, tvb, offset, sizeof(flags_and_ver), "%s",
		      decode_boolean_bitfield(flags_and_ver, GH_B_s, nbits,
					      "Strict source route", "No strict source route"));
  proto_tree_add_text(fv_tree, tvb, offset, sizeof(flags_and_ver), "%s",
		      decode_numeric_bitfield(flags_and_ver, GH_B_RECUR, nbits,
					      "Recursion control: %u"));
  if (is_ppp) {
    proto_tree_add_text(fv_tree, tvb, offset, sizeof(flags_and_ver), "%s",
			decode_boolean_bitfield(flags_and_ver, GH_P_A, nbits,
						"Acknowledgment number", "No acknowledgment number"));
    proto_tree_add_text(fv_tree, tvb, offset, sizeof(flags_and_ver), "%s",
			decode_numeric_bitfield(flags_and_ver, GH_P_FLAGS, nbits,
						"Flags: %u"));
  }
  else {
    proto_tree_add_text(fv_tree, tvb, offset, sizeof(flags_and_ver), "%s",
			decode_numeric_bitfield(flags_and_ver, GH_R_FLAGS, nbits,
						"Flags: %u"));
  }

  proto_tree_add_text(fv_tree, tvb, offset, sizeof(flags_and_ver), "%s",
		      decode_numeric_bitfield(flags_and_ver, GH_B_VER, nbits,
					      "Version: %u"));
 }

static void
dissect_gre_wccp2_redirect_header(tvbuff_t *tvb, int offset, proto_tree *tree)
{
  proto_item *	ti;
  proto_tree *	rh_tree;
  guint8	rh_flags;
  
  ti = proto_tree_add_text(tree, tvb, offset, 4, "Redirect header");
  rh_tree = proto_item_add_subtree(ti, ett_gre_wccp2_redirect_header);

  rh_flags = tvb_get_guint8(tvb, offset);
  proto_tree_add_text(rh_tree, tvb, offset, 1, "%s",
		      decode_boolean_bitfield(rh_flags, 0x80, 8,
				      "Dynamic service", "Well-known service"));
  proto_tree_add_text(rh_tree, tvb, offset, 1, "%s",
		      decode_boolean_bitfield(rh_flags, 0x40, 8,
			      "Alternative bucket used", "Alternative bucket not used"));

  proto_tree_add_text(rh_tree, tvb, offset + 1, 1, "Service ID: %s",
      val_to_str(tvb_get_guint8(tvb, offset + 1), service_id_vals, "Unknown (0x%02X)"));
  if (rh_flags & 0x40)
    proto_tree_add_text(rh_tree, tvb, offset + 2, 1, "Alternative bucket index: %u",
			tvb_get_guint8(tvb, offset + 2));
  proto_tree_add_text(rh_tree, tvb, offset + 3, 1, "Primary bucket index: %u",
			tvb_get_guint8(tvb, offset + 3));
}
 
void
proto_register_gre(void)
{
	static hf_register_info hf[] = {
		{ &hf_gre_proto,
			{ "Protocol Type", "gre.proto", FT_UINT16, BASE_HEX, VALS(typevals), 0x0,
				"The protocol that is GRE encapsulated"}
		},
	};
	static gint *ett[] = {
		&ett_gre,
		&ett_gre_flags,
		&ett_gre_wccp2_redirect_header,
	};

        proto_gre = proto_register_protocol("Generic Routing Encapsulation",
	    "GRE", "gre");
        proto_register_field_array(proto_gre, hf, array_length(hf));
	proto_register_subtree_array(ett, array_length(ett));
}

void
proto_reg_handoff_gre(void)
{
	dissector_add("ip.proto", IP_PROTO_GRE, dissect_gre);

	/*
	 * Get handles for the IP and PPP dissectors.
	 */
	ip_handle = find_dissector("ip");
	ppp_handle = find_dissector("ppp");
}
