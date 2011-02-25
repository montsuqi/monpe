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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "intl.h"
#include "object.h"
#include "connectionpoint.h"
#include "diarenderer.h"
#include "font.h"
#include "text.h"
#include "attributes.h"
#include "widgets.h"
#include "properties.h"
#include "pixmaps/earray.xpm"


#define HANDLE_TEXT HANDLE_CUSTOM1

typedef struct _EArray EArray;
struct _EArray {
  DiaObject object;

  Text *template;
  TextAttributes attrs;
  
  Text **texts;
  
  Color fill_color;
  gboolean show_background;

  gchar *embed_id;
  gint embed_text_size;
  gint embed_column_size;
  gint embed_array_size;
};

static real earray_distance_from(EArray *eary, Point *point);
static void earray_select(EArray *eary, Point *clicked_point,
			   DiaRenderer *interactive_renderer);
static ObjectChange* earray_move_handle(EArray *eary, Handle *handle,
					 Point *to, ConnectionPoint *cp,
					 HandleMoveReason reason, ModifierKeys modifiers);
static ObjectChange* earray_move(EArray *eary, Point *to);
static void earray_draw(EArray *eary, DiaRenderer *renderer);
static void earray_update_data(EArray *eary);
static DiaObject *earray_create(Point *startpoint,
			      void *user_data,
			      Handle **handle1,
			      Handle **handle2);
static void earray_destroy(EArray *eary);

static PropDescription *earray_describe_props(EArray *eary);
static void earray_get_props(EArray *eary, GPtrArray *props);
static void earray_set_props(EArray *eary, GPtrArray *props);

static void earray_save(EArray *eary, ObjectNode obj_node,
			 const char *filename);
static DiaObject *earray_load(ObjectNode obj_node, int version,
			    const char *filename);

static ObjectTypeOps earray_type_ops =
{
  (CreateFunc) earray_create,
  (LoadFunc)   earray_load,
  (SaveFunc)   earray_save,
  (GetDefaultsFunc)   NULL, 
  (ApplyDefaultsFunc) NULL
};

/* Version history:
 * Version 1 added vertical alignment, and needed old objects to use the
 *     right alignment.
 */

DiaObjectType earray_type =
{
  "Embed - Array",   /* name */
  1,                   /* version */
  (char **) earray_xpm,  /* pixmap */

  &earray_type_ops    /* ops */
};

DiaObjectType *_earray_type = (DiaObjectType *) &earray_type;

static ObjectOps earray_ops = {
  (DestroyFunc)         earray_destroy,
  (DrawFunc)            earray_draw,
  (DistanceFunc)        earray_distance_from,
  (SelectFunc)          earray_select,
  (CopyFunc)            object_copy_using_properties,
  (MoveFunc)            earray_move,
  (MoveHandleFunc)      earray_move_handle,
  (GetPropertiesFunc)   object_create_props_dialog,
  (ApplyPropertiesDialogFunc) object_apply_props_from_dialog,
  (ObjectMenuFunc)      NULL,
  (DescribePropsFunc)   earray_describe_props,
  (GetPropsFunc)        earray_get_props,
  (SetPropsFunc)        earray_set_props,
  (TextEditFunc) 0,
  (ApplyPropertiesListFunc) object_apply_props,
};

static PropNumData embed_text_size_range = { 1, G_MAXINT, 1 };
static PropNumData embed_column_size_range = { 0, G_MAXINT, 1 };

static PropDescription earray_props[] = {
  OBJECT_COMMON_PROPERTIES,
  PROP_STD_TEXT_ALIGNMENT,
  PROP_STD_TEXT_FONT,
  PROP_STD_TEXT_HEIGHT,
  PROP_STD_TEXT_COLOUR,
  PROP_STD_SAVED_TEXT,
  PROP_STD_FILL_COLOUR_OPTIONAL,
  PROP_STD_SHOW_BACKGROUND_OPTIONAL,
  { "embed_id", PROP_TYPE_STRING, PROP_FLAG_VISIBLE,
    N_("Embed ID"), NULL, NULL },
  { "embed_array_size", PROP_TYPE_INT, PROP_FLAG_VISIBLE,
    N_("Embed Array Size"), NULL, &embed_text_size_range },
  { "embed_text_size", PROP_TYPE_INT, PROP_FLAG_VISIBLE,
    N_("Embed Text Size"), NULL, &embed_text_size_range },
  { "embed_column_size", PROP_TYPE_INT, PROP_FLAG_VISIBLE,
    N_("Embed Column Size"), NULL, &embed_column_size_range },
  PROP_DESC_END
};

