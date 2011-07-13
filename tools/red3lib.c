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

#include "red3lib.h"

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
    info->id = CAST_BAD(id);
    info->type = EMBED_TYPE_TEXT;
    info->node = node;
    EmbedInfoAttr(info,Text,text_size) = text_size;
    EmbedInfoAttr(info,Text,column_size) = column_size;
  }
  return info;
}

static EmbedInfo*
GetEmbedInfoArray(xmlNodePtr node)
{
  EmbedInfo *info = NULL;
  xmlChar *id;
  int text_size,column_size, array_size;

  if (node == NULL) {
    return info;
  }

  id = GetAttributeString(node,BAD_CAST("embed_id"));
  text_size = GetAttributeInt(node,BAD_CAST("embed_text_size"));
  column_size = GetAttributeInt(node,BAD_CAST("embed_column_size"));
  array_size = GetAttributeInt(node,BAD_CAST("embed_array_size"));

  if (id != NULL && text_size != -1 && column_size != -1 && array_size != -1) {
    info = g_new0(EmbedInfo,1);
    info->id = CAST_BAD(id);
    info->type = EMBED_TYPE_ARRAY;
    info->node = node;
    EmbedInfoAttr(info,Array,text_size) = text_size;
    EmbedInfoAttr(info,Array,column_size) = column_size;
    EmbedInfoAttr(info,Array,array_size) = array_size;
  }

  return info;
}

static EmbedInfo*
GetEmbedInfoTable(xmlNodePtr node)
{
  EmbedInfo *info = NULL;
  int i;
  gchar **p;
  xmlChar *id, *str;
  int rows, cols;

  if (node == NULL) {
    return info;
  }

  id = GetAttributeString(node,BAD_CAST("embed_id"));
  str = GetAttributeString(node,BAD_CAST("embed_text_sizes"));
  rows = GetAttributeInt(node,BAD_CAST("grid_rows"));
  cols = GetAttributeInt(node,BAD_CAST("grid_cols"));

  if (id != NULL && str != NULL && rows != -1 && cols != -1) {
    info = g_new0(EmbedInfo,1);
    info->id = CAST_BAD(id);
    info->type = EMBED_TYPE_TABLE;
    info->node = node;
    EmbedInfoAttr(info,Table,rows) = rows;
    EmbedInfoAttr(info,Table,cols) = cols;
    for(i=0;i<MAX_ROWS_COLS;i++) {
      EmbedInfoAttr(info,Table,text_sizes[i]) = 256;
    }

    p = g_strsplit(CAST_BAD(str),",",MAX_ROWS_COLS);
    i = 0;
    while (*(p+i) != NULL) {
      if (*(p+i) != NULL) {
        EmbedInfoAttr(info,Table,text_sizes[i]) = atoi(*(p+i));
      }
      i++;
    }
    g_strfreev(p);
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
    info->id = CAST_BAD(id);
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
  } else if (!xmlStrcmp(type,BAD_CAST("Embed - Array"))) {
    info = GetEmbedInfoArray(node);
  } else if (!xmlStrcmp(type,BAD_CAST("Embed - Table"))) {
    info = GetEmbedInfoTable(node);
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
