/* filter.h
 * Definitions for packet filter window
 *
 * $Id: filter.h,v 1.2 1998/09/16 03:21:59 gerald Exp $
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

#ifndef __FILTER_H__
#define __FILTER_H__

typedef struct _filter_def {
  char *name;
  char *strval;
} filter_def;

typedef struct _filter_cb_data {
  GList     *fl;
  GtkWidget *win;
} filter_cb_data;

GList *read_filter_list();
void   filter_sel_cb(GtkWidget *, gpointer);
void   filter_sel_list_cb(GtkWidget *, gpointer);
void   filter_sel_new_cb(GtkWidget *, gpointer);
void   filter_sel_chg_cb(GtkWidget *, gpointer);
void   filter_sel_copy_cb(GtkWidget *, gpointer);
void   filter_sel_del_cb(GtkWidget *, gpointer);
void   filter_sel_ok_cb(GtkWidget *, gpointer);
void   filter_sel_save_cb(GtkWidget *, gpointer);
void   filter_sel_cancel_cb(GtkWidget *, gpointer);

/* GList *get_interface_list();
void   capture_prep_file_cb(GtkWidget *, gpointer);
void   cap_prep_fs_ok_cb(GtkWidget *, gpointer);
void   cap_prep_fs_cancel_cb(GtkWidget *, gpointer);
void   capture_prep_ok_cb(GtkWidget *, gpointer);
void   capture_prep_close_cb(GtkWidget *, gpointer);
 */
#endif /* capture.h */