static PropDescription *
earray_describe_props(EArray *eary)
{
  if (earray_props[0].quark == 0)
    prop_desc_list_calculate_quarks(earray_props);
  return earray_props;
}

static PropOffset earray_offsets[] = {
  OBJECT_COMMON_PROPERTIES_OFFSETS,
  {"text",PROP_TYPE_TEXT,offsetof(EArray,template)},
  {"text_font",PROP_TYPE_FONT,offsetof(EArray,attrs.font)},
  {PROP_STDNAME_TEXT_HEIGHT,PROP_STDTYPE_TEXT_HEIGHT,offsetof(EArray,attrs.height)},
  {"text_colour",PROP_TYPE_COLOUR,offsetof(EArray,attrs.color)},
  {"text_alignment",PROP_TYPE_ENUM,offsetof(EArray,attrs.alignment)},
  { "fill_colour", PROP_TYPE_COLOUR, offsetof(EArray, fill_color) },
  { "show_background", PROP_TYPE_BOOL, offsetof(EArray, show_background) },
  { "embed_id", PROP_TYPE_STRING, offsetof(EArray, embed_id) },
  { "embed_array_size", PROP_TYPE_INT, offsetof(EArray, embed_array_size) },
  { "embed_text_size", PROP_TYPE_INT, offsetof(EArray, embed_text_size) },
  { "embed_column_size", PROP_TYPE_INT, offsetof(EArray, embed_column_size) },
  { NULL, 0, 0 }
};

static void
earray_get_props(EArray *eary, GPtrArray *props)
{
  text_get_attributes(eary->template,&eary->attrs);
  object_get_props_from_offsets(&eary->object,earray_offsets,props);
}

static void
earray_set_props(EArray *eary, GPtrArray *props)
{
  object_set_props_from_offsets(&eary->object,earray_offsets,props);
  apply_textattr_properties(props,eary->template,"text",&eary->attrs);
  earray_update_data(eary);
}

static real
earray_distance_from(EArray *eary, Point *point)
{
  return text_distance_from(*(eary->texts), point); 
}

static void
earray_select(EArray *eary, Point *clicked_point,
	       DiaRenderer *interactive_renderer)
{
fprintf(stderr,"select clicked_point[%p]\n",clicked_point);
#if 0
  text_set_cursor(eary->text, clicked_point, interactive_renderer);
  text_grab_focus(eary->text, &eary->object);
#endif
}



static ObjectChange*
earray_move_handle(EArray *eary, Handle *handle,
		    Point *to, ConnectionPoint *cp,
		    HandleMoveReason reason, ModifierKeys modifiers)
{
  DiaObject *obj;
  int i;  

  assert(eary!=NULL);
  assert(handle!=NULL);
  assert(to!=NULL);

  obj = &eary->object;
  if (handle->id == HANDLE_TEXT) {
    for(i=0;i<obj->num_handles;i++) {
      if (handle == obj->handles[i]) {
        text_set_position(*(eary->texts+i),to);
        obj->handles[i]->pos = *to;
        break;
      }
    }
  }
  earray_update_data(eary);

  return NULL;
}

static ObjectChange*
earray_move(EArray *eary, Point *to)
{
  Point move;
  Point pos;
  int i;

  pos = (*(eary->texts))->position;
  move = *to;
  point_sub(&move,&pos);

  for(i=0;i<eary->embed_array_size;i++){
    pos = (*(eary->texts+i))->position;
    point_add(&pos,&move);
    text_set_position(*(eary->texts+i),&pos);
  }

  earray_update_data(eary);

  return NULL;
}

