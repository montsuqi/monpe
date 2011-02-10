/* Dia -- an diagram creation/manipulation program
 * Copyright (C) 1998 Alexander Larsson
 *
 * Grid object
 * Copyright (C) 2008 Don Blaheta
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

#include <assert.h>
#include <math.h>
#include <string.h>
#include <glib.h>
#include <time.h>
#include <stdio.h>

#include "intl.h"
#include "object.h"
#include "element.h"
#include "connectionpoint.h"
#include "diarenderer.h"
#include "attributes.h"
#include "widgets.h"
#include "message.h"
#include "color.h"
#include "text.h"
#include "font.h"
#include "properties.h"

#include "pixmaps/etable.xpm"

#define GRID_OBJECT_BASE_CONNECTION_POINTS 9

typedef struct _ETable {
  Element element;

  ConnectionPoint base_cps[GRID_OBJECT_BASE_CONNECTION_POINTS];
  Color border_color;
  real border_line_width;
  Color inner_color;
  gboolean show_background;
  gint grid_rows;
  gint grid_cols;
  Color gridline_color;
  real gridline_width;

  gboolean draw_line;

  /* texts */
  DiaFont *font;
  real font_height;
  Color text_color;
  Alignment alignment;
  gchar *template;

  int numtexts;
  gchar **texts;

  gchar *embed_id;
  gint embed_text_size;
} ETable;

static real etable_distance_from(ETable *etable,
                                       Point *point);

static void etable_select(ETable *etable,
                                Point *clicked_point,
                                DiaRenderer *interactive_renderer);
static ObjectChange* etable_move_handle(ETable *etable,
					      Handle *handle, Point *to, 
					      ConnectionPoint *cp, HandleMoveReason reason, 
                                     ModifierKeys modifiers);
static ObjectChange* etable_move(ETable *etable, Point *to);
static void etable_draw(ETable *etable, DiaRenderer *renderer);
static void etable_update_data(ETable *etable);
static DiaObject *etable_create(Point *startpoint,
                                   void *user_data,
                                   Handle **handle1,
                                   Handle **handle2);
static void etable_destroy(ETable *etable);
static DiaObject *etable_load(ObjectNode obj_node, int version, 
                                 const char *filename);
static PropDescription *etable_describe_props(
  ETable *etable);
static void etable_get_props(ETable *etable, 
                                   GPtrArray *props);
static void etable_set_props(ETable *etable, 
                                   GPtrArray *props);

static ObjectTypeOps etable_type_ops =
{
  (CreateFunc) etable_create,
  (LoadFunc)   etable_load/*using properties*/,
  (SaveFunc)   object_save_using_properties,
  (GetDefaultsFunc)   NULL,
  (ApplyDefaultsFunc) NULL
};

DiaObjectType etable_type =
{
  "Embed - Table",  /* name */
  0,                 /* version */
  (char **) etable_xpm, /* pixmap */
  
  &etable_type_ops      /* ops */
};

DiaObjectType *_etable_type = (DiaObjectType *) &etable_type;

static ObjectOps etable_ops = {
  (DestroyFunc)         etable_destroy,
  (DrawFunc)            etable_draw,
  (DistanceFunc)        etable_distance_from,
  (SelectFunc)          etable_select,
  (CopyFunc)            object_copy_using_properties,
  (MoveFunc)            etable_move,
  (MoveHandleFunc)      etable_move_handle,
  (GetPropertiesFunc)   object_create_props_dialog,
  (ApplyPropertiesDialogFunc) object_apply_props_from_dialog,
  (ObjectMenuFunc)      NULL,
  (DescribePropsFunc)   etable_describe_props,
  (GetPropsFunc)        etable_get_props,
  (SetPropsFunc)        etable_set_props,
  (TextEditFunc) 0,
  (ApplyPropertiesListFunc) object_apply_props,
};

static PropNumData rows_columns_range = { 1, G_MAXINT, 1 };
static PropNumData embed_text_size_range = { 1, G_MAXINT, 10 };

