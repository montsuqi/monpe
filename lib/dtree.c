#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <glib.h>
#include <libxml/tree.h>

#include "dtree.h"
#include "object.h"
#include "properties.h"
#include "prop_text.h"
#include "prop_inttypes.h"

DicNode *
dnode_new(char *name, int occurs, DicNodeType type, int length,
  DicNode *parent, DicNode *sibling)
{
  DicNode *node;

  node = g_malloc0(sizeof(DicNode));
  g_assert(node != NULL);

  dnode_initialize(node, name, occurs, type, length,parent, sibling);

  return node;
}

static GList *
dnode_new_objects(int size)
{
  GList *list = NULL;
  int i;
  
  for(i=0;i<size;i++) {
    list = g_list_append(list,NULL);
  }
  return list;
}

void
dnode_initialize(DicNode *node, char *name, int occurs, 
                 DicNodeType type, int length,
                 DicNode *parent, DicNode *sibling)
{
  GNode *g;
  g_assert(node != NULL);

  /* initialize parent (GNode) */
  g           = G_NODE(node);
  g->data     = NULL;
  g->next     = NULL;    
  g->prev     = NULL;
  g->parent   = NULL;
  g->children = NULL;
  
  node->istop = FALSE;
  node->name = g_strdup(name);
  node->occurs = occurs;
  node->type = type;
  node->length = length;
  node->objects = NULL;
  if (parent != NULL) {
    dtree_insert_before(parent, sibling, node);
  }
  if (type != DIC_NODE_TYPE_NODE) {
    node->objects = dnode_new_objects(dnode_calc_total_occurs(node));
  }
}

void
dnode_dump(DicNode *node)
{
  printf("dnode:%s(%d) ", node->name, node->occurs);
  switch (node->type) {
  case DIC_NODE_TYPE_NODE:
    printf("\n");
    break;
  case DIC_NODE_TYPE_TEXT:
    printf("text:%d\n",node->length);
    break;
  case DIC_NODE_TYPE_IMAGE:
    printf("image:%d\n",node->length);
    break;
  default: /* error */
    printf("\n");
  }
}

int
dnode_get_n_objects(DicNode *node)
{
  if (node->objects == NULL) {
    return 0;
  }
  return g_list_length(node->objects);
}

int 
dnode_get_n_used_objects(DicNode * node)
{
  int i,n;

  if (node->objects == NULL) {
    return 0;
  }
  for(i=n=0;i<g_list_length(node->objects);i++){
    if (g_list_nth_data(node->objects,i) != NULL) {
      n++;
    }
  }
  return n;
}

int 
dnode_calc_total_occurs(DicNode *node)
{
  if (node == NULL || DNODE_PARENT(node) == NULL) {
    return 1;
  }
  return node->occurs * dnode_calc_total_occurs(DNODE_PARENT(node));
}

int
dnode_calc_occurs_upto_parent(DicNode *parent,DicNode *node)
{
  if (node==NULL || DNODE_PARENT(node)==NULL) {
    return 1;
  }
  if (node == parent) {
    return node->occurs;
  }
  return node->occurs * 
    dnode_calc_occurs_upto_parent(parent,DNODE_PARENT(node));
}

int
dnode_calc_occurs_upto_before_parent(DicNode *parent,DicNode *node)
{
  if (node==NULL || DNODE_PARENT(node)==NULL) {
    return 1;
  }
  if (node == parent) {
    return 1;
  }
  return node->occurs * 
    dnode_calc_occurs_upto_parent(parent,DNODE_PARENT(node));
}

gboolean
dnode_data_is_used(DicNode *node)
{
  DicNode *child;
  int i;

  for(child = DNODE_CHILDREN(node); 
    child != NULL; 
    child = DNODE_NEXT(child)) {
    if (dnode_data_is_used(child)) {
      return TRUE;
    }
  }

  if (node->objects == NULL) {
    return FALSE;
  }

  for(i=0;i<g_list_length(node->objects);i++){
    if (g_list_nth_data(node->objects,i) != NULL) {
      return TRUE;
    }
  }
  return FALSE;
}

