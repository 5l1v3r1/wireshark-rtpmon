/* packet-ipv6.c
 * Routines for IPv6 packet disassembly 
 *
 * $Id: packet-ipv6.c,v 1.7 1999/03/28 18:31:59 gram Exp $
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
#include <sys/socket.h>

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#include <string.h>
#include <stdio.h>
#include <glib.h>
#include "packet.h"
#include "packet-ip.h"
#include "packet-ipv6.h"
#include "etypes.h"
#include "resolv.h"

#ifndef offsetof
#define	offsetof(type, member)	((size_t)(&((type *)0)->member))
#endif

static const char *
inet_ntop6(const u_char *src, char *dst, size_t size);

static const char *
inet_ntop4(const u_char *src, char *dst, size_t size);

static int
dissect_routing6(const u_char *pd, int offset, frame_data *fd, proto_tree *tree) {
    return 0;
}

static int
dissect_frag6(const u_char *pd, int offset, frame_data *fd, proto_tree *tree) {
    if (check_col(fd, COL_INFO)) {
	col_add_fstr(fd, COL_INFO, "IPv6 fragment");
    }
    return 0;
}

static int
dissect_hopopts(const u_char *pd, int offset, frame_data *fd, proto_tree *tree) {
    return 0;
}

static int
dissect_dstopts(const u_char *pd, int offset, frame_data *fd, proto_tree *tree) {
    struct ip6_dest dstopt;
    int len;
    proto_tree *dstopt_tree;
	proto_item *ti;

    memcpy(&dstopt, (void *) &pd[offset], sizeof(dstopt)); 
    len = (dstopt.ip6d_len + 1) << 3;

    if (tree) {
	/* !!! specify length */
	ti = proto_tree_add_item(tree, offset, len,
	    "Destination Option Header");
	dstopt_tree = proto_tree_new();
	proto_item_add_subtree(ti, dstopt_tree, ETT_IPv6);

	proto_tree_add_item(dstopt_tree,
	    offset + offsetof(struct ip6_dest, ip6d_len), 1,
	    "Length: %d (%d bytes)", dstopt.ip6d_len, len);
  
	/* decode... */
    }

    return len;
}


void
dissect_ipv6(const u_char *pd, int offset, frame_data *fd, proto_tree *tree) {
  proto_tree *ipv6_tree;
  proto_item *ti;
  guint8 nxt;
  int advance;

  struct ip6_hdr ipv6;

  memcpy(&ipv6, (void *) &pd[offset], sizeof(ipv6)); 

  if (check_col(fd, COL_PROTOCOL))
    col_add_str(fd, COL_PROTOCOL, "IPv6");
  if (check_col(fd, COL_RES_NET_SRC))
    col_add_str(fd, COL_RES_NET_SRC, get_hostname6(&ipv6.ip6_src));
  if (check_col(fd, COL_UNRES_NET_SRC))
    col_add_str(fd, COL_UNRES_NET_SRC, ip6_to_str(&ipv6.ip6_src));
  if (check_col(fd, COL_RES_NET_DST))
    col_add_str(fd, COL_RES_NET_DST, get_hostname6(&ipv6.ip6_dst));
  if (check_col(fd, COL_UNRES_NET_DST))
    col_add_str(fd, COL_UNRES_NET_DST, ip6_to_str(&ipv6.ip6_dst));

  if (tree) {
    /* !!! specify length */
    ti = proto_tree_add_item(tree, offset, 40, "Internet Protocol Version 6");
    ipv6_tree = proto_tree_new();
    proto_item_add_subtree(ti, ipv6_tree, ETT_IPv6);

    /* !!! warning: version also contains 4 Bit priority */
    proto_tree_add_item(ipv6_tree,
		offset + offsetof(struct ip6_hdr, ip6_vfc), 1,
		"Version: %d", ipv6.ip6_vfc >> 4);

    proto_tree_add_item(ipv6_tree,
		offset + offsetof(struct ip6_hdr, ip6_flow), 4,
		"Flowinfo: 0x%07x", ipv6.ip6_flow & IPV6_FLOWINFO_MASK);

    proto_tree_add_item(ipv6_tree,
		offset + offsetof(struct ip6_hdr, ip6_plen), 2,
		"Payload Length: %d", ntohs(ipv6.ip6_plen));

    proto_tree_add_item(ipv6_tree,
		offset + offsetof(struct ip6_hdr, ip6_nxt), 1,
		"Next Header: %d", ipv6.ip6_nxt);

    proto_tree_add_item(ipv6_tree,
		offset + offsetof(struct ip6_hdr, ip6_hlim), 1,
		"Hop limit: %d", ipv6.ip6_hlim);

    proto_tree_add_item(ipv6_tree,
		offset + offsetof(struct ip6_hdr, ip6_src), 16,
#ifdef INET6
		"Source address: %s (%s)",
		get_hostname6(&ipv6.ip6_src),
#else
		"Source address: %s",
#endif
		ip6_to_str(&ipv6.ip6_src));

	proto_tree_add_item(ipv6_tree,
		offset + offsetof(struct ip6_hdr, ip6_dst), 16,
#ifdef INET6
		"Destination address: %s (%s)",
		get_hostname6(&ipv6.ip6_dst),
#else
		"Destination address: %s",
#endif
		ip6_to_str(&ipv6.ip6_dst));
  }

  /* start of the new header (could be a extension header) */
  offset += 40;
  nxt = ipv6.ip6_nxt;

again:
    switch (nxt) {
    case IP_PROTO_HOPOPTS:
	dissect_hopopts(pd, offset, fd, tree);
#if 0
	goto again;
#else
	break;
#endif
    case IP_PROTO_IPIP:
	dissect_ip(pd, offset, fd, tree);
	break;
    case IP_PROTO_ROUTING:
	dissect_routing6(pd, offset, fd, tree);
#if 0
	goto again;
#else
	break;
#endif
    case IP_PROTO_FRAGMENT:
	dissect_frag6(pd, offset, fd, tree);
#if 0
	goto again;
#else
	break;
#endif
    case IP_PROTO_ICMPV6:
	dissect_icmpv6(pd, offset, fd, tree);
	break;
    case IP_PROTO_NONE:
	if (check_col(fd, COL_INFO)) {
	    col_add_fstr(fd, COL_INFO, "IPv6 no next header");
	}
	break;
    case IP_PROTO_AH:
	advance = dissect_ah(pd, offset, fd, tree);
	nxt = pd[offset];
	offset += advance;
	goto again;
    case IP_PROTO_ESP:
	dissect_esp(pd, offset, fd, tree);
	break;
    case IP_PROTO_DSTOPTS:
	advance = dissect_dstopts(pd, offset, fd, tree);
	nxt = pd[offset];
	offset += advance;
	goto again;
    case IP_PROTO_TCP:
	dissect_tcp(pd, offset, fd, tree);
	break;
    case IP_PROTO_UDP:
	dissect_udp(pd, offset, fd, tree);
	break;
    default:
	if (check_col(fd, COL_INFO)) {
	    col_add_fstr(fd, COL_INFO, "Unknown IPv6 protocol (0x%02x)",
		ipv6.ip6_nxt);
	}
	dissect_data(pd, offset, fd, tree);
    }
}

