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
#include <libmondai.h>
#include <RecParser.h>

#include "red2lib.h"

/********************************************************
 * misc function
 ********************************************************/

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

int
GetAttributeEnum(xmlNodePtr node,
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
          !xmlStrcmp(child2->name,BAD_CAST("enum"))) {
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
  int text_size, column_size, char_type;

  if (node == NULL) {
    return info;
  }
  id = GetAttributeString(node,BAD_CAST("embed_id"));
  text_size = GetAttributeInt(node,BAD_CAST("embed_text_size"));
  column_size = GetAttributeInt(node,BAD_CAST("embed_column_size"));
  char_type = GetAttributeEnum(node,BAD_CAST("embed_char_type"));

  if (id != NULL && text_size != -1 && column_size != -1) {
    info = g_new0(EmbedInfo,1);
    info->id = CAST_BAD(id);
    info->type = EMBED_TYPE_TEXT;
    info->node = node;
    EmbedInfoAttr(info,Text,text_size) = text_size;
    EmbedInfoAttr(info,Text,column_size) = column_size;
    EmbedInfoAttr(info,Text,char_type) = char_type;
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

/********************************************************
 * red2rec
 ********************************************************/

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

/********************************************************
 * red2inc
 ********************************************************/

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
}

/********************************************************
 * red2inc
 ********************************************************/

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

/********************************************************
 * red2fill
 ********************************************************/
#define ZENKAKU_TEXT "いろはにほへとちりぬるをわかよたれそつねならむうゐのおやまけふこえてあさきゆめみしゑひもせすん"
#define HANKAKU_TEXT "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

static void
_red2fill(GString *fill,GHashTable *usage,DicNode *node,GPtrArray *array,gchar *imagepath)
{
  EmbedInfo *info;
  DicNode *child;
  int i,j,l,k,n;
  gchar *p;
  gboolean destroy_usage = FALSE;

  if (usage == NULL) {
    usage = g_hash_table_new(g_direct_hash,g_direct_equal);
    destroy_usage = TRUE;
  }

  if (node->type == DIC_NODE_TYPE_NODE) {
    for (i=0;i<node->occurs;i++) {
      for(child=DNODE_CHILDREN(node);child!=NULL;child=DNODE_NEXT(child)) {
        _red2fill(fill,usage,child,array,imagepath);
      }
    }
  } else if(node->type == DIC_NODE_TYPE_TEXT){
    for (i=0;i<node->occurs;i++) {
      gchar *name,*xname;
      int type;

      n = GPOINTER_TO_INT(g_hash_table_lookup(usage,node));
      g_hash_table_insert(usage,node,GINT_TO_POINTER(n+1));

      type = 0;
      name = dnode_data_get_longname(node,n);
      xname = dtree_conv_longname_to_xml(name);

      for(j=0;j<array->len;j++) {
        info = (EmbedInfo*)g_ptr_array_index(array,j);
        if (!g_strcmp0(info->id,xname)) {
          type = EmbedInfoAttr(info,Text,char_type);
        }
      }
      if (type == 0) {
        p = (gchar*)ZENKAKU_TEXT;
        l = 0;
        for(k=0;k<node->length;k++) {
          if (l == strlen(ZENKAKU_TEXT)) {
            l = 0;
          }
          g_string_append_printf(fill,"%c",*(p+l));
          l++;
        }
      } else {
        p = (gchar*)HANKAKU_TEXT;
        l = 0;
        for(k=0;k<node->length;k++) {
          if (l == strlen(HANKAKU_TEXT)) {
            l = 0;
          }
          g_string_append_printf(fill,"%c",*(p+l));
          l++;
        }
      }
      g_free(name);
      g_free(xname);
    }
  } else if(node->type == DIC_NODE_TYPE_IMAGE){
    if (imagepath != NULL) {
      g_string_append_printf(fill,"%s",imagepath);
      for(i=strlen(imagepath);i<DNODE_IMAGE_PATH_SIZE;i++) {
        g_string_append_printf(fill," ");
      }
    } else {
      for(i=0;i<DNODE_IMAGE_PATH_SIZE;i++) {
        g_string_append_printf(fill," ");
      }
    }
  }

  if (destroy_usage) {
    g_hash_table_destroy(usage);
  }
}

GString*
red2fill(xmlDocPtr doc,gchar *imagepath)
{
  xmlNodePtr dict;
  GPtrArray *array;
  GString *fill;
  DicNode *dtree;
  DicNode *child;

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

  array = GetEmbedInfoList(doc);
  dtree = dtree_new();
  dtree_new_from_xml(&dtree,dict);
  fill = g_string_new("");
  for (child = DNODE_CHILDREN(dtree);child!=NULL;child=DNODE_NEXT(child)) {
    _red2fill(fill,NULL,child,array,imagepath);
  }
  return fill;
}

/********************************************************
 * red2embed
 ********************************************************/

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
_red2embed(GPtrArray *array,
  ValueStruct *data)
{
  EmbedInfo *info;
  ValueStruct *v;
  int i;
  gchar *id;

  for (i=0;i<array->len;i++) {
    info = (EmbedInfo*)g_ptr_array_index(array,i);
    id = EscapeNodeName(info->id);
    v = GetItemLongName(data,id);
    g_free(id);
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

GString *
red2embed(xmlDocPtr doc,gchar *data)
{
  GString *embed;
  GPtrArray *array;

  ValueStruct *value;
  char *vname;
  CONVOPT *conv;
  xmlChar *outmem;
  int outsize;
  GString *rec;

  embed = g_string_new("");
  array = GetEmbedInfoList(doc);
  rec = red2rec(doc);

  RecParserInit();
  value = RecParseValueMem(rec->str,&vname);
  if (value == NULL) {
    g_error("Error: unable to read rec\n");
  }
  if (!IS_VALUE_RECORD(value)) {
    g_error("Error: invalid value type:%d\n",
      ValueType(value));
  }

  conv = NewConvOpt();
  ConvSetSize(conv,500,100);
  ConvSetCodeset(conv,"euc-jisx0213");
  OpenCOBOL_UnPackValue(conv,(unsigned char*)data,value);

  _red2embed(array,value);

  xmlKeepBlanksDefault(0);
  xmlDocDumpFormatMemory(doc,&outmem,&outsize,1);
  g_string_append_printf(embed,"%s",CAST_BAD(outmem));

  xmlFree(outmem);

  return embed;
}