int 
dnode_data_get_empty_index(DicNode *node)
{
  int i;
  if (node->objects == NULL) {
    return -1;
  }
  for(i=0;i<g_list_length(node->objects);i++){
    if (g_list_nth_data(node->objects,i) == NULL) {
      return i;
    }
  }
  return -1;
}

gchar* 
dnode_data_get_longname(DicNode *node, int index) 
{
  gchar *names[DNODE_MAX_DEPTH];
  int occurs[DNODE_MAX_DEPTH];
  int i,j,k,l,depth;
  gchar *longname,*oldlongname;

  longname = NULL;
  depth = g_node_depth(G_NODE(node));
  if (depth >= DNODE_MAX_DEPTH) {
    fprintf(stderr,"over dnode_max_depth(%d)\n",DNODE_MAX_DEPTH);
    exit(1);
  }
  for (i=depth-1;DNODE_PARENT(node)!=NULL&&i>0;i--) {
    names[i] = node->name;
    occurs[i] = node->occurs;
    node = DNODE_PARENT(node);
  }
  for (i=1;i<depth;i++) {
    k = 1;
    for(j=i+1;j<depth;j++) {
      k *= occurs[j];
    }
    l = index/k;
    index -= k*l;
    if (i==1) {
      longname = g_strdup_printf("%s[%d]",names[i],l);
    } else {
      oldlongname = longname;
      longname = g_strdup_printf("%s.%s[%d]",longname,names[i],l);
      g_free(oldlongname);
    }
  }
  return longname;
}

static void dnode_update_object_name(DicNode *node)
{
  DicNode *child;
  DiaObject *obj;
  Property *prop;
  GPtrArray *props;
  int i;

  if (node->type == DIC_NODE_TYPE_NODE) {
    child = DNODE_CHILDREN(node);
    while(child!=NULL) {
      dnode_update_object_name(child);
      child = DNODE_NEXT(child);
    }
  } else {
    for(i=0;i<g_list_length(node->objects);i++) {
      obj = (DiaObject*)g_list_nth_data(node->objects,i);
      if (obj != NULL) {
        prop = object_prop_by_name_type(obj,"embed_id",PROP_TYPE_STRING);
        if (prop != NULL) {
          g_free(((StringProperty*)prop)->string_data);
          ((StringProperty*)prop)->string_data = 
            dnode_data_get_longname(node,i);
          props = g_ptr_array_new();
          g_ptr_array_add(props,prop);
          object_apply_props(obj,props);
        }
      }
    }
  }
}

void dnode_update_node_name(DicNode *node,gchar *newname)
{
  if (node->name != NULL) {
    g_free(node->name);
  }
  node->name = g_strdup(newname);
  dnode_update_object_name(node);
}

void dnode_set_object(DicNode *node,int index, gpointer object)
{
  if (node == NULL ||
      node->objects == NULL || 
      g_list_length(node->objects) < index) {
    return;
  }
  g_list_nth(node->objects,index)->data = object;
}

void dnode_unset_object(DicNode *node,gpointer object)
{
  int i;

  if (node == NULL || 
      node->objects == NULL) {
    return;
  }
  for(i=0;i<g_list_length(node->objects);i++) {
    if (g_list_nth_data(node->objects,i) == object) {
      g_list_nth(node->objects,i)->data = NULL;
    }
  }
}

GList *
dnode_get_objects_recursive(DicNode *node, GList *list)
{
  int i;
  DicNode *child;

  if (node->type == DIC_NODE_TYPE_NODE) {
    child = DNODE_CHILDREN(node);
    while(child != NULL) {
      list = g_list_concat(list,dnode_get_objects_recursive(child,list));
      child = DNODE_NEXT(child);
    }
  } else {
    if (node->objects != NULL) {
      for(i=0;i<g_list_length(node->objects);i++) {
        if (g_list_nth_data(node->objects,i) != NULL) {
          list = g_list_append(list,g_list_nth_data(node->objects,i));
        }
      }
    }
  }
  return list;
}


