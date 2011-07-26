/* Dia -- an diagram creation/manipulation program
 * Copyright (C) 1998 Alexander Larsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <config.h>

#include <stdio.h>

#include "cut_n_paste.h"
#include "object.h"
#include "object_ops.h"
#include "display.h"
#include "dtree.h"
#include "group.h"
#include "message.h"

static GList *stored_list = NULL;
static int stored_generation = 0;
static DDisplay *stored_ddisp = NULL;

static void free_stored(void)
{
  if (stored_list != NULL) {
    destroy_object_list(stored_list);
    stored_list = NULL;
  }
  stored_ddisp = NULL;
}

void
cnp_store_objects(GList *object_list, int generation,DDisplay *ddisp)
{
  free_stored();
  stored_list = object_list;
  stored_generation = generation;
  stored_ddisp = ddisp;
}

void
cnp_prepare_copy_embed_object_list(GList *copied_list)
{
  DiaObject *obj;
  int index,n,j;
  GHashTable *list;

  list = g_hash_table_new(g_direct_hash,g_direct_equal);
  for (j=0;j<g_list_length(copied_list);j++) {
    obj = (DiaObject*)g_list_nth_data(copied_list,j);

    if (IS_GROUP(obj)) {
      cnp_prepare_copy_embed_object_list(group_objects(obj));
    } else {
      if (obj->node != NULL) {
        n = GPOINTER_TO_INT(g_hash_table_lookup(list,obj->node));
        g_hash_table_insert(list,obj->node,GINT_TO_POINTER(n+1));

        if ((index = dnode_data_get_empty_nth_index(obj->node,n)) != -1) {
          object_set_embed_id(obj,dnode_data_get_longname(obj->node,index));
          n++;
        } else {
          message_error(_("cannot paste dictionary object by size over.\n"
                          "dummy object was created.\n"));
          object_change_unknown(obj);
        }
      }
    }
  }
  g_hash_table_destroy(list);
}

GList *
cnp_get_stored_objects(int* generation,DDisplay *ddisp)
{
  GList *copied_list;
  GList *_copied_list;
  int i, index;
  gboolean skipped = FALSE;
  DiaObject *obj;

  copied_list = object_copy_list(stored_list);
  *generation = stored_generation;
  ++stored_generation;

  _copied_list = NULL;
  if (ddisp != stored_ddisp) {
    for (i=0;i<g_list_length(copied_list);i++) {
      obj = (DiaObject*)g_list_nth_data(copied_list,i);
      if (obj->node != NULL) {
        skipped = TRUE;
      } else {
        _copied_list = g_list_append(_copied_list,obj);
      }
      if (skipped) {
        message_error(_("The embedding object paste to different diagram.\n"));
      }
      copied_list = _copied_list;
    }
  } else {
    cnp_prepare_copy_embed_object_list(copied_list);
  }
  return copied_list;
}

gint
cnp_exist_stored_objects(void)
{
  return (stored_list != NULL);
}
