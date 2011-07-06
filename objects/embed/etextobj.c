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
#include "pixmaps/etext.xpm"
#include "utils.h"
#include "dtree.h"

#define HANDLE_TEXT HANDLE_CUSTOM1

typedef enum _Valign Valign;
enum _Valign {
        VALIGN_TOP,
        VALIGN_BOTTOM,
        VALIGN_CENTER,
        VALIGN_FIRST_LINE
};
typedef struct _ETextobj ETextobj;
struct _ETextobj {
  DiaObject object;
  gchar *name;
  
  Handle text_handle;

  Text *text;
  TextAttributes attrs;
  Valign vert_align;
  
  Color fill_color;
  gboolean show_background;

  gchar *embed_id;
  gint embed_text_size;
  gint embed_column_size;
};

static struct _ETextobjProperties {
  Alignment alignment;
  Valign vert_align;
} default_properties = { ALIGN_LEFT, VALIGN_FIRST_LINE } ;

static real textobj_distance_from(ETextobj *textobj, Point *point);
static void textobj_select(ETextobj *textobj, Point *clicked_point,
			   DiaRenderer *interactive_renderer);
static ObjectChange* textobj_move_handle(ETextobj *textobj, Handle *handle,
					 Point *to, ConnectionPoint *cp,
					 HandleMoveReason reason, ModifierKeys modifiers);
static ObjectChange* textobj_move(ETextobj *textobj, Point *to);
static void textobj_draw(ETextobj *textobj, DiaRenderer *renderer);
static void textobj_update_data(ETextobj *textobj);
static DiaObject *textobj_create(Point *startpoint,
			      DicNode *data,
			      Handle **handle1,
			      Handle **handle2);
static void textobj_destroy(ETextobj *textobj);

static PropDescription *textobj_describe_props(ETextobj *textobj);
static void textobj_get_props(ETextobj *textobj, GPtrArray *props);
static void textobj_set_props(ETextobj *textobj, GPtrArray *props);

static void textobj_save(ETextobj *textobj, ObjectNode obj_node,
			 const char *filename);
static DiaObject *textobj_load(ObjectNode obj_node, int version,
			    const char *filename);
static void textobj_valign_point(ETextobj *textobj, Point* p, real factor);

static ObjectTypeOps etextobj_type_ops =
{
  (CreateFunc) textobj_create,
  (LoadFunc)   textobj_load,
  (SaveFunc)   textobj_save,
  (GetDefaultsFunc)   NULL, 
  (ApplyDefaultsFunc) NULL
};

/* Version history:
 * Version 1 added vertical alignment, and needed old objects to use the
 *     right alignment.
 */

DiaObjectType etextobj_type =
{
  "Embed - Text",   /* name */
  1,                   /* version */
  (char **) etext_xpm,  /* pixmap */

  &etextobj_type_ops    /* ops */
};

DiaObjectType *_etextobj_type = (DiaObjectType *) &etextobj_type;

static ObjectOps etextobj_ops = {
  (DestroyFunc)         textobj_destroy,
  (DrawFunc)            textobj_draw,
  (DistanceFunc)        textobj_distance_from,
  (SelectFunc)          textobj_select,
  (CopyFunc)            object_copy_using_properties,
  (MoveFunc)            textobj_move,
  (MoveHandleFunc)      textobj_move_handle,
  (GetPropertiesFunc)   object_create_props_dialog,
  (ApplyPropertiesDialogFunc) object_apply_props_from_dialog,
  (ObjectMenuFunc)      NULL,
  (DescribePropsFunc)   textobj_describe_props,
  (GetPropsFunc)        textobj_get_props,
  (SetPropsFunc)        textobj_set_props,
  (TextEditFunc) 0,
  (ApplyPropertiesListFunc) object_apply_props,
};

PropEnumData prop_text_vert_align_data[] = {
  { N_("Bottom"), VALIGN_BOTTOM },
  { N_("Top"), VALIGN_TOP },
  { N_("Center"), VALIGN_CENTER },
  { N_("First Line"), VALIGN_FIRST_LINE },
  { NULL, 0 }
};

static PropNumData embed_text_size_range = { 1, G_MAXINT, 1 };
static PropNumData embed_column_size_range = { 0, G_MAXINT, 1 };

