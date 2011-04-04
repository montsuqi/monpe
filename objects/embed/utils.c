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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define EMBED_UTILS_MAIN

#include "object.h"
#include "sheet.h"
#include "intl.h"
#include "plug-ins.h"
#include "utils.h"

gchar *
get_default_embed_id(
  gchar *template)
{
  int i = 1;
  gchar *id;

  while(1) {
    id = g_strdup_printf("%s%d",template,i);
    if ((g_hash_table_lookup(EmbedIDTable,id)) == NULL) {
      register_embed_id(id);
      break;
    }
    g_free(id);
    i++;
  }
  return id;
}

void 
register_embed_id(gchar *id)
{
  if ((g_hash_table_lookup(EmbedIDTable,id)) == NULL) {
    g_hash_table_insert(EmbedIDTable,id,id);
  }
}