static PropDescription etable_props[] = {
  ELEMENT_COMMON_PROPERTIES,
  PROP_STD_LINE_WIDTH,
  PROP_STD_LINE_COLOUR,
  PROP_STD_FILL_COLOUR,
  PROP_STD_SHOW_BACKGROUND,

  { "grid_rows", PROP_TYPE_INT, PROP_FLAG_VISIBLE,
    N_("Rows"), NULL, &rows_columns_range },
  { "grid_cols", PROP_TYPE_INT, PROP_FLAG_VISIBLE,
    N_("Columns"), NULL, &rows_columns_range },
  { "gridline_colour", PROP_TYPE_COLOUR, PROP_FLAG_VISIBLE,
    N_("Gridline color"), NULL, NULL },
  { "gridline_width", PROP_TYPE_REAL, PROP_FLAG_VISIBLE,
    N_("Gridline width"), NULL, &prop_std_line_width_data },


  { "draw_line", PROP_TYPE_BOOL, PROP_FLAG_VISIBLE ,
    N_("Draw Line"), NULL, NULL },

  { "font", PROP_TYPE_FONT, PROP_FLAG_VISIBLE ,
    N_("Font"), NULL, NULL },
  { "font_height", PROP_TYPE_REAL, PROP_FLAG_VISIBLE ,
    N_("Font height"), NULL, NULL },
  { "text_color", PROP_TYPE_COLOUR, PROP_FLAG_VISIBLE,
    N_("Text color"), NULL, NULL },
  { "alignment", PROP_TYPE_ENUM, PROP_FLAG_VISIBLE,
    N_("Alignment"), NULL,  prop_std_text_align_data },


  { "template", PROP_TYPE_STRING, PROP_FLAG_VISIBLE,
    N_("Text Template"), NULL, NULL },

  { "embed_id", PROP_TYPE_STRING, PROP_FLAG_VISIBLE,
    N_("Embed ID"), NULL, NULL },
  { "embed_text_size", PROP_TYPE_INT, PROP_FLAG_VISIBLE,
    N_("Embed Text Size"), NULL, &embed_text_size_range },
  
  {NULL}
};

static PropDescription *
etable_describe_props(ETable *etable) 
{
  if (etable_props[0].quark == 0) {
    prop_desc_list_calculate_quarks(etable_props);
  }
  return etable_props;
}    

static PropOffset etable_offsets[] = {
  ELEMENT_COMMON_PROPERTIES_OFFSETS,
  { PROP_STDNAME_LINE_WIDTH, PROP_STDTYPE_LINE_WIDTH, offsetof(ETable, border_line_width) },
  { "line_colour", PROP_TYPE_COLOUR, offsetof(ETable, border_color) },
  { "fill_colour", PROP_TYPE_COLOUR, offsetof(ETable,inner_color) },
  { "show_background", PROP_TYPE_BOOL,offsetof(ETable,show_background) },
  { "grid_rows", PROP_TYPE_INT, offsetof(ETable, grid_rows) },
  { "grid_cols", PROP_TYPE_INT, offsetof(ETable, grid_cols) },
  { "gridline_colour", PROP_TYPE_COLOUR, offsetof(ETable, gridline_color) },
  { "gridline_width", PROP_TYPE_REAL, offsetof(ETable,
                                                 gridline_width) },

  { "draw_line", PROP_TYPE_BOOL,offsetof(ETable,draw_line) },

  {"font",PROP_TYPE_FONT,offsetof(ETable,font)},
  {"font_height",PROP_TYPE_REAL,offsetof(ETable,font_height)},
  {"text_color",PROP_TYPE_COLOUR,offsetof(ETable,text_color)},
  {"alignment",PROP_TYPE_ENUM,offsetof(ETable,alignment)},

  { "embed_id", PROP_TYPE_STRING, offsetof(ETable, embed_id) },
  { "embed_text_size", PROP_TYPE_INT, offsetof(ETable, embed_text_size) },
  {NULL}
};

static void
etable_get_props(ETable *etable, GPtrArray *props)
{  
  object_get_props_from_offsets(&etable->element.object,
                                etable_offsets,props);
}

static void
etable_set_props(ETable *etable, GPtrArray *props)
{
  DiaObject *obj = &etable->element.object;

  object_set_props_from_offsets(obj, etable_offsets,props);

  etable_update_data(etable);
}

static real
etable_distance_from(ETable *etable, Point *point)
{
  DiaObject *obj = &etable->element.object;
  return distance_rectangle_point(&obj->bounding_box, point);
}

static void
etable_select(ETable *etable, Point *clicked_point,
		    DiaRenderer *interactive_renderer)
{
  element_update_handles(&etable->element);
}

static ObjectChange*
etable_move_handle(ETable *etable, Handle *handle,
			 Point *to, ConnectionPoint *cp, 
			 HandleMoveReason reason, ModifierKeys modifiers)
{
  g_assert(etable!=NULL);
  g_assert(handle!=NULL);
  g_assert(to!=NULL);

  element_move_handle(&etable->element, handle->id, to, cp, 
		      reason, modifiers);
  etable_update_data(etable);

  return NULL;
}

static ObjectChange*
etable_move(ETable *etable, Point *to)
{
  etable->element.corner = *to;
  etable_update_data(etable);

  return NULL;
}

/** Converts 2D indices into 1D.  Currently row-major. */
inline static int grid_cell (int i, int j, int rows, int cols)
{
  return j * cols + i;
}

static void
etable_update_data(ETable *etable)
{
  Element *elem = &etable->element;
  DiaObject *obj = &elem->object;
  ElementBBExtras *extra = &elem->extra_spacing;

  coord left, top;

  extra->border_trans = etable->border_line_width / 2.0;
  element_update_boundingbox(elem);
  element_update_handles(elem);
  element_update_connections_rectangle(elem, etable->base_cps);

  obj->position = elem->corner;
  left = obj->position.x;
  top = obj->position.y;
}  