static PropDescription textobj_props[] = {
  OBJECT_COMMON_PROPERTIES,
  PROP_STD_TEXT_ALIGNMENT,
  { "text_vert_alignment", PROP_TYPE_ENUM, PROP_FLAG_VISIBLE | PROP_FLAG_OPTIONAL, \
    N_("Vertical text alignment"), NULL, prop_text_vert_align_data },
  PROP_STD_TEXT_FONT,
  PROP_STD_TEXT_HEIGHT,
  PROP_STD_TEXT_COLOUR,
  PROP_STD_SAVED_TEXT,
  PROP_STD_FILL_COLOUR_OPTIONAL,
  PROP_STD_SHOW_BACKGROUND_OPTIONAL,
  { "name", PROP_TYPE_STRING, PROP_FLAG_DONT_SAVE,
    N_("Name"), NULL, NULL },
  { "embed_id", PROP_TYPE_STRING, PROP_FLAG_VISIBLE,
    N_("Embed ID"), NULL, NULL },
  { "embed_text_size", PROP_TYPE_INT, 0,
    N_("Embed Text Size"), NULL, &embed_text_size_range },
  { "embed_column_size", PROP_TYPE_INT, PROP_FLAG_VISIBLE,
    N_("Embed Column Size"), NULL, &embed_column_size_range },
  PROP_DESC_END
};

static PropDescription *
textobj_describe_props(ETextobj *textobj)
{
  if (textobj_props[0].quark == 0)
    prop_desc_list_calculate_quarks(textobj_props);
  return textobj_props;
}

static PropOffset textobj_offsets[] = {
  OBJECT_COMMON_PROPERTIES_OFFSETS,
  {"text",PROP_TYPE_TEXT,offsetof(ETextobj,text)},
  {"text_font",PROP_TYPE_FONT,offsetof(ETextobj,attrs.font)},
  {PROP_STDNAME_TEXT_HEIGHT,PROP_STDTYPE_TEXT_HEIGHT,offsetof(ETextobj,attrs.height)},
  {"text_colour",PROP_TYPE_COLOUR,offsetof(ETextobj,attrs.color)},
  {"text_alignment",PROP_TYPE_ENUM,offsetof(ETextobj,attrs.alignment)},
  {"text_vert_alignment",PROP_TYPE_ENUM,offsetof(ETextobj,vert_align)},
  { "fill_colour", PROP_TYPE_COLOUR, offsetof(ETextobj, fill_color) },
  { "show_background", PROP_TYPE_BOOL, offsetof(ETextobj, show_background) },
  { "name", PROP_TYPE_STRING, offsetof(ETextobj, name) },
  { "embed_id", PROP_TYPE_STRING, offsetof(ETextobj, embed_id) },
  { "embed_text_size", PROP_TYPE_INT, offsetof(ETextobj, embed_text_size) },
  { "embed_column_size", PROP_TYPE_INT, offsetof(ETextobj, embed_column_size) },
  { NULL, 0, 0 }
};

static void
textobj_get_props(ETextobj *textobj, GPtrArray *props)
{
  text_get_attributes(textobj->text,&textobj->attrs);
  object_get_props_from_offsets(&textobj->object,textobj_offsets,props);
}

static void
textobj_set_props(ETextobj *textobj, GPtrArray *props)
{
  object_set_props_from_offsets(&textobj->object,textobj_offsets,props);
  apply_textattr_properties(props,textobj->text,"text",&textobj->attrs);
  textobj_update_data(textobj);
}

static real
textobj_distance_from(ETextobj *textobj, Point *point)
{
  return text_distance_from(textobj->text, point); 
}

static void
textobj_select(ETextobj *textobj, Point *clicked_point,
	       DiaRenderer *interactive_renderer)
{
if (clicked_point != NULL) {
fprintf(stderr,"clicked_point[%lf,%lf]\n",clicked_point->x,clicked_point->y);
}
  text_set_cursor(textobj->text, clicked_point, interactive_renderer);
  text_grab_focus(textobj->text, &textobj->object);
}



