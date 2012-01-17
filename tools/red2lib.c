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
  int column_size, char_type;

  if (node == NULL) {
    return info;
  }
  id = GetAttributeString(node,BAD_CAST("embed_id"));
  column_size = GetAttributeInt(node,BAD_CAST("embed_column_size"));
  char_type = GetAttributeEnum(node,BAD_CAST("embed_char_type"));

  if (id != NULL && column_size != -1) {
    info = g_new0(EmbedInfo,1);
    info->id = CAST_BAD(id);
    info->type = EMBED_TYPE_TEXT;
    info->node = node;
    info->column_size = column_size;
    info->char_type = char_type;
  }
  return info;
}

static EmbedInfo*
GetEmbedInfoImage(xmlNodePtr node)
{
  EmbedInfo *info = NULL;
  xmlChar *id;

  if (node == NULL) {
    return info;
  }

  id = GetAttributeString(node,BAD_CAST("embed_id"));

  if (id != NULL) {
    info = g_new0(EmbedInfo,1);
    info->id = CAST_BAD(id);
    info->type = EMBED_TYPE_IMAGE;
    info->node = node;
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
  return array;
}

static DicNode*
GetDTree(xmlDocPtr doc)
{
  DicNode *dtree = NULL;
  xmlNodePtr dict;

  if (doc == NULL) {
    g_error("Error: xmlDocPtr doc is NULL\n");
  }
  dict = FindNodeByTag(doc->xmlRootNode->xmlChildrenNode,
    BAD_CAST(MONPE_XML_DICTIONARY));
  if (dict == NULL) {
    g_error("no dictionary data\n");
  }
  dtree = dtree_new();
  dtree_new_from_xml(&dtree,dict);

  if (dtree == NULL) {
    g_error("Error: can't make dtree\n");
  }
  return dtree;
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
  DicNode *dtree;
  DicNode *child;
  GString *rec;

  if (doc == NULL) {
    g_error("Error: xmlDocPtr doc is NULL\n");
  }
  dtree = GetDTree(doc);

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

GString*
red2inc(xmlDocPtr doc,gchar *prefix)
{
  DicNode *child;
  GString *inc;
  DicNode *dtree;

  if (doc == NULL) {
    g_error("Error: xmlDocPtr doc is NULL\n");
  }
  dtree = GetDTree(doc);

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
_red2fill(GString *fill,
  GHashTable *usage,
  DicNode *node,
  GPtrArray *array,
  gchar *imagepath)
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
          type = info->char_type;
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
  GPtrArray *array;
  GString *fill;
  DicNode *dtree;
  DicNode *child;

  if (doc == NULL) {
    g_error("Error: xmlDocPtr doc is NULL\n");
  }

  array = GetEmbedInfoList(doc);
  dtree = GetDTree(doc);
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
  ValueStruct *value,
  int length)
{
  gchar *content;
  xmlNodePtr n1,n2,n3,n4;

  content = foldText(value, length, info->column_size);

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

  content = foldText(value,DNODE_IMAGE_PATH_SIZE, 0);

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
  ValueStruct *data,
  DicNode *dtree)
{
  EmbedInfo *info;
  ValueStruct *v;
  int i,index;
  gchar *vid,*nid;
  DicNode *node;

  for (i=0;i<array->len;i++) {
    info = (EmbedInfo*)g_ptr_array_index(array,i);
    vid = EscapeNodeName(info->id);
    v = GetItemLongName(data,vid);
    g_free(vid);
    if (v == NULL) {
      continue;
    }
    nid = dtree_conv_longname_from_xml(info->id);
    node = dtree_get_node_by_longname(dtree,&index,nid);
    g_free(nid);
    if (node == NULL) {
      continue;
    }
    switch(info->type) {
    case EMBED_TYPE_TEXT:
      embedText(info,v,node->length);
      break;
    case EMBED_TYPE_IMAGE:
      embedImage(info,v);
      break;
    }
  }
}

/*************************/

static const struct _paper_metrics {
  gchar *name;
  gdouble pswidth, psheight;
} paper_metrics[] = {
  { "#A3#", 29.7, 42.0},
  { "#A4#", 21.0, 29.7},
  { "#A5#", 14.85, 21.0},
  { "#B4#", 25.7528, 36.4772},
  { "#B5#", 17.6389, 25.0472},
  { "#B5-Japan#", 18.2386, 25.7528},
  { "#Letter#", 21.59, 27.94},
  { "#Legal#", 21.59, 35.56},
  { "#Ledger#", 27.9, 43.2},
  { "#Half-Letter#", 21.59, 14.0 },
  { "#Executive#", 18.45, 26.74 },
  { "#Tabloid#", 28.01, 43.2858 },
  { "#Monarch#", 9.8778, 19.12 },
  { "#SuperB#", 29.74, 43.2858 },
  { "#Envelope-Commercial#", 10.5128, 24.2 },
  { "#Envelope-Monarch#", 9.8778, 19.12 },
  { "#Envelope-DL#", 11.0, 22.0 },
  { "#Envelope-C5#", 16.2278, 22.9306 },
  { "#EuroPostcard#", 10.5128, 14.8167 },
  { "#A0#", 84.1, 118.9 },
  { "#A1#", 59.4, 84.1 },
  { "#A2#", 42.0, 59.4 },
  { "#Custom#", 10.0, 10.0},
  { NULL, 0.0, 0.0 }
};

static double
get_real_value(xmlNodePtr node)
{
  xmlChar *val;
  double ret;

  if (node == NULL) {
    return 0.0;
  }

  val = xmlGetProp(node,BAD_CAST("val"));
  if (val == NULL) {
    return 0.0;
  }
  ret = atof(CAST_BAD(val));
  xmlFree(val);
  return 0.0;
}

static double
getPageSkip(xmlDocPtr doc)
{
  xmlXPathContextPtr xpathCtx; 
  xmlXPathObjectPtr xpathObj; 
  xmlChar *paper;
  xmlChar *exIsPortrait = 
    BAD_CAST(
      "//dia:composite[@type='paper']/"
      "dia:attribute[@name='is_portrait']/"
      "dia:boolean[@val='true']"); 
  xmlChar *exPaper = 
    BAD_CAST(
      "//dia:composite[@type='paper']/"
      "dia:attribute[@name='name']/"
      "dia:string"); 
  xmlChar *exTmargin = 
    BAD_CAST(
      "//dia:composite[@type='paper']/"
      "dia:attribute[@name='tmargin']/"
      "dia:real"); 
  xmlChar *exBmargin = 
    BAD_CAST(
      "//dia:composite[@type='paper']/"
      "dia:attribute[@name='bmargin']/"
      "dia:real"); 
  xmlChar *exCustomWidth = 
    BAD_CAST(
      "//dia:composite[@type='paper']/"
      "dia:attribute[@name='custom_width']/"
      "dia:real"); 
  xmlChar *exCustomHeight = 
    BAD_CAST(
      "//dia:composite[@type='paper']/"
      "dia:attribute[@name='custom_height']/"
      "dia:real"); 

  int i,size;
  double tmargin,bmargin,custom_width,custom_height,skip = 0.0;
  gboolean is_portrait;

  xpathCtx = xmlXPathNewContext(doc);
  xmlXPathRegisterNs(xpathCtx,
    BAD_CAST("dia"),
    BAD_CAST("http://www.lysator.liu.se/~alla/dia/"));

  /*custom width*/
  custom_width = 10.0;
  xpathObj = xmlXPathEvalExpression(exCustomWidth, xpathCtx);
  if(xpathObj != NULL && xpathObj->nodesetval != NULL) {
    custom_width = get_real_value(xpathObj->nodesetval->nodeTab[0]);
  }
  xmlXPathFreeObject(xpathObj);

  /*custom height*/
  custom_height = 10.0;
  xpathObj = xmlXPathEvalExpression(exCustomHeight, xpathCtx);
  if(xpathObj != NULL && xpathObj->nodesetval != NULL) {
    custom_height = get_real_value(xpathObj->nodesetval->nodeTab[0]);
  } 
  xmlXPathFreeObject(xpathObj);

  /*is portrait*/
  is_portrait = TRUE;
  xpathObj = xmlXPathEvalExpression(exIsPortrait, xpathCtx);
  if(xpathObj != NULL) {
    size = (xpathObj->nodesetval) ? xpathObj->nodesetval->nodeNr : 0;
    if (size == 0) {
      is_portrait = FALSE;
    }
  }
  xmlXPathFreeObject(xpathObj);

  /*paper size*/
  paper = NULL;
  xpathObj = xmlXPathEvalExpression(exPaper, xpathCtx);
  if(xpathObj != NULL && xpathObj->nodesetval != NULL) {
    paper = xmlXPathCastNodeSetToString(xpathObj->nodesetval);
  } else {
    g_critical("cannot found paper node");
  }
  if (paper == NULL) {
    g_critical("cannot get paper size");
  }
  for(i=0;paper_metrics[i].name!=NULL;i++) {
    if (!g_ascii_strcasecmp(CAST_BAD(paper),paper_metrics[i].name)) {
      if (is_portrait) {
        skip = paper_metrics[i].psheight;
      } else {
        skip = paper_metrics[i].pswidth;
      }
    }
    if (!g_ascii_strcasecmp(CAST_BAD(paper),"#custom#")) {
      if (is_portrait) {
        skip = custom_height;
      } else {
        skip = custom_width;
      }
    }
  }
  if (skip == 0.0) {
    g_critical("cannot found paper");
  }
  xmlXPathFreeObject(xpathObj);

  /*tmargin*/
  tmargin = 0.0;
  xpathObj = xmlXPathEvalExpression(exTmargin, xpathCtx);
  if(xpathObj != NULL && xpathObj->nodesetval != NULL) {
    tmargin = get_real_value(xpathObj->nodesetval->nodeTab[0]);
  } else {
    g_critical("cannot get tmargin node");
  }
  skip -= tmargin;
  xmlXPathFreeObject(xpathObj);

  /*bmargin*/
  bmargin = 0.0;
  xpathObj = xmlXPathEvalExpression(exBmargin, xpathCtx);
  if(xpathObj != NULL && xpathObj->nodesetval != NULL) {
    bmargin = get_real_value(xpathObj->nodesetval->nodeTab[0]);
  } else {
    g_critical("cannot get bmargin node");
  }
  skip -= bmargin;
  xmlXPathFreeObject(xpathObj);

  xmlXPathFreeContext(xpathCtx); 
  return skip;
}

static void
delete_layers(xmlNodePtr node)
{
  xmlNodePtr child;
  xmlNodePtr next;

  if (node == NULL) {
    return ;
  }
  if (!xmlStrcmp(node->name,BAD_CAST("layer"))) {
    xmlUnlinkNode(node);
    xmlFreeNode(node);
  }

  for(child = node->children; child != NULL; child = next)
  {
    next = child->next;
    delete_layers(child);
  }
}

static void
skip_obj_bb(xmlNodePtr node,double skip)
{
  double x1,y1,x2,y2;
  xmlChar *val;
  gchar **strs,*new;
  GRegex *regex;

  val = xmlGetProp(node,BAD_CAST("val"));
  if (val == NULL) {
   g_critical("invalid obj_pos");
  }
  regex = g_regex_new("[,;]",0,0,NULL);
  strs = g_regex_split(regex,CAST_BAD(val),0);
  if (strs[0] != NULL && strs[1] != NULL &&
      strs[2] != NULL && strs[3] != NULL) {
    x1 = atof(strs[0]);
    y1 = atof(strs[1]) + skip;
    x2 = atof(strs[2]);
    y2 = atof(strs[3]) + skip;
    new = g_strdup_printf("%lf,%lf;%lf,%lf",x1,y1,x2,y2);
    xmlSetProp(node,BAD_CAST("val"),BAD_CAST(new));
    g_free(new);
  }
  g_strfreev(strs);
  g_regex_unref(regex);
}

static void
skip_obj_pos(xmlNodePtr node,double skip)
{
  double x,y;
  xmlChar *val;
  gchar **strs,*new;
  GRegex *regex;

  val = xmlGetProp(node,BAD_CAST("val"));
  if (val == NULL) {
   g_critical("invalid obj_pos");
  }
  regex = g_regex_new(",",0,0,NULL);
  strs = g_regex_split(regex,CAST_BAD(val),0);
  if (strs[0] != NULL && strs[1] != NULL) {
    x = atof(strs[0]);
    y = atof(strs[1]) + skip;
    new = g_strdup_printf("%lf,%lf",x,y);
    xmlSetProp(node,BAD_CAST("val"),BAD_CAST(new));
    g_free(new);
  }
  g_strfreev(strs);
  g_regex_unref(regex);
}

static void
page_skip(xmlDocPtr doc,double skip)
{
  int i,size;
  xmlXPathContextPtr xpathCtx; 
  xmlXPathObjectPtr xpathObj; 
  xmlNodeSetPtr nodes;
  xmlChar *exPos = 
    BAD_CAST(
      "//dia:attribute[@name='obj_pos']/"
      "dia:point");
  xmlChar *exBB = 
    BAD_CAST(
      "//dia:attribute[@name='obj_bb']/"
      "dia:rectangle");
  xmlChar *exElemCorner = 
    BAD_CAST(
      "//dia:attribute[@name='elem_corner']/"
      "dia:point");
  xmlChar *exConnEnd = 
    BAD_CAST(
      "//dia:attribute[@name='conn_endpoints']/"
      "dia:point");

  xpathCtx = xmlXPathNewContext(doc);
  xmlXPathRegisterNs(xpathCtx,
    BAD_CAST("dia"),
    BAD_CAST("http://www.lysator.liu.se/~alla/dia/"));

  /*obj_pos*/
  xpathObj = xmlXPathEvalExpression(exPos, xpathCtx);
  if(xpathObj != NULL) {
    nodes = xpathObj->nodesetval;
    size = (nodes) ? nodes->nodeNr : 0;
    
    for(i = 0; i < size; i++) {
      if (nodes->nodeTab[i] == NULL) {
        continue;
      }
      skip_obj_pos(nodes->nodeTab[i],skip);
    }
  }
  xmlXPathFreeObject(xpathObj);

  /*obj_bb*/
  xpathObj = xmlXPathEvalExpression(exBB, xpathCtx);
  if(xpathObj != NULL) {
    nodes = xpathObj->nodesetval;
    size = (nodes) ? nodes->nodeNr : 0;
    
    for(i = 0; i < size; i++) {
      if (nodes->nodeTab[i] == NULL) {
        continue;
      }
      skip_obj_bb(nodes->nodeTab[i],skip);
    }
  }
  xmlXPathFreeObject(xpathObj);

  /*elem_corner*/
  xpathObj = xmlXPathEvalExpression(exElemCorner, xpathCtx);
  if(xpathObj != NULL) {
    nodes = xpathObj->nodesetval;
    size = (nodes) ? nodes->nodeNr : 0;
    
    for(i = 0; i < size; i++) {
      if (nodes->nodeTab[i] == NULL) {
        continue;
      }
      skip_obj_pos(nodes->nodeTab[i],skip);
    }
  }
  xmlXPathFreeObject(xpathObj);

  /*conn end*/
  xpathObj = xmlXPathEvalExpression(exConnEnd, xpathCtx);
  if(xpathObj != NULL) {
    nodes = xpathObj->nodesetval;
    size = (nodes) ? nodes->nodeNr : 0;
    
    for(i = 0; i < size; i++) {
      if (nodes->nodeTab[i] == NULL) {
        continue;
      }
      skip_obj_pos(nodes->nodeTab[i],skip);
    }
  }
  xmlXPathFreeObject(xpathObj);

  xmlXPathFreeContext(xpathCtx); 
}

static void
add_layers(xmlDocPtr dst,xmlDocPtr src)
{
  int i,size;
  xmlXPathContextPtr xpathCtx; 
  xmlXPathObjectPtr xpathObj; 
  xmlNodeSetPtr nodes;
  xmlNodePtr node;
  xmlChar *exLayer = 
    BAD_CAST( "//dia:layer");

  xpathCtx = xmlXPathNewContext(src);
  xmlXPathRegisterNs(xpathCtx,
    BAD_CAST("dia"),
    BAD_CAST("http://www.lysator.liu.se/~alla/dia/"));

  /*obj_bb*/
  xpathObj = xmlXPathEvalExpression(exLayer, xpathCtx);
  if(xpathObj != NULL) {
    nodes = xpathObj->nodesetval;
    size = (nodes) ? nodes->nodeNr : 0;
    
    for(i = 0; i < size; i++) {
      if (nodes->nodeTab[i] == NULL) {
        continue;
      }
      node = xmlCopyNode(nodes->nodeTab[i],1);
      xmlAddChild(xmlDocGetRootElement(dst),node);
    }
  }
  xmlXPathFreeObject(xpathObj);

  xmlXPathFreeContext(xpathCtx); 
}

gchar *
red2embed(int argc,char *argv[])
{
  xmlDocPtr doc,out;
  gchar *buf;
  gsize size;
  int i;
  double skip;
  xmlChar *outmem;
  int outsize;

  GPtrArray *array;
  ValueStruct *value;
  char *vname;
  CONVOPT *conv;
  GString *rec;
  DicNode *dtree;

  xmlInitParser();
  LIBXML_TEST_VERSION

  conv = NewConvOpt();
  ConvSetSize(conv,500,100);
  ConvSetCodeset(conv,"euc-jisx0213");

  out = xmlParseFile(argv[1]);
  if (out == NULL) {
    g_error("Error: unable to parse file:%s", argv[1]);
  }
  doc = xmlParseFile(argv[1]);

  array = GetEmbedInfoList(doc);
  rec = red2rec(doc);
  dtree = GetDTree(doc);

  RecParserInit();
  value = RecParseValueMem(rec->str,&vname);
  if (value == NULL) {
    g_error("Error: unable to read rec\n");
  }
  if (!IS_VALUE_RECORD(value)) {
    g_error("Error: invalid value type:%d\n",
      ValueType(value));
  }

  /* parse out */
  skip = getPageSkip(out);
  delete_layers(xmlDocGetRootElement(out));

  for(i = 2; i < argc; i++) {
    if (!g_file_get_contents(argv[i],&buf,&size,NULL)) {
      g_error("Error: unable to read data file:%s\n",argv[i]);
    }
    OpenCOBOL_UnPackValue(conv,(unsigned char*)buf,value);
    _red2embed(array,value,dtree);
    if (i != 2) {
      page_skip(doc,skip);
    }
    add_layers(out,doc);
    g_free(buf);
  }

  xmlKeepBlanksDefault(0);
  xmlDocDumpFormatMemory(out,&outmem,&outsize,1);

  xmlFreeDoc(out);
  xmlFreeDoc(doc);
  return g_strdup(CAST_BAD(outmem));
}
