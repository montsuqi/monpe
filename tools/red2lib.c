/* -*- Mode: C; c-basic-offset: 2 -*-
 *
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "red2lib.h"

gchar*
EscapeNodeName(gchar *_name)
{
  gchar *name;
  GRegex *reg;

  reg = g_regex_new("-",0,0,NULL);
  name = g_regex_replace(reg,_name,-1,0,"_",0,NULL);
  g_regex_unref(reg);

  return name;
}

xmlNodePtr
FindNodeByTag (xmlNodePtr p, xmlChar *name)
{
  while (p && 0 != xmlStrcmp(p->name, name))
    p = p->next;
  return p;
}

xmlNodePtr
GetChildByTag(xmlNodePtr node,
  xmlChar *tag)
{
  xmlNodePtr child = node->children;

  while(child != NULL) {
    if (!xmlStrcmp(child->name,tag)) {
      return child;
    }
    child = child->next;
  }
  return NULL;
}

xmlNodePtr
GetChildByTagAttr(xmlNodePtr node,
  xmlChar *tag,
  xmlChar *attr,
  xmlChar *val)
{
  xmlNodePtr child = node->children;
  xmlChar *v;

  while(child != NULL) {
    if (!xmlStrcmp(child->name,tag)) {
      v = xmlGetProp(child, attr);
      if (v != NULL && !xmlStrcmp(v, val)) {
        return child;
      }
    }
    child = child->next;
  }
  return NULL;
}

xmlChar*
GetAttributeString(xmlNodePtr node,
  xmlChar *propname)
{
  xmlNodePtr child = node->children;
  xmlNodePtr child2;
  xmlChar *name;
  xmlChar *content = NULL;
  xmlChar *ret = NULL;
  GRegex *regex;
  GMatchInfo *match_info;

  regex = g_regex_new("^#(.*)#$",G_REGEX_MULTILINE,0,NULL);

  while(child != NULL) {
    name = xmlGetProp(child,BAD_CAST("name"));
    if (name != NULL && !xmlStrcmp(name, propname)) {
      child2 = child->children;
      while(child2 != NULL) {
        if (child2->type == XML_ELEMENT_NODE && 
          !xmlStrcmp(child2->name,BAD_CAST("string"))) {
          content = xmlNodeGetContent(child2);
          if (content != NULL) {
            g_regex_match(regex,CAST_BAD(content),0,&match_info);
            if (g_match_info_matches(match_info)) {
              ret = (xmlChar*)g_match_info_fetch(match_info,1);
            }
            g_match_info_free(match_info);
          }
        } 
        child2 = child2->next;
      }
    }
    if (name != NULL) {
      xmlFree(name);
    }
    child = child->next;
  }

  g_regex_unref(regex);
  return ret;
}

int
GetAttributeInt(xmlNodePtr node,
  xmlChar *propname)
{
  xmlNodePtr child = node->children;
  xmlNodePtr child2;
  xmlChar *name;
  xmlChar *val;
  int ret = -1;

  while(child != NULL) {
    name = xmlGetProp(child,BAD_CAST("name"));
    if (name != NULL && !xmlStrcmp(name, propname)) {
      child2 = child->children;
      while(child2 != NULL) {
        if (child2->type == XML_ELEMENT_NODE && 
          !xmlStrcmp(child2->name,BAD_CAST("int"))) {
          val = xmlGetProp(child2,BAD_CAST("val"));
          if (val != NULL) {
            ret = atoi((char*)val);
            xmlFree(val);
          }
        } 
        child2 = child2->next;
      }
    }
    if (name != NULL) {
      xmlFree(name);
    }
    child = child->next;
  }
  return ret;
}

static EmbedInfo*
GetEmbedInfoText(xmlNodePtr node)
{
  EmbedInfo *info = NULL;
  xmlChar *id;
  int text_size, column_size;

  if (node == NULL) {
    return info;
  }
  id = GetAttributeString(node,BAD_CAST("embed_id"));
  text_size = GetAttributeInt(node,BAD_CAST("embed_text_size"));
  column_size = GetAttributeInt(node,BAD_CAST("embed_column_size"));

  if (id != NULL && text_size != -1 && column_size != -1) {
    info = g_new0(EmbedInfo,1);
    info->id = EscapeNodeName(CAST_BAD(id));
    info->type = EMBED_TYPE_TEXT;
    info->node = node;
    EmbedInfoAttr(info,Text,text_size) = text_size;
    EmbedInfoAttr(info,Text,column_size) = column_size;
  }
  return info;
}

static EmbedInfo*
GetEmbedInfoImage(xmlNodePtr node)
{
  EmbedInfo *info = NULL;
  xmlChar *id;
  int path_size;

  if (node == NULL) {
    return info;
  }

  id = GetAttributeString(node,BAD_CAST("embed_id"));
  path_size = GetAttributeInt(node,BAD_CAST("embed_path_size"));

  if (id != NULL && path_size != -1) {
    info = g_new0(EmbedInfo,1);
    info->id = EscapeNodeName(CAST_BAD(id));
    info->type = EMBED_TYPE_IMAGE;
    info->node = node;
    EmbedInfoAttr(info,Image,path_size) = path_size;
  }

  return info;
}

static EmbedInfo*
GetEmbedInfo(xmlNodePtr node)
{
  EmbedInfo *info = NULL;
  xmlChar *type;

  type = xmlGetProp(node,BAD_CAST("type"));
  if (type == NULL) {
    return info;
  }
  if (!xmlStrcmp(type,BAD_CAST("Embed - Text"))) {
    info = GetEmbedInfoText(node);
  } else if (!xmlStrcmp(type,BAD_CAST("Embed - Image"))) {
    info = GetEmbedInfoImage(node);
  }
  xmlFree(type);

  return info;
}

static gint
EmbedInfoCompare(gconstpointer a,
  gconstpointer b,
  gpointer user_data)
{
  EmbedInfo **p1,**p2;
  gchar *s1,*s2;
  int len1,len2,min,res;

  p1 = (EmbedInfo**)a;
  p2 = (EmbedInfo**)b;

  len1 = strlen((*p1)->id);
  len2 = strlen((*p2)->id);
  min = len1 < len2 ? len1 : len2;

  s1 = g_strndup((*p1)->id,min);
  s2 = g_strndup((*p2)->id,min);

  res = strcmp(s1,s2);
  if (res == 0) {
    if (len1 > len2) {
      return -1;
    } else if (len1 < len2 ){
      return 1;
    } else {
      return 0;
    }
  }
  g_free(s1);
  g_free(s2);
  return res;
}

static void
_GetEmbedInfoList(xmlNodePtr node,
  GPtrArray *array)
{
  xmlNodePtr child;
  EmbedInfo *info;

  if (node == NULL) {
    return ;
  }
  if (!xmlStrcmp(node->name,BAD_CAST("object"))) {
    if ((info = GetEmbedInfo(node)) != NULL) {
      g_ptr_array_add(array,info);
    }
    return;
  }

  for(child = node->children; child != NULL; child = child->next)
  {
    _GetEmbedInfoList(child,array);
  }
}

GPtrArray* 
GetEmbedInfoList(xmlDocPtr doc)
{
  GPtrArray *array;

  array = g_ptr_array_new();
  if (doc == NULL) {
    g_warning("doc is NULL");
    return array;
  }
  _GetEmbedInfoList(xmlDocGetRootElement(doc),array);
  g_ptr_array_sort(array,(GCompareFunc)EmbedInfoCompare);
  return array;
}

static void
rec_print_tab(GString *rec,int l,char *format,...)
{
  int i;
  va_list va;

  for (i=0;i<l;i++) {
    g_string_append_printf(rec,"  ");
  }
  va_start(va,format);
  g_string_append_vprintf(rec,format,va);
  va_end(va);
}

static void
print_rec(GString *rec,int l,DicNode *node)
{
  DicNode *child;
  gchar *name;

  name = EscapeNodeName(node->name);

  if (node->type == DIC_NODE_TYPE_NODE) {
    rec_print_tab(rec,l,"%s {\n",name);
    for(child=DNODE_CHILDREN(node);child!=NULL;child=DNODE_NEXT(child)) {
      print_rec(rec,l+1,child);
    }
    if (node->occurs > 1) {
      rec_print_tab(rec,l,"}[%d];\n",node->occurs);
    } else {
      rec_print_tab(rec,l,"};\n");
    }
  } else if(node->type == DIC_NODE_TYPE_TEXT){
    if (node->occurs > 1) {
      rec_print_tab(rec,l,"%s varchar(%d)[%d];\n",
        name,
        node->length,
        node->occurs);
    } else {
      rec_print_tab(rec,l,"%s varchar(%d);\n",
        name,
        node->length);
    }
  } else if(node->type == DIC_NODE_TYPE_IMAGE){
    if (node->occurs > 1) {
      rec_print_tab(rec,l,"%s varchar(%d)[%d];\n",
        name,
        DNODE_IMAGE_PATH_SIZE,
        node->occurs);
    } else {
      rec_print_tab(rec,l,"%s varchar(%d);\n",
        name,
        DNODE_IMAGE_PATH_SIZE);
    }
  }
  g_free(name);
}

GString*
red2rec(xmlDocPtr doc)
{
  xmlNodePtr dict;
  DicNode *dtree;
  DicNode *child;
  GString *rec;

  if (doc == NULL) {
    fprintf(stderr, "Error: xmlDocPtr doc is NULL\n");
    exit(1);
  }
  dict = FindNodeByTag(doc->xmlRootNode->xmlChildrenNode,
    BAD_CAST(MONPE_XML_DICTIONARY));
  if (dict == NULL) {
    fprintf(stderr,"no dictionary data\n");
    exit(1);
  }
  dtree = dtree_new();
  dtree_new_from_xml(&dtree,dict);

  rec = g_string_new("red2rec {\n");
  for (child = DNODE_CHILDREN(dtree);child!=NULL;child=DNODE_NEXT(child)) {
    print_rec(rec,1,child);
  }
  g_string_append_printf(rec,"};\n");
  return rec;
}

static void
inc_print_node(GString *inc,int l,char *format,...)
{
  int i;
  va_list va;

  g_string_append_printf(inc,"            ");
  for (i=0;i<l;i++) {
    g_string_append_printf(inc,"  ");
  }
  va_start(va,format);
  g_string_append_vprintf(inc,format,va);
  va_end(va);
}

static void
inc_print_occurs(GString *inc,int occ)
{
  g_string_append_printf(inc,
"                                        OCCURS %d TIMES.\n",occ);
}

static void
inc_print_pic(GString *inc,int x)
{
  g_string_append_printf(inc,
"                        PIC X(%d).\n",x);
}

static void
inc_print_pic_occurs(GString *inc,int x,int occ)
{
  int i;
  gchar buf[256];

  sprintf(buf,
"                        PIC X(%d)",x);
  g_string_append_printf(inc,"%s",buf);
  for(i=strlen(buf);i<40;i++) {
    g_string_append_printf(inc," ");
  }
  g_string_append_printf(inc,"OCCURS %d TIMES.\n",occ);
}

static void
print_inc(GString *inc,int l,DicNode *node,gchar *prefix)
{
  DicNode *child;
  gchar *name;

  inc_print_node(inc,l,"%02d  %s-%s",l+2,prefix,node->name);
  if (node->type == DIC_NODE_TYPE_NODE) {
    if (node->occurs > 1) {
      g_string_append_printf(inc,"\n");
      inc_print_occurs(inc,node->occurs);
    } else {
      g_string_append_printf(inc,".\n");
    }
    for(child=DNODE_CHILDREN(node);child!=NULL;child=DNODE_NEXT(child)) {
      print_inc(inc,l+1,child,prefix);
    }
  } else if(node->type == DIC_NODE_TYPE_TEXT){
    if (node->occurs > 1) {
      g_string_append_printf(inc,"\n");
      inc_print_pic_occurs(inc,node->length,node->occurs);
    } else {
      g_string_append_printf(inc,"\n");
      inc_print_pic(inc,node->length);
    }
  } else if(node->type == DIC_NODE_TYPE_IMAGE){
    if (node->occurs > 1) {
      g_string_append_printf(inc,"\n");
      inc_print_pic_occurs(inc,DNODE_IMAGE_PATH_SIZE,node->occurs);
    } else {
      g_string_append_printf(inc,"\n");
      inc_print_pic(inc,DNODE_IMAGE_PATH_SIZE);
    }
  }
  g_free(name);
}

GString*
red2inc(xmlDocPtr doc,gchar *prefix)
{
  xmlNodePtr dict;
  DicNode *dtree;
  DicNode *child;
  GString *inc;

  if (doc == NULL) {
    fprintf(stderr, "Error: xmlDocPtr doc is NULL\n");
    exit(1);
  }
  dict = FindNodeByTag(doc->xmlRootNode->xmlChildrenNode,
    BAD_CAST(MONPE_XML_DICTIONARY));
  if (dict == NULL) {
    fprintf(stderr,"no dictionary data\n");
    exit(1);
  }
  dtree = dtree_new();
  dtree_new_from_xml(&dtree,dict);

  inc = g_string_new("        01  ");
  g_string_append_printf(inc,"%s.\n",prefix);
  for (child = DNODE_CHILDREN(dtree);child!=NULL;child=DNODE_NEXT(child)) {
    print_inc(inc,0,child,prefix);
  }
  return inc;
}