static ObjectChange*
textobj_move_handle(ETextobj *textobj, Handle *handle,
		    Point *to, ConnectionPoint *cp,
		    HandleMoveReason reason, ModifierKeys modifiers)
{
  assert(textobj!=NULL);
  assert(handle!=NULL);
  assert(to!=NULL);

  if (handle->id == HANDLE_TEXT) {
          /*Point to2 = *to;
          point_add(&to2,&textobj->text->position);
          point_sub(&to2,&textobj->text_handle.pos);
          textobj_move(textobj, &to2);*/
          textobj_move(textobj, to);
          
  }

  return NULL;
}

static ObjectChange*
textobj_move(ETextobj *textobj, Point *to)
{
  textobj->object.position = *to;

  textobj_update_data(textobj);

  return NULL;
}

static void
textobj_draw(ETextobj *textobj, DiaRenderer *renderer)
{
  assert(textobj != NULL);
  assert(renderer != NULL);

  if (textobj->show_background) {
    Rectangle box;
    Point ul, lr;
    text_calc_boundingbox (textobj->text, &box);
    ul.x = box.left;
    ul.y = box.top;
    lr.x = box.right;
    lr.y = box.bottom;
    DIA_RENDERER_GET_CLASS (renderer)->fill_rect (renderer, &ul, &lr, &textobj->fill_color);
  }
  text_draw(textobj->text, renderer);
}

static void textobj_valign_point(ETextobj *textobj, Point* p, real factor)
        /* factor should be 1 or -1 */
{
    Rectangle *bb  = &(textobj->object.bounding_box); 
    real offset ;
    switch (textobj->vert_align){
        case VALIGN_BOTTOM:
            offset = bb->bottom - textobj->object.position.y;
            p->y -= offset * factor; 
            break;
        case VALIGN_TOP:
            offset = bb->top - textobj->object.position.y;
            p->y -= offset * factor; 
            break;
        case VALIGN_CENTER:
            offset = (bb->bottom + bb->top)/2 - textobj->object.position.y;
            p->y -= offset * factor; 
            break;
        case VALIGN_FIRST_LINE:
            break;
        }
}
static void
textobj_update_data(ETextobj *textobj)
{
  Point to2;
  DiaObject *obj = &textobj->object;

  if (textobj->name != NULL) {
    g_free(textobj->name);
  }
  textobj->name = g_strdup(textobj->embed_id);
  register_embed_id(textobj->embed_id);
  
  text_set_position(textobj->text, &obj->position);
  text_calc_boundingbox(textobj->text, &obj->bounding_box);

  to2 = obj->position;
  textobj_valign_point(textobj, &to2, 1);
  text_set_position(textobj->text, &to2);
  text_calc_boundingbox(textobj->text, &obj->bounding_box);
  
  textobj->text_handle.pos = obj->position;
}

static DiaObject *
textobj_create(Point *startpoint,
	       DicNode *node, 
	       Handle **handle1,
	       Handle **handle2)
{
  ETextobj *textobj;
  DiaObject *obj;
  Color col;
  DiaFont *font = NULL;
  real font_height;
  int index=0;
  
  textobj = g_malloc0(sizeof(ETextobj));
  obj = &textobj->object;
  
  obj->type = &etextobj_type;

  obj->ops = &etextobj_ops;

  if (node != NULL) {
  	index = dnode_data_get_empty_index(node);
    textobj->embed_id = dnode_data_get_longname(node,index);
    textobj->embed_text_size = node->length;
  } else {
    textobj->embed_id = get_default_embed_id("embed_text");
  }
  textobj->embed_column_size = 0;

  col = attributes_get_foreground();
  attributes_get_default_font(&font, &font_height);
  textobj->text = new_text(textobj->embed_id, font, font_height,
			   startpoint, &col, default_properties.alignment );
  /* need to initialize to object.position as well, it is used update data */
  obj->position = *startpoint;

  text_get_attributes(textobj->text,&textobj->attrs);
  dia_font_unref(font);
  textobj->vert_align = default_properties.vert_align;
  
  /* default visibility must be off to keep compatibility */
  textobj->fill_color = attributes_get_background();
  textobj->show_background = FALSE;
  
  object_init(obj, 1, 0);

  obj->handles[0] = &textobj->text_handle;
  textobj->text_handle.id = HANDLE_TEXT;
  textobj->text_handle.type = HANDLE_MAJOR_CONTROL;
  textobj->text_handle.connect_type = HANDLE_CONNECTABLE;
  textobj->text_handle.connected_to = NULL;

  textobj_update_data(textobj);

  *handle1 = NULL;
  *handle2 = obj->handles[0];
  if (node != NULL) {
    dnode_set_object(node,index,&textobj->object);
  }
  return &textobj->object;
}