static void 
etable_draw_gridlines (ETable *etable, DiaRenderer *renderer,
    		Point* lr_corner)
{
  DiaRendererClass *renderer_ops = DIA_RENDERER_GET_CLASS (renderer);
  Element *elem;
  Point st, fn;
  unsigned i;
  real inset;
  real cell_size;


  elem = &etable->element;

  inset = (etable->border_line_width - etable->gridline_width)/2;

  /* horizontal gridlines */
  st.x = elem->corner.x;
  st.y = elem->corner.y + inset;
  fn.x = elem->corner.x + elem->width;
  fn.y = elem->corner.y + inset;

  cell_size = (elem->height - 2 * inset)
              / etable->grid_rows;
  if (cell_size < 0)
    cell_size = 0;
  for (i = 1; i < etable->grid_rows; ++i) {
    st.y += cell_size;
    fn.y += cell_size;
    renderer_ops->draw_line(renderer,&st,&fn,&etable->gridline_color);
  }

  /* vertical gridlines */
  st.x = elem->corner.x + inset;
  st.y = elem->corner.y;
  fn.x = elem->corner.x + inset;
  fn.y = elem->corner.y + elem->height;

  cell_size = (elem->width - 2 * inset)
              / etable->grid_cols;
  if (cell_size < 0)
    cell_size = 0;
  for (i = 1; i < etable->grid_cols; ++i) {
    st.x += cell_size;
    fn.x += cell_size;
    renderer_ops->draw_line(renderer,&st,&fn,&etable->gridline_color);
  }
}

static void
etable_draw(ETable *etable, DiaRenderer *renderer)
{
  DiaRendererClass *renderer_ops = DIA_RENDERER_GET_CLASS (renderer);
  Element *elem;
  Point lr_corner;
  
  g_assert(etable != NULL);
  g_assert(renderer != NULL);

  elem = &etable->element;

  lr_corner.x = elem->corner.x + elem->width;
  lr_corner.y = elem->corner.y + elem->height;

  renderer_ops->set_linejoin(renderer, LINEJOIN_MITER);
  renderer_ops->set_linestyle(renderer, LINESTYLE_SOLID);

  /* draw background */
  if (etable->show_background)
    renderer_ops->fill_rect(renderer,&elem->corner,
                                &lr_corner,
                                &etable->inner_color);

  /* draw gridlines */
  renderer_ops->set_linewidth(renderer, etable->gridline_width);
  etable_draw_gridlines(etable, renderer, &lr_corner);
  
  /* draw outline */
  renderer_ops->set_linewidth(renderer, etable->border_line_width);
  renderer_ops->draw_rect(renderer,&elem->corner,
                              &lr_corner,
                              &etable->border_color);
}


static DiaObject *
etable_create(Point *startpoint,
	   void *user_data,
	   Handle **handle1,
	   Handle **handle2)
{
  ETable *etable;
  Element *elem;
  DiaObject *obj;
  unsigned i;

  etable = g_new0(ETable,1);
  elem = &(etable->element);

  obj = &(etable->element.object);
  obj->type = &etable_type;
  obj->ops = &etable_ops;

  elem->corner = *startpoint;
  elem->width = 4.0;
  elem->height = 4.0;

  element_init(elem, 8, 9);

  etable->border_color = attributes_get_foreground();
  etable->border_line_width = attributes_get_default_linewidth();
  etable->inner_color = attributes_get_background();
  etable->show_background = TRUE;
  etable->grid_rows = 3;
  etable->grid_cols = 4;
  etable->gridline_color = attributes_get_foreground();
  etable->gridline_width = attributes_get_default_linewidth();

  for (i = 0; i < GRID_OBJECT_BASE_CONNECTION_POINTS; ++i)
  {
    obj->connections[i] = &etable->base_cps[i];
    etable->base_cps[i].object = obj;
    etable->base_cps[i].connected = NULL;
  }
  etable->base_cps[8].flags = CP_FLAGS_MAIN;

  etable_update_data(etable);
  
  *handle1 = NULL;
  *handle2 = obj->handles[7];  

  /* font */
  if (etable->font == NULL) {
      etable->font_height = 0.8;
      etable->font = dia_font_new_from_style (DIA_FONT_MONOSPACE, 0.8);
  }
  etable->text_color = attributes_get_foreground();
  etable->alignment = ALIGN_LEFT;
  etable->template = g_strdup("abcdef");

  etable->numtexts = etable->grid_rows * etable->grid_cols;
  etable->texts = g_malloc0(sizeof(gchar*)*etable->numtexts);
  for (i = 0; i < etable->numtexts; ++i) {
    *(etable->texts + i) = g_strdup(etable->template);
  }

  etable->embed_id = g_strdup("embed_table");
  etable->embed_text_size = 10;

  return &etable->element.object;
}

static void 
etable_destroy(ETable *etable)
{
  element_destroy(&etable->element);
  dia_font_unref(etable->font);
  g_free(etable->template);
  g_free(etable->embed_id);
  g_strfreev(etable->texts);
}

static DiaObject *
etable_load(ObjectNode obj_node, int version, const char *filename)
{
  return object_load_using_properties(&etable_type,
                                      obj_node,version,filename);  
}
