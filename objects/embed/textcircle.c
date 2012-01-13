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

#include "pixmaps/textcircle.xpm"

#define HANDLE_TEXT HANDLE_CUSTOM1

typedef struct _TCircle TCircle;
struct _TCircle {
  DiaObject object;
  
  Handle text_handle;

  Text *text;
  TextAttributes attrs;
};

static struct _TCircleProperties {
  Alignment alignment;
} default_properties = { ALIGN_LEFT } ;

static real tcircle_distance_from(TCircle *tcircle, Point *point);
static void tcircle_select(TCircle *tcircle, Point *clicked_point,
			   DiaRenderer *interactive_renderer);
static ObjectChange* tcircle_move_handle(TCircle *tcircle, Handle *handle,
					 Point *to, ConnectionPoint *cp,
					 HandleMoveReason reason, ModifierKeys modifiers);
static ObjectChange* tcircle_move(TCircle *tcircle, Point *to);
static void tcircle_draw(TCircle *tcircle, DiaRenderer *renderer);
static void tcircle_update_data(TCircle *tcircle);
static DiaObject *tcircle_create(Point *startpoint,
			      void *user_data,
			      Handle **handle1,
			      Handle **handle2);
static void tcircle_destroy(TCircle *tcircle);

static PropDescription *tcircle_describe_props(TCircle *tcircle);
static void tcircle_get_props(TCircle *tcircle, GPtrArray *props);
static void tcircle_set_props(TCircle *tcircle, GPtrArray *props);

static void tcircle_save(TCircle *tcircle, ObjectNode obj_node,
			 const char *filename);
static DiaObject *tcircle_load(ObjectNode obj_node, int version,
			    const char *filename);

static ObjectTypeOps tcircle_type_ops =
{
  (CreateFunc) tcircle_create,
  (LoadFunc)   tcircle_load,
  (SaveFunc)   tcircle_save,
  (GetDefaultsFunc)   NULL, 
  (ApplyDefaultsFunc) NULL
};

/* Version history:
 * Version 1 added vertical alignment, and needed old objects to use the
 *     right alignment.
 */

DiaObjectType tcircle_type =
{
  "ORCA - TextCircle",   /* name */
  1,                   /* version */
  (char **) textcircle_xpm,  /* pixmap */

  &tcircle_type_ops    /* ops */
};

DiaObjectType *_tcircle_type = (DiaObjectType *) &tcircle_type;

static ObjectOps tcircle_ops = {
  (DestroyFunc)         tcircle_destroy,
  (DrawFunc)            tcircle_draw,
  (DistanceFunc)        tcircle_distance_from,
  (SelectFunc)          tcircle_select,
  (CopyFunc)            object_copy_using_properties,
  (MoveFunc)            tcircle_move,
  (MoveHandleFunc)      tcircle_move_handle,
  (GetPropertiesFunc)   object_create_props_dialog,
  (ApplyPropertiesDialogFunc) object_apply_props_from_dialog,
  (ObjectMenuFunc)      NULL,
  (DescribePropsFunc)   tcircle_describe_props,
  (GetPropsFunc)        tcircle_get_props,
  (SetPropsFunc)        tcircle_set_props,
  (TextEditFunc) 0,
  (ApplyPropertiesListFunc) object_apply_props,
};

static PropDescription tcircle_props[] = {
  OBJECT_COMMON_PROPERTIES,
  PROP_STD_TEXT_ALIGNMENT,
  PROP_STD_TEXT_FONT,
  PROP_STD_TEXT_HEIGHT,
  PROP_STD_TEXT_COLOUR,
  PROP_STD_SAVED_TEXT,
  PROP_DESC_END
};

static PropDescription *
tcircle_describe_props(TCircle *tcircle)
{
  if (tcircle_props[0].quark == 0)
    prop_desc_list_calculate_quarks(tcircle_props);
  return tcircle_props;
}

static PropOffset tcircle_offsets[] = {
  OBJECT_COMMON_PROPERTIES_OFFSETS,
  {"text",PROP_TYPE_TEXT,offsetof(TCircle,text)},
  {"text_font",PROP_TYPE_FONT,offsetof(TCircle,attrs.font)},
  {PROP_STDNAME_TEXT_HEIGHT,PROP_STDTYPE_TEXT_HEIGHT,offsetof(TCircle,attrs.height)},
  {"text_colour",PROP_TYPE_COLOUR,offsetof(TCircle,attrs.color)},
  {"text_alignment",PROP_TYPE_ENUM,offsetof(TCircle,attrs.alignment)},
  { NULL, 0, 0 }
};

static void
tcircle_get_props(TCircle *tcircle, GPtrArray *props)
{
  text_get_attributes(tcircle->text,&tcircle->attrs);
  object_get_props_from_offsets(&tcircle->object,tcircle_offsets,props);
}

static void
tcircle_set_props(TCircle *tcircle, GPtrArray *props)
{
  object_set_props_from_offsets(&tcircle->object,tcircle_offsets,props);
  apply_textattr_properties(props,tcircle->text,"text",&tcircle->attrs);
  tcircle_update_data(tcircle);
}

static real
tcircle_distance_from(TCircle *tcircle, Point *point)
{
  return text_distance_from(tcircle->text, point); 
}

static void
tcircle_select(TCircle *tcircle, Point *clicked_point,
	       DiaRenderer *interactive_renderer)
{
  text_set_cursor(tcircle->text, clicked_point, interactive_renderer);
  text_grab_focus(tcircle->text, &tcircle->object);
}