static void
earray_draw(EArray *eary, DiaRenderer *renderer)
{
  int i;
  Text *text;

  assert(eary != NULL);
  assert(renderer != NULL);

  for (i=0;i<eary->embed_array_size;i++) {
    text = *(eary->texts+i);
    if (eary->show_background) {
      Rectangle box;
      Point ul, lr;
      text_calc_boundingbox (text, &box);
      ul.x = box.left;
      ul.y = box.top;
      lr.x = box.right;
      lr.y = box.bottom;
      DIA_RENDERER_GET_CLASS (renderer)->fill_rect (renderer, &ul, &lr, &eary->fill_color);
    }
    text_draw(text, renderer);
  }
}

static void
earray_calc_boundingbox(EArray *eary)
{
  DiaObject *obj = &eary->object;
  Rectangle bb;
  int i;

  for(i = 0; i < eary->embed_array_size; i++) {
    text_calc_boundingbox(*(eary->texts+i), &bb);
    rectangle_union(&obj->bounding_box,&bb);
  }
}

static void
earray_update_data(EArray *eary)
{
  DiaObject *obj = &eary->object;
  Point pos;
  int i;

  earray_calc_boundingbox(eary);
  obj->position = ((Text*)*(eary->texts))->position;
  for(i=0;i<eary->embed_array_size;i++) {
    obj->handles[i]->pos = ((Text*)*(eary->texts+i))->position;
  }
  for(i=0;i<eary->embed_array_size;i++) {
    pos = ((Text*)*(eary->texts+i))->position;
    text_set_attributes(*(eary->texts+i),&eary->attrs);
    text_set_position(*(eary->texts+i),&pos);
  }
}

static void
add_handle(EArray *eary)
{
  DiaObject *obj;
  Handle *handle;

  obj = &(eary->object);
  handle = g_malloc0(sizeof(Handle));
  handle->id = HANDLE_TEXT;
  handle->type = HANDLE_MAJOR_CONTROL;
  handle->connect_type = HANDLE_NONCONNECTABLE;
  handle->connected_to = NULL;
  object_add_handle(obj,handle);
}

static void
remove_handle(EArray *eary)
{
  DiaObject *obj;
  Handle *handle;

  obj = &(eary->object);
  if (obj->num_handles > 0) {
    handle = obj->handles[obj->num_handles-1];
    if (handle->id == HANDLE_TEXT) {
      object_remove_handle(obj,handle);
    }
  }
}

static DiaObject *
earray_create(Point *startpoint,
	       void *user_data,
	       Handle **handle1,
	       Handle **handle2)
{
  EArray *eary;
  DiaObject *obj;
  Color col;
  DiaFont *font = NULL;
  real font_height;
  Point pos;
  gchar buf[256];
  int i;
  
  eary = g_malloc0(sizeof(EArray));
  obj = &eary->object;
  
  obj->type = &earray_type;

  obj->ops = &earray_ops;

  col = attributes_get_foreground();
  attributes_get_default_font(&font, &font_height);
  eary->template = new_text("", font, font_height,
			   startpoint, &col, ALIGN_LEFT );
  /* need to initialize to object.position as well, it is used update data */
  obj->position = *startpoint;

  text_get_attributes(eary->template,&eary->attrs);
  dia_font_unref(font);
  
  /* default visibility must be off to keep compatibility */
  eary->fill_color = attributes_get_background();
  eary->show_background = FALSE;

  eary->embed_id = g_strdup("embed_array");
  eary->embed_array_size = 10;
  eary->embed_text_size = 10;
  eary->embed_column_size = 0;
  
  object_init(obj, 0, 0);

  eary->texts = g_malloc0(sizeof(Text*)*(eary->embed_array_size));

  pos = *startpoint;
  for(i=0;i<eary->embed_array_size;i++) {
    add_handle(eary);

    g_snprintf(buf,sizeof(buf),"%s[%d]",eary->embed_id,i);
    *(eary->texts + i) = new_text(buf, font, font_height,
      &pos, &col, ALIGN_LEFT );
    pos.y += font_height + 0.1;
  }

  earray_update_data(eary);

  *handle1 = NULL;
  *handle2 = obj->handles[0];
  return &eary->object;
}

static void
earray_destroy(EArray *eary)
{
  int i;
  for(i=0;i<eary->embed_array_size;i++) {
    text_destroy(*(eary->texts+i));
  }
  g_free(eary->texts);
  text_destroy(eary->template);
  dia_font_unref(eary->attrs.font);
  object_destroy(&eary->object);
}

