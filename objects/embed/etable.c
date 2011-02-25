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
#define MAX_ROWS_COLS 100

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

  real cell_width[MAX_ROWS_COLS];
  Alignment align[MAX_ROWS_COLS];

  /* texts */
  real padding;
  DiaFont *font;
  real font_height;
  Color text_color;
  gchar *template;
  gchar *cell_widths;
  gchar *aligns;

  int numtexts;
  gchar **texts;

  gchar *embed_id;
  gchar *embed_text_sizes;
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
static void etable_save(ETable *etable, ObjectNode obj_node, const char *filename);
static DiaObject *etable_load(ObjectNode obj_node, int version, 
                                 const char *filename);
static PropDescription *etable_describe_props(
  ETable *etable);
static void etable_get_props(ETable *etable, 
                                   GPtrArray *props);
static void etable_set_props(ETable *etable, 
                                   GPtrArray *props);
static void add_column_handle(ETable *etable, 
  Point point);
static void remove_column_handle(ETable *etable);
static void etable_eval_aligns(ETable *etable);
static void etable_eval_cell_widths(ETable *etable);
static void etable_update_cell_widths(ETable *etable);
static void etable_rescale_cell_widths(ETable *etable);

static ObjectTypeOps etable_type_ops =
{
  (CreateFunc) etable_create,
  (LoadFunc)   etable_load/*using properties*/,
  (SaveFunc)   etable_save,
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

static PropNumData rows_columns_range = { 1, MAX_ROWS_COLS, 1 };
static PropNumData text_padding_data = { 0.0, 10.0, 0.1 };

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

  { "padding", PROP_TYPE_REAL, PROP_FLAG_VISIBLE,
    N_("Text padding"), NULL, &text_padding_data },
  { "font", PROP_TYPE_FONT, PROP_FLAG_VISIBLE ,
    N_("Font"), NULL, NULL },
  { "font_height", PROP_TYPE_REAL, PROP_FLAG_VISIBLE ,
    N_("Font height"), NULL, NULL },
  { "text_color", PROP_TYPE_COLOUR, PROP_FLAG_VISIBLE,
    N_("Text color"), NULL, NULL },

  { "template", PROP_TYPE_STRING, PROP_FLAG_VISIBLE,
    N_("Text template"), NULL, NULL },
  { "aligns", PROP_TYPE_STRING, PROP_FLAG_VISIBLE,
    N_("Cell Alignments"), NULL, NULL },
  { "cell_widths", PROP_TYPE_STRING, PROP_FLAG_VISIBLE,
    N_("Cell Widths"), NULL, NULL },

  { "embed_id", PROP_TYPE_STRING, PROP_FLAG_VISIBLE,
    N_("Embed ID"), NULL, NULL },
  { "embed_text_sizes", PROP_TYPE_STRING, PROP_FLAG_VISIBLE,
    N_("Embed Text Sizes"), NULL, NULL },
  
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

  {"padding", PROP_TYPE_REAL, offsetof(ETable, padding) },
  {"font",PROP_TYPE_FONT,offsetof(ETable,font)},
  {"font_height",PROP_TYPE_REAL,offsetof(ETable,font_height)},
  {"text_color",PROP_TYPE_COLOUR,offsetof(ETable,text_color)},
  {"template", PROP_TYPE_STRING, offsetof(ETable, template) },
  {"aligns", PROP_TYPE_STRING, offsetof(ETable, aligns) },
  {"cell_widths", PROP_TYPE_STRING, offsetof(ETable, cell_widths) },

  { "embed_id", PROP_TYPE_STRING, offsetof(ETable, embed_id) },
  { "embed_text_sizes", PROP_TYPE_STRING, offsetof(ETable, embed_text_sizes) },
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
  int i;
  DiaObject *obj = &etable->element.object;
  Point pos;
  gint old_grid_cols;

  object_set_props_from_offsets(obj, etable_offsets,props);

  pos = etable->element.corner;
  pos.x += etable->element.width;
  pos.y += etable->element.height / 2.0;
  old_grid_cols = obj->num_handles - 8 + 1;
  if (old_grid_cols < etable->grid_cols) {
    for(i=old_grid_cols - 1;i < etable->grid_cols - 1;i++) {
      add_column_handle(etable,pos);
      pos.x += etable->cell_width[i];
    }
  } else if (old_grid_cols > etable->grid_cols) {
    for(i=old_grid_cols - 1;i > etable->grid_cols - 1;i--) {
      remove_column_handle(etable);
    }
  }
  etable_eval_aligns(etable);
  etable_eval_cell_widths(etable);
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
  DiaObject *obj;
  int i;
  real oldx,newx,diff;
  g_assert(etable!=NULL);
  g_assert(handle!=NULL);
  g_assert(to!=NULL);

  obj = &(etable->element.object);
  if (handle->id == HANDLE_CUSTOM1) {
    for (i = 8; i < obj->num_handles; i++) {
      if (handle == obj->handles[i]) {
        oldx = handle->pos.x;
        newx = to->x;
        if (oldx > newx) {
          diff = oldx - newx;
          if (diff > etable->cell_width[i-8]) {
            diff = etable->cell_width[i-8];
          }
          etable->cell_width[i-8] -= diff;
          etable->cell_width[i-8+1] += diff;
        } else if (oldx < newx){
          diff = newx - oldx;
          if (diff > etable->cell_width[i-8+1]) {
            diff = etable->cell_width[i-8+1];
          }
          etable->cell_width[i-8] += diff;
          etable->cell_width[i-8+1] -= diff;
        }
      }
    }
  } else {
    element_move_handle(&etable->element, handle->id, to, cp, 
  		      reason, modifiers);
  }
  etable_rescale_cell_widths(etable);
  etable_update_cell_widths(etable);
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
etable_rescale_cell_widths(ETable *etable)
{
  Element *elem = &etable->element;
  real old_width;
  int i;

  old_width = 0;
  for(i=0;i<etable->grid_cols;i++) {
    old_width += etable->cell_width[i];
  }
  for(i=0;i<etable->grid_cols;i++) {
    etable->cell_width[i] *= elem->width / old_width;
  }
}

static void
etable_update_data(ETable *etable)
{
  Element *elem = &etable->element;
  DiaObject *obj = &elem->object;
  ElementBBExtras *extra = &elem->extra_spacing;
  int new_numtexts;
  int i;

  coord left, top;

  extra->border_trans = etable->border_line_width / 2.0;
  element_update_boundingbox(elem);
  element_update_handles(elem);
  element_update_connections_rectangle(elem, etable->base_cps);

#if 0
  etable_rescale_cell_widths(etable);
#endif

  obj->position = elem->corner;
  left = obj->position.x;
  top = obj->position.y;

  new_numtexts = etable->grid_rows * etable->grid_cols;
  if (etable->numtexts < new_numtexts) {
    g_strfreev(etable->texts);
    etable->texts = g_malloc0(sizeof(gchar*)*(new_numtexts + 1));
    for (i = 0; i < new_numtexts; ++i) {
      *(etable->texts + i) = g_strdup(etable->template);
    }
    *(etable->texts + new_numtexts) = NULL;
  }
  etable->numtexts = new_numtexts;
}  

static void 
etable_update_column_handles (ETable *etable)
{
  DiaObject *obj;
  Element *elem;
  Point st;
  unsigned i;
  real inset;

  elem = &etable->element;
  obj = &elem->object;

  inset = (etable->border_line_width - etable->gridline_width)/2;

  st.x = elem->corner.x + inset;
  st.y = elem->corner.y + elem->height / 2.0;
  for (i = 0; i < etable->grid_cols - 1; ++i) {
    st.x += etable->cell_width[i];
    obj->handles[8 + i]->pos = st;
  }
}

static void 
etable_draw_gridlines (ETable *etable, DiaRenderer *renderer,
    		Point* lr_corner)
{
  DiaObject *obj;
  DiaRendererClass *renderer_ops = DIA_RENDERER_GET_CLASS (renderer);
  Element *elem;
  Point st, fn;
  unsigned i;
  real inset;
  real cell_size;

  elem = &etable->element;
  obj = &elem->object;

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

  for (i = 0; i < etable->grid_cols; ++i) {
    st.x += etable->cell_width[i];
    fn.x += etable->cell_width[i];
    renderer_ops->draw_line(renderer,&st,&fn,&etable->gridline_color);
  }

  st.x = elem->corner.x + inset;
  st.y = elem->corner.y + elem->height / 2.0;
  for (i = 0; i < etable->grid_cols - 1; ++i) {
    st.x += etable->cell_width[i];
    obj->handles[8 + i]->pos = st;
  }

}

static void 
etable_draw_texts (ETable *etable, DiaRenderer *renderer,
    		Point* lr_corner)
{
  Element *elem;
  Point p;
  unsigned i, j;
  real inset;
  real startx;
  real cell_height;
  Text *text;

  elem = &etable->element;

  inset = (etable->border_line_width - etable->gridline_width)/2;
  cell_height = (elem->height - 2 * inset)/etable->grid_rows;

  text = new_text("",etable->font,etable->font_height, 
    &p,&etable->text_color,ALIGN_LEFT);

  for (j=0; j<etable->grid_rows;j++) {
    p.y = elem->corner.y + inset + 
      j * cell_height + cell_height / 2.0;
    startx = elem->corner.x + inset;
    for (i=0; i<etable->grid_cols;i++) {
      switch(etable->align[i]) {
      case ALIGN_LEFT:
        p.x = startx + etable->padding;
        break;
      case ALIGN_CENTER:
        p.x = startx + etable->cell_width[i]/2.0;
        break;
      case ALIGN_RIGHT:
        p.x = startx + etable->cell_width[i] 
          - etable->padding;
        break;
      }
      text_set_alignment(text,etable->align[i]);
      text_set_position(text,&p);
      if (etable->template != NULL && 
        strlen(etable->template) > 0) {
        text_set_string(text,etable->template);
      } else {
        text_set_string(text,
          *(etable->texts + j * etable->grid_cols + i));
      }
      text_draw(text,renderer);
      startx += etable->cell_width[i];
    }
  }
  text_destroy(text);
}

static void
etable_draw(ETable *etable, DiaRenderer *renderer)
{
  DiaRendererClass *renderer_ops = DIA_RENDERER_GET_CLASS (renderer);
  Element *elem;
  Point lr_corner;
  int i;
  
  g_assert(etable != NULL);
  g_assert(renderer != NULL);

  elem = &etable->element;

  elem->width = 0;
  for(i=0;i<etable->grid_cols;i++) {
    elem->width += etable->cell_width[i];
  }

  lr_corner.x = elem->corner.x + elem->width;
  lr_corner.y = elem->corner.y + elem->height;

  renderer_ops->set_linejoin(renderer, LINEJOIN_MITER);
  renderer_ops->set_linestyle(renderer, LINESTYLE_SOLID);

  /* draw background */
  if (etable->show_background)
    renderer_ops->fill_rect(renderer,&elem->corner,
                                &lr_corner,
                                &etable->inner_color);
  if (etable->draw_line) {
    /* draw gridlines */
    renderer_ops->set_linewidth(renderer, etable->gridline_width);
    etable_draw_gridlines(etable, renderer, &lr_corner);
    
    /* draw outline */
    renderer_ops->set_linewidth(renderer, etable->border_line_width);
    renderer_ops->draw_rect(renderer,&elem->corner,
                              &lr_corner,
                              &etable->border_color);
  }
  etable_draw_texts(etable, renderer, &lr_corner);
  etable_update_column_handles(etable);
}

static void
add_column_handle(ETable *etable,
  Point point)
{
  DiaObject *obj;
  Handle *handle;

  obj = &(etable->element.object);
  handle = g_malloc(sizeof(Handle));
  handle->id = HANDLE_CUSTOM1;
  handle->type = HANDLE_MINOR_CONTROL;
  handle->connect_type = HANDLE_NONCONNECTABLE;
  handle->connected_to = NULL;
  handle->pos = point;
  object_add_handle(obj,handle);
}

static void
remove_column_handle(ETable *etable)
{
  DiaObject *obj;
  Handle *handle;

  obj = &(etable->element.object);
  if (obj->num_handles > 0) {
    handle = obj->handles[obj->num_handles-1];
    if (handle->id == HANDLE_CUSTOM1) {
      object_remove_handle(obj,handle);
    }
  }
}

static void
etable_eval_aligns(ETable *etable)
{
  int i;
  gchar **p;

  if (etable->aligns == NULL) {
    return ;
  }

  for(i=0;i<MAX_ROWS_COLS;i++){
    etable->align[i] = ALIGN_LEFT;
  }

  p = g_strsplit(etable->aligns,",",MAX_ROWS_COLS);
  i = 0;
  while (*(p+i) != NULL) {
    if (**(p+i) == 'c' || **(p+i) == 'C') {
      etable->align[i] = ALIGN_CENTER;
    } else if (**(p+i) == 'r' || **(p+i) == 'R') {
      etable->align[i] = ALIGN_RIGHT;
    }
    i++;
  }
  g_strfreev(p);
}

static void
etable_update_cell_widths(ETable *etable)
{
  int i;
  int size = MAX_ROWS_COLS*16;
  gchar buf[32];
  gchar *p = g_malloc0(size);

  if (etable->cell_widths != NULL) {
    g_free(etable->cell_widths);
  }

  for(i=0;i<etable->grid_cols;i++) {
    if (i == 0) {

      sprintf(buf,"%lf",etable->cell_width[i]);
    } else {
      sprintf(buf,",%lf",etable->cell_width[i]);
    }
    g_strlcat(p,buf,size);
  } 
  etable->cell_widths = p;
}

static void
etable_eval_cell_widths(ETable *etable)
{
  int i;
  gchar **p;

  if (etable->cell_widths == NULL) {
    return ;
  }

  p = g_strsplit(etable->cell_widths,",",MAX_ROWS_COLS);
  i = 0;
  while (*(p+i) != NULL) {
    etable->cell_width[i]=(real)g_ascii_strtod(*(p+i),NULL);
    i++;
  }
  
  etable->element.width = 0;
  for(i=0;i<etable->grid_cols;i++){
    etable->element.width += etable->cell_width[i];
  }
  g_strfreev(p);
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
  Point pos;

  etable = g_new0(ETable,1);
  elem = &(etable->element);

  obj = &(etable->element.object);
  obj->type = &etable_type;
  obj->ops = &etable_ops;

  elem->corner = *startpoint;
  elem->width = 12.0;
  elem->height = 5.0;

  element_init(elem, 8, 9);

  etable->border_color = attributes_get_foreground();
  etable->border_line_width = 0.00;
  etable->inner_color = attributes_get_background();
  etable->show_background = FALSE;
  etable->grid_rows = 3;
  etable->grid_cols = 4;
  etable->gridline_color = attributes_get_foreground();
  etable->gridline_width = 0.00;

  for (i = 0; i < GRID_OBJECT_BASE_CONNECTION_POINTS; ++i)
  {
    obj->connections[i] = &etable->base_cps[i];
    etable->base_cps[i].object = obj;
    etable->base_cps[i].connected = NULL;
  }
  etable->base_cps[8].flags = CP_FLAGS_MAIN;

  for (i = 0; i < MAX_ROWS_COLS; i++) {
    etable->cell_width[i] = elem->width / etable->grid_cols;
    etable->align[i]= ALIGN_LEFT;
  } 

  pos = *startpoint;
  pos.y += elem->height/2.0;
  for (i = 0; i < etable->grid_cols - 1; i++) {
    pos.x += etable->cell_width[i];
    add_column_handle(etable,pos);
  }

  etable->draw_line = TRUE;

  /* font */
  if (etable->font == NULL) {
      etable->font_height = 0.8;
      etable->font = dia_font_new_from_style (DIA_FONT_MONOSPACE, 0.8);
  }
  etable->text_color = attributes_get_foreground();
  etable->template = g_strdup("abcdefg");

  etable->numtexts = etable->grid_rows * etable->grid_cols;
  etable->texts = g_malloc0(sizeof(gchar*)*(etable->numtexts)+1);
  for (i = 0; i < etable->numtexts; ++i) {
    *(etable->texts + i) = g_strdup(etable->template);
  }
  *(etable->texts + etable->numtexts) = NULL;

  etable->embed_id = g_strdup("embed_table");

  etable_update_data(etable);
  *handle1 = NULL;
  *handle2 = obj->handles[7];  

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
  int i;
  int num;
  AttributeNode attr;
  DataNode data;
  DiaObject *obj;
  ETable *etable;
 
  obj = object_load_using_properties(&etable_type,
    obj_node,version,filename);  
  etable = (ETable*)obj;

  etable->numtexts = etable->grid_rows * etable->grid_cols;
  etable->texts = g_malloc0(sizeof(gchar*)*(etable->numtexts+1));
  for (i = 0; i < etable->numtexts; ++i) {
    *(etable->texts + i) = g_strdup(etable->template);
  }
  *(etable->texts + etable->numtexts) = NULL;

  attr = object_find_attribute(obj_node, "etable_texts");
  if (attr != NULL) {
    num = attribute_num_data(attr);
  } else {
    num = 0;
  }

  data = attribute_first_data(attr);
  for (i=0; i<num && i<etable->numtexts;i++) {
    g_free(*(etable->texts + i));
    *(etable->texts +i) = g_strdup(data_string(data));
    data = data_next(data);
  }

  etable_eval_aligns(etable);
  etable_eval_cell_widths(etable);

  return obj; 
}

static void
etable_save(ETable *etable, ObjectNode obj_node, const char *filename)
{
  int i;
  AttributeNode attr;

  object_save_props(&etable->element.object, obj_node);

  attr = new_attribute(obj_node, "etable_texts");
  for (i=0;i<etable->numtexts;i++) {
    data_add_string(attr, *(etable->texts + i));
  }
}

