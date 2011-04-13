/* -*- Mode: C; c-basic-offset: 2 -*-
 *
 *
 */

#ifndef	_INC_RED3PARSER_H
#define	_INC_RED3PARSER_H

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

#define CAST_BAD (gchar*)

#define MAX_ROWS_COLS 100

enum EmbedType
{
  EMBED_TYPE_TEXT,
  EMBED_TYPE_ARRAY,
  EMBED_TYPE_TABLE,
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
    } Text;
    struct {
      int text_size;
      int column_size;
      int array_size;
    } Array;
    struct {
      int rows;
      int cols;
      int text_sizes[MAX_ROWS_COLS];
    } Table;
    struct {
      int path_size;
    } Image;
  } Data;  
} EmbedInfo;

#define EmbedInfoAttr(p,t,a) ((p)->Data.t.a)

GPtrArray* GetEmbedInfoList(xmlDocPtr doc);
xmlNodePtr GetChildByTag(xmlNodePtr node, xmlChar *tag);
xmlNodePtr GetChildByTagAttr(xmlNodePtr node, xmlChar *tag, 
  xmlChar *attr, xmlChar *val);
xmlChar* GetAttributeString(xmlNodePtr node, xmlChar *propname);
int GetAttributeInt(xmlNodePtr node, xmlChar *propname);

#endif
