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

#include "red3parser.h"

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
    fprintf(stderr,"WARNING %s:%d invalid value type(%d)\n",
      __FILE__,__LINE__,ValueType(value));
    return NULL;
  }
  if (IS_VALUE_NIL(value)) {
    fprintf(stderr,"WARNING %s:%d value is nil\n",
      __FILE__,__LINE__);
    return NULL;
  }

  str = ValueToString(value,NULL);

  if (!g_utf8_validate(str,-1,NULL)) {
    fprintf(stderr,"WARNING %s:%d invalid utf8 string\n",
      __FILE__,__LINE__);
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
setArrayContent(xmlNodePtr node,
  int idx,
  xmlChar *content)
{
  xmlNodePtr n1,n2,n3,n4;
  xmlChar *name;
  int i;

  n1 = GetChildByTagAttr(node,
    BAD_CAST("attribute"),BAD_CAST("name"),BAD_CAST("earray_texts"));
  if (n1 == NULL) {
    return;
  }

  i = 0;
  n2 = n1->children;
  while(n2 != NULL) {
    if (n2->type == XML_ELEMENT_NODE && 
      !xmlStrcmp(n2->name,BAD_CAST("composite"))) {
      name = xmlGetProp(n2,BAD_CAST("type"));
      if (name != NULL && !xmlStrcmp(name,BAD_CAST("text"))) {
        if (i == idx) {
          n3 = GetChildByTagAttr(n2,
            BAD_CAST("attribute"),BAD_CAST("name"),BAD_CAST("string"));
          if (n3 == NULL) {
            return;
          }
          n4 = GetChildByTag(n3, BAD_CAST("string"));
          if (n4 == NULL) {
            return ;
          }
          xmlNodeSetContent(n4,content);
          return;
        }
        i++;
      }
    }
    n2 = n2->next;
  }
}

static void
embedArray(EmbedInfo *info,
  ValueStruct *array)
{
  gchar *content;
  ValueStruct *value;
  xmlNodePtr n1,n2;
  int i,array_size;

  if (array == NULL || ValueType(array) != GL_TYPE_ARRAY) {
    fprintf(stderr,"WARNING %s:%d invalid value type(%d)\n",
      __FILE__,__LINE__,ValueType(array));
    return;
  }
  if (EmbedInfoAttr(info,Array,array_size) > ValueArraySize(array)) {
    array_size = ValueArraySize(array);
  } else {
    array_size = EmbedInfoAttr(info,Array,array_size);
  }
  for(i=0;i<array_size;i++) {
    value = ValueArrayItem(array,i);
    content = foldText(value,
      EmbedInfoAttr(info,Array,text_size),
      EmbedInfoAttr(info,Array,column_size));
    if (content != NULL) {
      setArrayContent(info->node,i,BAD_CAST(content));
    }
  }

  n1 = GetChildByTagAttr(info->node,
    BAD_CAST("attribute"),BAD_CAST("name"),BAD_CAST("sample"));
  if (n1 != NULL) {
    n2 = GetChildByTag(n1, BAD_CAST("string"));
    if (n2 != NULL) {
      xmlNodeSetContent(n2,BAD_CAST(g_strdup("##")));
    }
  }
}

static void
setTableContent(xmlNodePtr node,
  int idx,
  xmlChar *content)
{
  xmlNodePtr n1,n2;
  int i;

  n1 = GetChildByTagAttr(node,
    BAD_CAST("attribute"),BAD_CAST("name"),BAD_CAST("etable_texts"));
  if (n1 == NULL) {
    return;
  }

  i = 0;
  n2 = n1->children;
  while(n2 != NULL) {
    if (n2->type == XML_ELEMENT_NODE && 
      !xmlStrcmp(n2->name,BAD_CAST("string"))) {
      if (i == idx) {
        xmlNodeSetContent(n2,content);
        return;
      }
      i++;
    }
    n2 = n2->next;
  }
}

static void
embedTable(EmbedInfo *info,
  ValueStruct *value)
{
  gchar *content;
  xmlNodePtr n1,n2;
  int i,j,k;
  ValueStruct *record,*str;

  if (value == NULL || ValueType(value) != GL_TYPE_ARRAY) {
    fprintf(stderr,"WARNING %s:%d invalid value type(%d)\n",
      __FILE__,__LINE__,ValueType(value));
    return;
  }
  if (EmbedInfoAttr(info,Table,rows) != ValueArraySize(value)) {
    fprintf(stderr,"WARNING %s:%d rows are not corresponding (%d,%d)\n",
      __FILE__,__LINE__,
      EmbedInfoAttr(info,Table,rows),
      ValueArraySize(value));
    return;
  }
  k = 0;
  for(i=0;i<ValueArraySize(value);i++) {
    record = ValueArrayItem(value,i);
    if (record == NULL || !IS_VALUE_RECORD(record)) {
      fprintf(stderr,"WARNING %s:%d invalid value type(%d)\n",
        __FILE__,__LINE__,ValueType(record));
      return;
    }
    if (EmbedInfoAttr(info,Table,cols) != ValueRecordSize(record)) {
      fprintf(stderr,"WARNING %s:%d columns are not corresponding (%d,%d)\n",
        __FILE__,__LINE__,
        EmbedInfoAttr(info,Table,rows),
        ValueArraySize(value));
      return;
    }
    for (j = 0; j < ValueRecordSize(record); j++) {
      str = GetRecordItem(record,ValueRecordName(record,j));
      if (str == NULL || !IS_VALUE_STRING(str)) {
        fprintf(stderr,"WARNING %s:%d invalid value type(%d)\n",
          __FILE__,__LINE__,ValueType(str));
        return;
      }
      content = foldText(str,
        EmbedInfoAttr(info,Table,text_sizes[j]),
        0);
      if (content != NULL) {
        setTableContent(info->node,k,BAD_CAST(content));
      }
      k++;
    }
  }

  n1 = GetChildByTagAttr(info->node,
    BAD_CAST("attribute"),BAD_CAST("name"),BAD_CAST("sample"));
  if (n1 != NULL) {
    n2 = GetChildByTag(n1, BAD_CAST("string"));
    if (n2 != NULL) {
      xmlNodeSetContent(n2,BAD_CAST(g_strdup("##")));
    }
  }

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
    v = GetRecordItem(data,info->id);
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
      case EMBED_TYPE_ARRAY:
        embedArray(info,v);
        break;
      case EMBED_TYPE_TABLE:
        embedTable(info,v);
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
    fprintf(stderr, "Error: unable to parse file:%s\n", diafile);
    return;
  }
  array = GetEmbedInfoList(doc);

  RecParserInit();
  value = RecParseValue(recfile,&vname);
  if (value == NULL) {
    fprintf(stderr, "Error: unable to read rec file:%s\n",recfile);
    return;
  }
  if (!IS_VALUE_RECORD(value)) {
    fprintf(stderr, "Error: invalid value type:%d rec file:%s\n",
      ValueType(value),recfile);
    return;
  }

  if (!g_file_get_contents(datafile,&buf,&size,NULL)) {
    fprintf(stderr, "Error: unable to read data file:%s\n",datafile);
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