static ObjectChange*
tcircle_move_handle(TCircle *tcircle, Handle *handle,
		    Point *to, ConnectionPoint *cp,
		    HandleMoveReason reason, ModifierKeys modifiers)
{
  assert(tcircle!=NULL);
  assert(handle!=NULL);
  assert(to!=NULL);

  if (handle->id == HANDLE_TEXT) {
          /*Point to2 = *to;
          point_add(&to2,&tcircle->text->position);
          point_sub(&to2,&tcircle->text_handle.pos);
          tcircle_move(TCircle, &to2);*/
          tcircle_move(tcircle, to);
          
  }

  return NULL;
}

static ObjectChange*
tcircle_move(TCircle *tcircle, Point *to)
{
  tcircle->object.position = *to;

  tcircle_update_data(tcircle);

  return NULL;
}

static void
tcircle_draw(TCircle *tcircle, DiaRenderer *renderer)
{
  DiaRendererClass *renderer_ops = DIA_RENDERER_GET_CLASS (renderer);
  DiaObject *obj;
  Point center;
  real bh,bw;

  assert(tcircle != NULL);
  assert(renderer != NULL);

  obj = &tcircle->object;

  renderer_ops->set_linewidth(renderer, 0.0);
  renderer_ops->set_linestyle(renderer, LINESTYLE_SOLID);
  renderer_ops->set_linejoin(renderer, LINEJOIN_MITER);

  bh = obj->bounding_box.bottom - obj->bounding_box.top;
  bw = obj->bounding_box.right - obj->bounding_box.left;
  center.x = obj->bounding_box.left + bw / 2;
  center.y = obj->bounding_box.top + bh / 2;

  renderer_ops->draw_ellipse(renderer, &center,
			      bh, bh,
			      &tcircle->attrs.color);

  text_draw(tcircle->text, renderer);
}

static void
tcircle_update_data(TCircle *tcircle)
{
  DiaObject *obj = &tcircle->object;
  real width;
  
  text_set_position(tcircle->text, &obj->position);
  text_calc_boundingbox(tcircle->text, &obj->bounding_box);
  width = obj->bounding_box.right - obj->bounding_box.left;
  obj->bounding_box.left -= width;
  obj->bounding_box.right += width;
  
  tcircle->text_handle.pos = obj->position;
}

static DiaObject *
tcircle_create(Point *startpoint,
	       void *user_data,
	       Handle **handle1,
	       Handle **handle2)
{
  TCircle *tcircle;
  DiaObject *obj;
  Color col;
  DiaFont *font = NULL;
  real font_height;
  
  tcircle = g_malloc0(sizeof(TCircle));
  obj = &tcircle->object;
  
  obj->type = &tcircle_type;

  obj->ops = &tcircle_ops;

  col = attributes_get_foreground();
  attributes_get_default_font(&font, &font_height);
  tcircle->text = new_text("", font, font_height,
			   startpoint, &col, default_properties.alignment );
  /* need to initialize to object.position as well, it is used update data */
  obj->position = *startpoint;

  text_get_attributes(tcircle->text,&tcircle->attrs);
  dia_font_unref(font);
  
  object_init(obj, 1, 0);

  obj->handles[0] = &tcircle->text_handle;
  tcircle->text_handle.id = HANDLE_TEXT;
  tcircle->text_handle.type = HANDLE_MAJOR_CONTROL;
  tcircle->text_handle.connect_type = HANDLE_CONNECTABLE;
  tcircle->text_handle.connected_to = NULL;

  tcircle_update_data(tcircle);

  *handle1 = NULL;
  *handle2 = obj->handles[0];
  return &tcircle->object;
}

static void
tcircle_destroy(TCircle *tcircle)
{
  text_destroy(tcircle->text);
  dia_font_unref(tcircle->attrs.font);
  object_destroy(&tcircle->object);
}

static void
tcircle_save(TCircle *tcircle, ObjectNode obj_node, const char *filename)
{
  object_save(&tcircle->object, obj_node);

  data_add_text(new_attribute(obj_node, "text"),
		tcircle->text);
}

static DiaObject *
tcircle_load(ObjectNode obj_node, int version, const char *filename)
{
  TCircle *tcircle;
  DiaObject *obj;
  AttributeNode attr;
  Point startpoint = {0.0, 0.0};

  tcircle = g_malloc0(sizeof(TCircle));
  obj = &tcircle->object;
  
  obj->type = &tcircle_type;
  obj->ops = &tcircle_ops;

  object_load(obj, obj_node);

  attr = object_find_attribute(obj_node, "text");
  if (attr != NULL) {
    tcircle->text = data_text( attribute_first_data(attr) );
  } else {
    DiaFont* font = dia_font_new_from_style(DIA_FONT_MONOSPACE,1.0);
    tcircle->text = new_text("", font, 1.0,
			     &startpoint, &color_black, ALIGN_CENTER);
    dia_font_unref(font);
  }
  /* initialize attrs from text */
  text_get_attributes(tcircle->text,&tcircle->attrs);

  object_init(obj, 1, 0);

  obj->handles[0] = &tcircle->text_handle;
  tcircle->text_handle.id = HANDLE_TEXT;
  tcircle->text_handle.type = HANDLE_MAJOR_CONTROL;
  tcircle->text_handle.connect_type = HANDLE_CONNECTABLE;
  tcircle->text_handle.connected_to = NULL;

  tcircle_update_data(tcircle);

  return &tcircle->object;
}