static void
earray_save(EArray *eary, ObjectNode obj_node, const char *filename)
{
  AttributeNode attr;
  int i;

  object_save(&eary->object, obj_node);

  data_add_text(new_attribute(obj_node, "template"),
		eary->template);

  if (eary->show_background) {
    data_add_color(new_attribute(obj_node, "fill_color"), &eary->fill_color);
    data_add_boolean(new_attribute(obj_node, "show_background"), eary->show_background);
  }

  data_add_string(new_attribute(obj_node, "embed_id"),
		eary->embed_id);
  data_add_int(new_attribute(obj_node, "embed_array_size"),
		eary->embed_array_size);
  data_add_int(new_attribute(obj_node, "embed_text_size"),
		eary->embed_text_size);
  data_add_int(new_attribute(obj_node, "embed_column_size"),
		eary->embed_column_size);

  attr = new_attribute(obj_node, "earray_texts");
  for (i=0;i<eary->embed_array_size;i++) {
    data_add_text(attr, *(eary->texts + i));
  }
}

static DiaObject *
earray_load(ObjectNode obj_node, int version, const char *filename)
{
  EArray *eary;
  DiaObject *obj;
  AttributeNode attr;
  DataNode data;
  Point startpoint = {0.0, 0.0};
  Text *text;
  gchar buf[256];
  int i;
  int num;

  eary = g_malloc0(sizeof(EArray));
  obj = &eary->object;
  
  obj->type = &earray_type;
  obj->ops = &earray_ops;

  object_load(obj, obj_node);

  attr = object_find_attribute(obj_node, "template");
  if (attr != NULL) {
    eary->template = data_text( attribute_first_data(attr) );
  } else {
    DiaFont* font = dia_font_new_from_style(DIA_FONT_MONOSPACE,1.0);
    eary->template = new_text("", font, 1.0,
			     &startpoint, &color_black, ALIGN_LEFT);
    dia_font_unref(font);
  }
  /* initialize attrs from text */
  text_get_attributes(eary->template,&eary->attrs);

  /* default visibility must be off to keep compatibility */
  eary->fill_color = attributes_get_background();
  attr = object_find_attribute(obj_node, "fill_color");
  if (attr)
    data_color(attribute_first_data(attr), &eary->fill_color);
  attr = object_find_attribute(obj_node, "show_background");
  if (attr)
    eary->show_background = data_boolean( attribute_first_data(attr) );
  else
    eary->show_background = FALSE;

  attr = object_find_attribute(obj_node, "embed_id");
  if (attr)
    eary->embed_id = data_string( attribute_first_data(attr) );

  attr = object_find_attribute(obj_node, "embed_array_size");
  if (attr)
    eary->embed_array_size = data_int( attribute_first_data(attr) );

  attr = object_find_attribute(obj_node, "embed_text_size");
  if (attr)
    eary->embed_text_size = data_int( attribute_first_data(attr) );

  attr = object_find_attribute(obj_node, "embed_column_size");
  if (attr)
    eary->embed_column_size = data_int( attribute_first_data(attr) );

  object_init(obj, 0, 0);
  for(i=0;i<eary->embed_array_size;i++) {
    add_handle(eary);
  }

  eary->texts = g_malloc0(
    sizeof(Text*)*(eary->embed_array_size));

  attr = object_find_attribute(obj_node, "earray_texts");
  if (attr != NULL) {
    num = attribute_num_data(attr);
  } else {
    num = 0;
  }

  data = attribute_first_data(attr);
  for (i=0; i<num && i<eary->embed_array_size;i++) {
    *(eary->texts+i) = data_text(data);
    data = data_next(data);
  }
  if (num < eary->embed_array_size) {
    for (i=num; i<eary->embed_array_size;i++) {
      g_snprintf(buf,sizeof(buf),"%s[%d]",eary->embed_id,i);
      text = text_copy(eary->template);
      text_set_string(text,buf);
      text->position.x += i * text->height;
      *(eary->texts+i) = text;
    }
  }

  earray_update_data(eary);

  return &eary->object;
}
