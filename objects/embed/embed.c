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

#include "object.h"
#include "sheet.h"
#include "intl.h"
#include "plug-ins.h"
#include "utils.h"

extern DiaObjectType *_etextobj_type;
extern DiaObjectType *_eimage_type;
extern DiaObjectType *_tcircle_type;

DIA_PLUGIN_CHECK_INIT

PluginInitResult
dia_plugin_init(PluginInfo *info)
{
  if (!dia_plugin_info_init(info, "Embed", _("Embed objects"),
			    NULL, NULL))
    return DIA_PLUGIN_INIT_ERROR;

  object_register_type(_etextobj_type);
  object_register_type(_eimage_type);
  object_register_type(_tcircle_type);

  EmbedIDTable = g_hash_table_new(g_str_hash,g_str_equal);

  return DIA_PLUGIN_INIT_OK;
}
