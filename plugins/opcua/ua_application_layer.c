/******************************************************************************
** $Id: ua_application_layer.c,v 1.3 2007/02/08 11:31:56 gergap Exp $
**
** Copyright (C) 2006-2007 ascolab GmbH. All Rights Reserved.
** Web: http://www.ascolab.com
** 
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
** 
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
** 
** Project: OpcUa Wireshark Plugin
**
** Description: OpcUa Application Layer Decoder.
**
** Author: Gerhard Gappmeier <gerhard.gappmeier@ascolab.com>
** Last change by: $Author: gergap $
**
******************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <gmodule.h>
#include <epan/packet.h>
#include "opcua_simpletypes.h"

/** NodeId encoding mask table */
static const value_string g_nodeidmasks[] = {
    { 0, "Two byte encoded Numeric" },
    { 1, "Four byte encoded Numeric" },
    { 2, "Numeric of arbitrary length" },
    { 3, "String" },
    { 4, "URI" },
    { 5, "GUID" },
    { 6, "ByteString" },
    { 0x80, "UriMask" },
    { 0, NULL }
};

/** Service type table */
extern const value_string g_requesttypes[];

static int hf_opcua_nodeid_encodingmask = -1;
static int hf_opcua_app_nsid = -1;
static int hf_opcua_app_numeric = -1;

/** header field definitions */
static hf_register_info hf[] =
{
    { &hf_opcua_nodeid_encodingmask,
    {  "NodeId EncodingMask",        "application.nodeid.encodingmask", FT_UINT8,   BASE_HEX,  VALS(g_nodeidmasks), 0x0,    "",    HFILL }
    },
    { &hf_opcua_app_nsid,
    {  "NodeId EncodingMask",        "application.nodeid.nsid",         FT_UINT8,   BASE_DEC,  NULL, 0x0,    "",    HFILL }
    },
    { &hf_opcua_app_numeric,
    {  "NodeId Identifier Numeric",  "application.nodeid.numeric",      FT_UINT32,  BASE_DEC,  VALS(g_requesttypes), 0x0,    "",    HFILL }
    }
};

/** Register application layer types. */
void registerApplicationLayerTypes(int proto)
{
    proto_register_field_array(proto, hf, array_length(hf));
}

/** Parses an OpcUa Service NodeId and returns the service type.
 * In this cases the NodeId is always from type numeric and NSId = 0.
 */
int parseServiceNodeId(proto_tree *tree, tvbuff_t *tvb, gint *pOffset, char *szFieldName)
{
    gint    iOffset = *pOffset;
    guint8  EncodingMask, NSId = 0;
    guint32 Numeric = 0;

	szFieldName = 0; /* avoid warning */

    EncodingMask = tvb_get_guint8(tvb, iOffset);
    proto_tree_add_item(tree, hf_opcua_nodeid_encodingmask, tvb, iOffset, 1, TRUE);
    iOffset++;

    switch(EncodingMask)
    {
    case 0x00: /* two byte node id */
        Numeric = tvb_get_guint8(tvb, iOffset);
        proto_tree_add_item(tree, hf_opcua_app_numeric, tvb, iOffset, 1, TRUE);
        iOffset+=1;
        break;
    case 0x01: /* four byte node id */
        NSId = tvb_get_guint8(tvb, iOffset);
        proto_tree_add_item(tree, hf_opcua_app_nsid, tvb, iOffset, 1, TRUE);
        iOffset+=1;
        Numeric = tvb_get_letohs(tvb, iOffset);
        proto_tree_add_item(tree, hf_opcua_app_numeric, tvb, iOffset, 2, TRUE);
        iOffset+=2;
        break;
    case 0x02: /* numeric, that does not fit into four bytes */
        NSId = tvb_get_letohl(tvb, iOffset);
        proto_tree_add_item(tree, hf_opcua_app_nsid, tvb, iOffset, 4, TRUE);
        iOffset+=4;
        Numeric = tvb_get_letohl(tvb, iOffset);
        proto_tree_add_item(tree, hf_opcua_app_numeric, tvb, iOffset, 4, TRUE);
        iOffset+=4;
        break;
    case 0x03: /* string */
    case 0x04: /* uri */
    case 0x05: /* guid */
    case 0x06: /* byte string */
        /* NOT USED */
        break;
    };

    *pOffset = iOffset;

    return Numeric;
}

