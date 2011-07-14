/* -*- Mode: C; c-basic-offset: 2 -*-
 * diaembed.c
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <libmondai.h>
#include <RecParser.h>

#include "red3lib.h"

static char *outfile = NULL;

static gchar*
foldText(ValueStruct *value,
  int text_size,
  int column_size)
{
  gchar *str;
  gchar *cutted;
  gchar *folded = NULL;
  gchar *p,*q,*r;
  int i,len,num;
  gsize size;

  if (!IS_VALUE_STRING(value)) {
    g_warning("WARNING invalid value type(%d)", ValueType(value));
    return NULL;
  }
  if (IS_VALUE_NIL(value)) {
    g_warning("WARNING value is nil");
    return NULL;
  }

  str = ValueToString(value,NULL);

  if (!g_utf8_validate(str,-1,NULL)) {
    g_warning("WARNING invalid utf8 string");
    return NULL;
  } 

  if (g_utf8_strlen(str,0) <= text_size) {
    cutted = str;
  } else {
    cutted = g_strdup(str);
    g_utf8_strncpy(cutted,str,text_size);
  }

  len = g_utf8_strlen(cutted,-1);

  if (column_size <= 0 || len <= column_size) {
    size = strlen(cutted) + 3;
    folded = g_malloc0(size);
    g_strlcat(folded,"#",size);
    g_strlcat(folded,cutted,size);
    g_strlcat(folded,"#",size);
  } else {
    num = len / column_size;
    size = strlen(cutted) + num + 3;
    folded = g_malloc0(size);
    g_strlcat(folded,"#",size);
    q = NULL;
    for(i=0;i<num;i++) {
      p = g_utf8_offset_to_pointer(cutted,i * column_size);
      q = g_utf8_offset_to_pointer(cutted,(i + 1) * column_size);
      r = g_strndup(p,(q-p));
      g_strlcat(folded,r,size);
      g_strlcat(folded,"\n",size);
      g_free(r);
    }
    if (q != NULL) {
      g_strlcat(folded,q,size);
    }
    g_strlcat(folded,"#",size);
  }
  return folded;
}

static void
embedText(EmbedInfo *info,
  ValueStruct *value)
{
  gchar *content;
  xmlNodePtr n1,n2,n3,n4;

  content = foldText(value,
    EmbedInfoAttr(info,Text,text_size),
    EmbedInfoAttr(info,Text,column_size));

  if (content == NULL) {
    return;
  }

  n1 = GetChildByTagAttr(info->node,
    BAD_CAST("attribute"),BAD_CAST("name"),BAD_CAST("text"));
  if (n1 == NULL) {
    return;
  }
  n2 = GetChildByTagAttr(n1,
    BAD_CAST("composite"),BAD_CAST("type"),BAD_CAST("text"));
  if (n2 == NULL) {
    return;
  }
  n3 = GetChildByTagAttr(n2,
    BAD_CAST("attribute"),BAD_CAST("name"),BAD_CAST("string"));
  if (n3 == NULL) {
    return;
  }
  n4 = GetChildByTag(n3, BAD_CAST("string"));
  if (n4 == NULL) {
    return ;
  }
  xmlNodeSetContent(n4,BAD_CAST(content));
}

static void
embedImage(EmbedInfo *info,
  ValueStruct *value)
{
  gchar *content;
  xmlNodePtr n1,n2;

  content = foldText(value,EmbedInfoAttr(info,Image,path_size), 0);

  if (content == NULL) {
    return;
  }

  n1 = GetChildByTagAttr(info->node,
    BAD_CAST("attribute"),BAD_CAST("name"),BAD_CAST("file"));
  if (n1 == NULL) {
    return;
  }
  n2 = GetChildByTag(n1, BAD_CAST("string"));
  if (n2 == NULL) {
    return ;
  }
  xmlNodeSetContent(n2,BAD_CAST(content));
}

static void
embed(GPtrArray *array,
  ValueStruct *data)
{
  EmbedInfo *info;
  ValueStruct *v;
  int i;

  for (i=0;i<array->len;i++) {
    info = (EmbedInfo*)g_ptr_array_index(array,i);
    v = GetItemLongName(data,info->id);
    if (v == NULL) {
      continue;
    } else {
      switch(info->type) {
      case EMBED_TYPE_TEXT:
        embedText(info,v);
        break;
      case EMBED_TYPE_IMAGE:
        embedImage(info,v);
        break;
      }
    }
  }
}

static void
red3embed(char *diafile,
  char *recfile,
  char *datafile)
{
  xmlDocPtr doc;
  GPtrArray *array;

  ValueStruct *value;
  char *vname;
  gchar *buf;
  gsize size;
  CONVOPT *conv;
  xmlChar *outmem;
  int outsize;

  xmlInitParser();
  LIBXML_TEST_VERSION

  doc = xmlParseFile(diafile);
  if (doc == NULL) {
    g_warning("Error: unable to parse file:%s", diafile);
    return;
  }
  array = GetEmbedInfoList(doc);

  RecParserInit();
  value = RecParseValue(recfile,&vname);
  if (value == NULL) {
    g_warning("Error: unable to read rec file:%s\n",recfile);
    return;
  }
  if (!IS_VALUE_RECORD(value)) {
    g_warning("Error: invalid value type:%d rec file:%s\n",
      ValueType(value),recfile);
    return;
  }

  if (!g_file_get_contents(datafile,&buf,&size,NULL)) {
    g_warning("Error: unable to read data file:%s\n",datafile);
    return;
  }
  conv = NewConvOpt();
  ConvSetSize(conv,500,100);
  ConvSetCodeset(conv,"euc-jisx0213");
  OpenCOBOL_UnPackValue(conv,(unsigned char*)buf,value);

  embed(array,value);

  xmlKeepBlanksDefault(0);
  if (outfile != NULL) {
    xmlSaveFormatFile(outfile,doc,1);
  } else {
    xmlDocDumpFormatMemory(doc,&outmem,&outsize,1);
    printf("%s",outmem);
  }
  xmlFreeDoc(doc);
}

static GOptionEntry entries[] =
{
  { "out",'o',0,G_OPTION_ARG_STRING,&outfile,"output filename",NULL},
  { NULL}
};

int main(int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *ctx;

  ctx = g_option_context_new("<.red3> <.rec> <.dat>");
  g_option_context_add_main_entries(ctx, entries, NULL);
  if (!g_option_context_parse(ctx,&argc,&argv,&error)) {
    g_print("option parsing failed:%s\n",error->message);
    exit(1);
  }
  if (argc < 4) {
    g_print("%s",g_option_context_get_help(ctx,TRUE,NULL));
    exit(1);
  }
  red3embed(argv[1],argv[2],argv[3]);
  return 0;
}
