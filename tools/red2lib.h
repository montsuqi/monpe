/* -*- Mode: C; c-basic-offset: 2 -*-
 *
 *
 */

#ifndef	_INC_RED2LIB_H
#define	_INC_RED2LIB_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "dtree.h"

#define CAST_BAD (gchar*)

#define MAX_ROWS_COLS 100

enum EmbedType
{
  EMBED_TYPE_TEXT,
  EMBED_TYPE_IMAGE
};

typedef struct {
  gchar *id;
  xmlNodePtr node;
  enum EmbedType type;
  union {
    struct {
      int text_size;
      int column_size;
      int char_type;
    } Text;
    struct {
      int path_size;
    } Image;
  } Data;  
} EmbedInfo;

#define EmbedInfoAttr(p,t,a) ((p)->Data.t.a)

gchar* EscapeNodeName(gchar *_name);
GPtrArray* GetEmbedInfoList(xmlDocPtr doc);
xmlNodePtr FindNodeByTag(xmlNodePtr p,xmlChar *tag);
xmlNodePtr GetChildByTag(xmlNodePtr node, xmlChar *tag);
xmlNodePtr GetChildByTagAttr(xmlNodePtr node, xmlChar *tag, 
  xmlChar *attr, xmlChar *val);
xmlChar* GetAttributeString(xmlNodePtr node, xmlChar *propname);
int GetAttributeInt(xmlNodePtr node, xmlChar *propname);
int GetAttributeEnum(xmlNodePtr node, xmlChar *propname);
GString *red2rec(xmlDocPtr);
GString *red2inc(xmlDocPtr,gchar *prefix);
GString *red2fill(xmlDocPtr doc,gchar *imagepath);
GString *red2embed(xmlDocPtr doc,gchar *data);

#endif