gchar *
ip6_to_str(struct e_in6_addr *ad) {
  static gchar buf[4 * 8 + 8];

  inet_ntop6((u_char*)ad, (gchar*)buf, sizeof(buf));
  return buf;
}

#ifndef NS_IN6ADDRSZ
#define NS_IN6ADDRSZ	16
#endif

#ifndef NS_INT16SZ
#define NS_INT16SZ	(sizeof(guint16))
#endif

#define SPRINTF(x) ((size_t)sprintf x)

/*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* const char *
 * inet_ntop4(src, dst, size)
 *	format an IPv4 address
 * return:
 *	`dst' (as a const)
 * notes:
 *	(1) uses no statics
 *	(2) takes a u_char* not an in_addr as input
 * author:
 *	Paul Vixie, 1996.
 */
static const char *
inet_ntop4(src, dst, size)
	const u_char *src;
	char *dst;
	size_t size;
{
	static const char fmt[] = "%u.%u.%u.%u";
	char tmp[sizeof "255.255.255.255"];

	if (SPRINTF((tmp, fmt, src[0], src[1], src[2], src[3])) > size) {
		return (NULL);
	}
	strcpy(dst, tmp);
	return (dst);
}

/* const char *
 * inet_ntop6(src, dst, size)
 *	convert IPv6 binary address into presentation (printable) format
 * author:
 *	Paul Vixie, 1996.
 */
static const char *
inet_ntop6(src, dst, size)
	const u_char *src;
	char *dst;
	size_t size;
{
	/*
	 * Note that int32_t and int16_t need only be "at least" large enough
	 * to contain a value of the specified size.  On some systems, like
	 * Crays, there is no such thing as an integer variable with 16 bits.
	 * Keep this in mind if you think this function should have been coded
	 * to use pointer overlays.  All the world's not a VAX.
	 */
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
	struct { int base, len; } best, cur;
	u_int words[NS_IN6ADDRSZ / NS_INT16SZ];
	int i;

	/*
	 * Preprocess:
	 *	Copy the input (bytewise) array into a wordwise array.
	 *	Find the longest run of 0x00's in src[] for :: shorthanding.
	 */
	memset(words, '\0', sizeof words);
	for (i = 0; i < NS_IN6ADDRSZ; i++)
		words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));
	best.base = -1;
	cur.base = -1;
	for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
		if (words[i] == 0) {
			if (cur.base == -1)
				cur.base = i, cur.len = 1;
			else
				cur.len++;
		} else {
			if (cur.base != -1) {
				if (best.base == -1 || cur.len > best.len)
					best = cur;
				cur.base = -1;
			}
		}
	}
	if (cur.base != -1) {
		if (best.base == -1 || cur.len > best.len)
			best = cur;
	}
	if (best.base != -1 && best.len < 2)
		best.base = -1;

	/*
	 * Format the result.
	 */
	tp = tmp;
	for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++) {
		/* Are we inside the best run of 0x00's? */
		if (best.base != -1 && i >= best.base &&
		    i < (best.base + best.len)) {
			if (i == best.base)
				*tp++ = ':';
			continue;
		}
		/* Are we following an initial run of 0x00s or any real hex? */
		if (i != 0)
			*tp++ = ':';
		/* Is this address an encapsulated IPv4? */
		if (i == 6 && best.base == 0 &&
		    (best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
			if (!inet_ntop4(src+12, tp, sizeof tmp - (tp - tmp)))
				return (NULL);
			tp += strlen(tp);
			break;
		}
		tp += SPRINTF((tp, "%x", words[i]));
	}
	/* Was it a trailing run of 0x00's? */
	if (best.base != -1 && (best.base + best.len) == 
	    (NS_IN6ADDRSZ / NS_INT16SZ))
		*tp++ = ':';
	*tp++ = '\0';

	/*
	 * Check for overflow, copy, and we're done.
	 */
	if ((size_t)(tp - tmp) > size) {
		return (NULL);
	}
	strcpy(dst, tmp);
	return (dst);
}