void
dnode_reset_objects(DicNode *node)
{
  if (node->objects != NULL) {
    g_list_free(node->objects);
  }
  node->objects = dnode_new_objects(dnode_calc_total_occurs(node));
}

void
dnode_reset_objects_recursive(DicNode *node)
{
  DicNode *child;

  if (node->type == DIC_NODE_TYPE_NODE) {
    child = DNODE_CHILDREN(node);
    while(child != NULL) {
      dnode_reset_objects_recursive(child);
      child = DNODE_NEXT(child);
    }
  } else {
    dnode_reset_objects(node);
  }  
}

static gboolean
dnode_is_enable_set_occurs(DicNode *parent,DicNode *node,int occurs)
{
  DicNode *child;
  int total, new, old;
  int i,j,k;

  if (node->type == DIC_NODE_TYPE_NODE) {
    child = DNODE_CHILDREN(node);
    while(child != NULL) {
      if (!dnode_is_enable_set_occurs(parent,child,occurs)) {
        return FALSE;
      }
      child = DNODE_NEXT(child);
    }
  } else {
    total = dnode_calc_total_occurs(node);
    old = dnode_calc_occurs_upto_parent(parent,node);
    new = dnode_calc_occurs_upto_before_parent(parent,node) * occurs;

    if (old <= new) {
      return TRUE;
    }
    k = 0;
    for(i=0;i<(total/old);i++) {
      for(j=0;j<old;j++) {
        if (new <= j) {
          if (g_list_nth_data(node->objects,k) != NULL) {
            return FALSE;
          }
        }
        k++;
      }
    }
  }
  return TRUE;
}

static void
dnode_set_occurs_sub(DicNode *parent,DicNode *node,int occurs)
{
  GList *list = NULL;
  DicNode *child;
  int total, new, old;
  int i,j,n;

  if (node->type == DIC_NODE_TYPE_NODE) {
    child = DNODE_CHILDREN(node);
    while(child != NULL) {
      dnode_set_occurs_sub(parent,child,occurs);
      child = DNODE_NEXT(child);
    }
  } else {
    total = dnode_calc_total_occurs(node);
    old = dnode_calc_occurs_upto_parent(parent,node);
    new = dnode_calc_occurs_upto_before_parent(parent,node) * occurs;

    for(i=0;i<(total/old);i++) {
      n = i * old;
      for(j=0;j<new;j++) {
        if (old < new) {
          if (j < old) {
            list = g_list_append(list,
              g_list_nth_data(node->objects,n));
          } else {
            list = g_list_append(list,NULL);
          }
        } else {
          list = g_list_append(list,
            g_list_nth_data(node->objects,n));
        }
        n++;
      }
    }
    g_list_free(node->objects);
    node->objects = list;
  }
}

gboolean
dnode_set_occurs(DicNode *node,int occurs)
{
  if (node->occurs == occurs) {
    return TRUE;
  }
  if (!dnode_is_enable_set_occurs(node,node,occurs)){
    return FALSE;
  }
  dnode_set_occurs_sub(node,node,occurs);
  node->occurs = occurs;
  return TRUE;
}

void
dnode_text_set_length(DicNode *node,int length)
{
  DiaObject *obj;
  Property *prop;
  GPtrArray *props;
  int i;

  if (node->type != DIC_NODE_TYPE_TEXT || node->objects == NULL) {
    return ;
  }
  node->length = length;

  for(i=0;i<g_list_length(node->objects);i++) {
    obj = (DiaObject*)g_list_nth_data(node->objects,i);
    if (obj != NULL) {
      prop = object_prop_by_name_type(obj,"embed_text_size",PROP_TYPE_INT);
      if (prop != NULL) {
        ((IntProperty*)prop)->int_data = length;
        props = g_ptr_array_new();
        g_ptr_array_add(props,prop);
        object_apply_props(obj,props);
      }
    }
  }
}

#define CAST_BAD (gchar*)

static xmlNodePtr
xml_get_child_node(xmlNodePtr root, const xmlChar *name)
{
  xmlNodePtr p;
  
  for (p = root->children; p != NULL; p = p->next) {
    if (xmlStrcmp(p->name, BAD_CAST(name)) == 0) {
      return p;
    }
  }
  
  return NULL;
}