static void
textobj_destroy(ETextobj *textobj)
{
  text_destroy(textobj->text);
  dia_font_unref(textobj->attrs.font);
  object_destroy(&textobj->object);
}

static void
textobj_save(ETextobj *textobj, ObjectNode obj_node, const char *filename)
{
  object_save(&textobj->object, obj_node);

  data_add_text(new_attribute(obj_node, "text"),
		textobj->text);
  data_add_enum(new_attribute(obj_node, "valign"),
		  textobj->vert_align);

  if (textobj->show_background) {
    data_add_color(new_attribute(obj_node, "fill_color"), &textobj->fill_color);
    data_add_boolean(new_attribute(obj_node, "show_background"), textobj->show_background);
  }

  data_add_string(new_attribute(obj_node, "embed_id"),
		textobj->embed_id);
  data_add_int(new_attribute(obj_node, "embed_text_size"),
		textobj->embed_text_size);
  data_add_int(new_attribute(obj_node, "embed_column_size"),
		textobj->embed_column_size);
}

static DiaObject *
textobj_load(ObjectNode obj_node, int version, const char *filename)
{
  ETextobj *textobj;
  DiaObject *obj;
  AttributeNode attr;
  Point startpoint = {0.0, 0.0};

  textobj = g_malloc0(sizeof(ETextobj));
  obj = &textobj->object;
  
  obj->type = &etextobj_type;
  obj->ops = &etextobj_ops;

  object_load(obj, obj_node);

  attr = object_find_attribute(obj_node, "text");
  if (attr != NULL) {
    textobj->text = data_text( attribute_first_data(attr) );
  } else {
    DiaFont* font = dia_font_new_from_style(DIA_FONT_MONOSPACE,1.0);
    textobj->text = new_text("", font, 1.0,
			     &startpoint, &color_black, ALIGN_CENTER);
    dia_font_unref(font);
  }
  /* initialize attrs from text */
  text_get_attributes(textobj->text,&textobj->attrs);

  attr = object_find_attribute(obj_node, "valign");
  if (attr != NULL)
    textobj->vert_align = data_enum( attribute_first_data(attr) );
  else if (version == 0) {
    textobj->vert_align = VALIGN_FIRST_LINE;
  }

  /* default visibility must be off to keep compatibility */
  textobj->fill_color = attributes_get_background();
  attr = object_find_attribute(obj_node, "fill_color");
  if (attr)
    data_color(attribute_first_data(attr), &textobj->fill_color);
  attr = object_find_attribute(obj_node, "show_background");
  if (attr)
    textobj->show_background = data_boolean( attribute_first_data(attr) );
  else
    textobj->show_background = FALSE;

  attr = object_find_attribute(obj_node, "embed_id");
  if (attr) {
    textobj->embed_id = data_string( attribute_first_data(attr) );
  } else {
    textobj->embed_id = get_default_embed_id("embed_text");
  }
  register_embed_id(textobj->embed_id);

  attr = object_find_attribute(obj_node, "embed_text_size");
  if (attr) {
    textobj->embed_text_size = data_int( attribute_first_data(attr) );
  } else {
    textobj->embed_text_size = 10;
  }

  attr = object_find_attribute(obj_node, "embed_column_size");
  if (attr) {
    textobj->embed_column_size = data_int( attribute_first_data(attr) );
  } else {
    textobj->embed_column_size = 0;
  }

  object_init(obj, 1, 0);

  obj->handles[0] = &textobj->text_handle;
  textobj->text_handle.id = HANDLE_TEXT;
  textobj->text_handle.type = HANDLE_MAJOR_CONTROL;
  textobj->text_handle.connect_type = HANDLE_CONNECTABLE;
  textobj->text_handle.connected_to = NULL;

  textobj_update_data(textobj);

  return &textobj->object;
}