static int
xml_get_prop_int(xmlNodePtr node, const xmlChar *name, int default_value)
{
  xmlChar *wk;
  int value;

  if (node == NULL) return default_value;
  wk = xmlGetProp(node, BAD_CAST(name));
  if (wk != NULL) {
    value = atoi((const char*)wk);
    free(wk);
  }
  else {
    value = default_value;
  }

  return value;
}

static DicNode *
dnode_new_with_xml(xmlNodePtr element,DicNode *parent,DicNode *sibling)
{
  xmlChar *name;
  xmlChar *type;
  xmlNodePtr appinfo,embed;
  DicNode *node = NULL;
  int occurs, length;

  name = xmlGetProp(element,BAD_CAST(MONPE_XML_DIC_ELEMENT_NAME));
  if (name == NULL) {
    return node;
  }

  occurs = xml_get_prop_int(element,BAD_CAST(MONPE_XML_DIC_ELEMENT_OCCURS),
    DNODE_DEFAULT_OCCURS);
  appinfo = xml_get_child_node(element,BAD_CAST(MONPE_XML_DIC_APPINFO));
  if (appinfo == NULL) {
    node = dnode_new(CAST_BAD(name),occurs,DIC_NODE_TYPE_NODE,1,parent,sibling);
  } else {
    embed = xml_get_child_node(appinfo,BAD_CAST(MONPE_XML_DIC_EMBED));
    if (embed == NULL) {
      return node;
    }
    type = xmlGetProp(embed, BAD_CAST(MONPE_XML_DIC_EMBED_OBJ));
    if (type == NULL) {
      return node;
    }
    if (!xmlStrcmp(type,BAD_CAST(MONPE_XML_DIC_EMBED_OBJ_TXT))) {
      length = xml_get_prop_int(embed,BAD_CAST(MONPE_XML_DIC_EMBED_OBJ_TXT_LEN),
        DNODE_DEFAULT_LENGTH);
      node = dnode_new(CAST_BAD(name),occurs,DIC_NODE_TYPE_TEXT,length,
        parent,sibling);
    } else if (!xmlStrcmp(type,BAD_CAST(MONPE_XML_DIC_EMBED_OBJ_IMG))) {
      node = dnode_new(CAST_BAD(name),occurs,DIC_NODE_TYPE_IMAGE,1024,
        parent,sibling);
    }
  }

  return node;
}

#define DTREE_C_BUF_SIZE 1024

static xmlNodePtr
dnode_to_xml(DicNode *node)
{
  xmlNodePtr element;
  gchar buf[DTREE_C_BUF_SIZE];

  element = xmlNewNode(NULL,BAD_CAST(MONPE_XML_DIC_ELEMENT));
  xmlSetProp(element,BAD_CAST(MONPE_XML_DIC_ELEMENT_NAME),BAD_CAST(node->name));
  g_snprintf(buf,DTREE_C_BUF_SIZE,"%d",node->occurs);
  xmlSetProp(element, BAD_CAST(MONPE_XML_DIC_ELEMENT_OCCURS),BAD_CAST(buf));
  if (node->type != DIC_NODE_TYPE_NODE) {
    xmlNodePtr appinfo;
    xmlNodePtr embed;

    appinfo = xmlNewChild(element, NULL, BAD_CAST(MONPE_XML_DIC_APPINFO), NULL);
    embed = xmlNewChild(appinfo, NULL, BAD_CAST(MONPE_XML_DIC_EMBED), NULL);

    if (node->type == DIC_NODE_TYPE_IMAGE) {
      xmlSetProp(embed,BAD_CAST(MONPE_XML_DIC_EMBED_OBJ), 
        BAD_CAST(MONPE_XML_DIC_EMBED_OBJ_IMG));
    } else if (node->type == DIC_NODE_TYPE_TEXT) {
      xmlSetProp(embed,BAD_CAST(MONPE_XML_DIC_EMBED_OBJ), 
        BAD_CAST(MONPE_XML_DIC_EMBED_OBJ_TXT));
      g_snprintf(buf,DTREE_C_BUF_SIZE,"%d",node->length);
      xmlSetProp(embed,BAD_CAST(MONPE_XML_DIC_EMBED_OBJ_TXT_LEN),BAD_CAST(buf));
    }
  }
  return element;
}

/**************************************************
 * dtree 
 **************************************************/

DicNode *
dtree_new(void)
{
  DicNode *node;
  node = dnode_new(NULL, 1, DIC_NODE_TYPE_NODE,0, NULL, NULL);
  node->istop = TRUE;
  return node;
}

void
dtree_insert_before(DicNode *parent, DicNode *sibling, DicNode *node)
{
  g_node_insert_before(G_NODE(parent), G_NODE(sibling), G_NODE(node));
}

static void
tree_dump_sub(GNode *node, gpointer data)
{
  gint indent = GPOINTER_TO_INT(data);
  int i;

  for(i=0;i<indent;i++) {
    printf("  ");
  }
  dnode_dump(DNODE(node));
  g_node_children_foreach(node, G_TRAVERSE_ALL, tree_dump_sub,
                          GINT_TO_POINTER(indent+1));
}

void
dtree_dump(DicNode *tree)
{
  gint indent = 0;
  
  printf("-----\n");
  g_node_children_foreach(G_NODE(tree), G_TRAVERSE_ALL, tree_dump_sub,
                          GINT_TO_POINTER(indent));
}

void
dtree_unlink(DicNode *tree)
{
  g_node_unlink(G_NODE(tree));
}

void 
dtree_move_before(DicNode *node,DicNode *parent,DicNode *sibling)
{
  dtree_unlink(node);
  g_node_insert_before(G_NODE(parent),G_NODE(sibling),G_NODE(node));
}

void 
dtree_move_after(DicNode *node,DicNode *parent,DicNode *sibling)
{
  dtree_unlink(node);
  g_node_insert_after(G_NODE(parent),G_NODE(sibling),G_NODE(node));
}

static void
new_from_xml_sub(xmlNodePtr xnode,DicNode *dnode)
{
  xmlNodePtr xchild;

  for(xchild = xnode->children;xchild !=NULL; xchild = xchild->next) {
    if (!xmlStrcmp(xchild->name,BAD_CAST(MONPE_XML_DIC_ELEMENT))) {
      new_from_xml_sub(xchild,dnode_new_with_xml(xchild,dnode,NULL));
    }
  }
}

void 
dtree_new_from_xml(DicNode **dnode,xmlNodePtr xnode)
{
  if (*dnode == NULL) {
    *dnode = dtree_new();
  }
  new_from_xml_sub(xnode, *dnode);
}

void 
dtree_write_to_xml(xmlNodePtr xnode,DicNode  *dnode)
{
  DicNode *p;

  for(p = DNODE_CHILDREN(dnode);p != NULL; p= DNODE_NEXT(p)) {
    xmlNodePtr xchild;
    xchild = dnode_to_xml(p);
    xmlAddChild(xnode,xchild);
    dtree_write_to_xml(xchild,p);
  }
}


DicNode *
dtree_set_data_path(DicNode *node,gchar *path,gpointer object)
{
  DicNode *p,*ret = NULL;
  int i;
  gchar *_path;

  for(p = DNODE_CHILDREN(node);p != NULL; p= DNODE_NEXT(p)) {
    ret = dtree_set_data_path(p,path,object);
  }
  if (node->type != DIC_NODE_TYPE_NODE) {
    for (i=0;i<dnode_calc_total_occurs(node);i++) {
      _path = dnode_data_get_longname(node,i);
      if (!g_strcmp0(path,_path) && 
        g_list_nth_data(node->objects,i) == NULL) {
        dnode_set_object(node,i,object);
        g_free(_path);
        return node;
      }
      g_free(_path);
    }
  }
  return ret;
}

/*************************************************************
 * Local Variables:
 * mode:c
 * tab-width:2
 * indent-tabs-mode:nil
 * End:
 ************************************************************/
