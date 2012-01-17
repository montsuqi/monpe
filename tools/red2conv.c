/* -*- Mode: C; c-basic-offset: 2 -*-
 * red2conv.c
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <zlib.h>
#include <fcntl.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "red2lib.h"

#define RED_IMAGE_PATH_SIZE 1024

static char *outfile = NULL;

enum DictType
{
  DICT_TYPE_STRING,
  DICT_TYPE_TEXTBOX,
  DICT_TYPE_IMAGE
};

typedef struct {
  gchar *id;
  enum DictType type;
  gint length;
  gboolean located;
} DictInfo;

typedef struct {
  gchar *oldname;
  double scale;
  gchar *family;
  gchar *style;
  gchar *name;
} FontInfo;

static FontInfo Fonts[] = {
  {"OCRB"               ,1.25,"OCRB"         ,"0" ,"Courier"},
  {"OCRROSAI"           ,1.25,"OCRROSAI"     ,"0" ,"Courier"},
  {"Mincho"             ,1.25,"Takao明朝"    ,"0" ,"Courier"},
  {"明朝"               ,1.25,"Takao明朝"    ,"0" ,"Courier"},
  {"Gothic"             ,1.25,"Takaoゴシック","0" ,"Courier"},
  {"ゴシック"           ,1.25,"Takaoゴシック","0" ,"Courier"},
  {"Courier"            ,1.25,"Takao明朝"    ,"0" ,"Courier"},
  {"Courier-Oblique"    ,1.25,"Takao明朝"    ,"8" ,"Courier"},
  {"Courier-Bold"       ,1.25,"Takao明朝"    ,"80","Courier"},
  {"Courier-BoldOblique",1.25,"Takao明朝"    ,"88","Courier"},
  {"Times-Roman"        ,1.25,"Takao明朝"    ,"0" ,"Times-Roman"},
  {NULL                 ,0.0 ,NULL           ,0   ,NULL}
};

static void
fix_font_height(xmlNodePtr node,
  double scale)
{
  xmlNodePtr child;
  xmlChar *val;
  double height;

  for (child = node->children; child != NULL; child = child->next) {
    if (!xmlStrcmp(child->name,BAD_CAST("real"))) {
      val = xmlGetProp(child,BAD_CAST("val"));
      if (val != NULL) { 
        height = atof((const char*)val) * scale;
        xmlSetProp(child,BAD_CAST("val"),
          BAD_CAST(g_strdup_printf("%lf",height)));
      }
      return;
    }
  }
}

static double
fix_font_name(xmlNodePtr node)
{
  xmlNodePtr child;
  xmlChar *name;
  int i;

  for (child = node->children; child != NULL; child = child->next) {
    if (!xmlStrcmp(child->name,BAD_CAST("font"))) {
      name = xmlGetProp(child,BAD_CAST("name"));
      if (name != NULL) {
        for (i=0;Fonts[i].oldname!=NULL;i++) {
          if (!xmlStrcmp(name,BAD_CAST(Fonts[i].oldname))) {
            xmlNewProp(child,BAD_CAST("family"),BAD_CAST(Fonts[i].family));
            xmlNewProp(child,BAD_CAST("style"),BAD_CAST(Fonts[i].style));
            xmlSetProp(child,BAD_CAST("name"),BAD_CAST(Fonts[i].name));
            return Fonts[i].scale;
          }
        }
      }
    }
  }
  return 1.0;
}

static void
fix_font(xmlNodePtr node)
{
  xmlNodePtr cur;
  xmlChar *type;
  xmlChar *name;
  double scale = 1.0;

  if (!xmlStrcmp(node->name,BAD_CAST("composite"))) {
    type = xmlGetProp(node,BAD_CAST("type"));
    if (!xmlStrcmp(type,BAD_CAST("text"))) {
      for (cur = node->children; cur != NULL; cur = cur->next) {
        if (!xmlStrcmp(cur->name,BAD_CAST("attribute"))) {
          name = xmlGetProp(cur,BAD_CAST("name"));
          if (!xmlStrcmp(name,BAD_CAST("font"))) {
            scale = fix_font_name(cur);
          } else if (!xmlStrcmp(name,BAD_CAST("height"))) {
            fix_font_height(cur,scale);
          } 
        }
      }
      return ;
    }
  }
  for (cur = node->children; cur != NULL; cur = cur->next) {
    fix_font(cur);
  }
}

#define G_ERROR(p) if((p)!=NULL){g_error("%s",(p)->message);}

static void
modify_embed_image(xmlNodePtr node,
  GList *list)
{
  xmlChar *id;
  DictInfo *info;
  xmlNodePtr child,child2;
  int i;

  id = GetAttributeString(node,BAD_CAST("dnode_path"));
  if (id == NULL) {
    return;
  }

  for (i=0;i<g_list_length(list);i++) {
    info = g_list_nth_data(list,i);
    if (!xmlStrcmp(id,BAD_CAST(info->id))) {
      /*embed_id */
      child = xmlNewTextChild(node,NULL,BAD_CAST("attribute"),NULL);
      xmlNewProp(child,BAD_CAST("name"),BAD_CAST("embed_id"));
      child2 = xmlNewTextChild(child,NULL,BAD_CAST("string"),
        BAD_CAST(g_strdup_printf("#%s#",info->id)));
      /*embed_text_size*/
      child = xmlNewTextChild(node,NULL,BAD_CAST("attribute"),NULL);
      xmlNewProp(child,BAD_CAST("name"),BAD_CAST("embed_path_size"));
      child2 = xmlNewTextChild(child,NULL,BAD_CAST("int"),NULL);
      xmlNewProp(child2,BAD_CAST("val"),
        BAD_CAST(g_strdup_printf("%d",RED_IMAGE_PATH_SIZE)));
      info->located = TRUE;
    }
  }
}

static void
modify_embed_text(xmlNodePtr node,
  GList *list)
{
  xmlChar *id;
  DictInfo *info;
  xmlNodePtr child,child2;
  int i;
  int columns;

  id = GetAttributeString(node,BAD_CAST("dnode_path"));
  if (id == NULL) {
    return;
  }
  columns = GetAttributeInt(node,BAD_CAST("embed_text_column"));

  for (i=0;i<g_list_length(list);i++) {
    info = g_list_nth_data(list,i);
    if (!xmlStrcmp(id,BAD_CAST(info->id))) {
      /*embed_id*/
      child = xmlNewTextChild(node,NULL,BAD_CAST("attribute"),NULL);
      xmlNewProp(child,BAD_CAST("name"),BAD_CAST("embed_id"));
      child2 = xmlNewTextChild(child,NULL,BAD_CAST("string"),
        BAD_CAST(g_strdup_printf("#%s#",info->id)));
      /*embed_text_size*/
      child = xmlNewTextChild(node,NULL,BAD_CAST("attribute"),NULL);
      xmlNewProp(child,BAD_CAST("name"),BAD_CAST("embed_text_size"));
      child2 = xmlNewTextChild(child,NULL,BAD_CAST("int"),NULL);
      xmlNewProp(child2,BAD_CAST("val"),
        BAD_CAST(g_strdup_printf("%d",info->length)));
      /*embed_column_size*/
      child = xmlNewTextChild(node,NULL,BAD_CAST("attribute"),NULL);
      xmlNewProp(child,BAD_CAST("name"),BAD_CAST("embed_column_size"));
      child2 = xmlNewTextChild(child,NULL,BAD_CAST("int"),NULL);
      if (columns <= 0) {
        xmlNewProp(child2,BAD_CAST("val"),
          BAD_CAST(g_strdup_printf("%d",0)));
      } else {
        xmlNewProp(child2,BAD_CAST("val"),
          BAD_CAST(g_strdup_printf("%d",columns/2)));
      }
      info->located = TRUE;
    }
  }
}

static void
modify_embed_object(xmlNodePtr node,
  GList *list)
{
  xmlNodePtr cur;
  xmlChar *type;

  if (!xmlStrcmp(node->name,BAD_CAST("object"))) {
    type = xmlGetProp(node,BAD_CAST("type"));
    if (type != NULL) {
      if (!xmlStrcmp(type,BAD_CAST("Embed - Text"))) {
        modify_embed_text(node,list);
      } else if (!xmlStrcmp(type,BAD_CAST("Embed - Image"))) {
        modify_embed_image(node,list);
      } else if (!xmlStrcmp(type,BAD_CAST("Embed - TextBox"))) {
        /* do nothing*/
        node->name = BAD_CAST(g_strdup("undef"));
      }
    }
    return;
  }

  for (cur = node->children; cur != NULL; cur = cur->next) {
    modify_embed_object(cur,list);
  }
}

static GList*
append_dict_list(xmlNodePtr node,
  xmlChar *path,
  GList *list)
{
  GList *retlist = list;
  xmlChar *object;
  xmlChar *len;
  DictInfo *info;
  enum DictType type;

  object = xmlGetProp(node,BAD_CAST("object"));
  if (object == NULL) {
    return retlist;
  } 
  if (!xmlStrcmp(object,BAD_CAST("textbox"))) {
    type = DICT_TYPE_TEXTBOX;
  } else if (!xmlStrcmp(object,BAD_CAST("string"))) {
    type = DICT_TYPE_STRING;
  } else if (!xmlStrcmp(object,BAD_CAST("image"))) {
    type = DICT_TYPE_IMAGE;
  } else {
    return retlist;
  }
  info = (DictInfo *)g_new0(DictInfo,1);
  info->id = CAST_BAD(xmlStrdup(path));
  info->type = type;
  info->located = FALSE;

  if (type == DICT_TYPE_TEXTBOX || type == DICT_TYPE_STRING) {
    len = xmlGetProp(node,BAD_CAST("length"));
    if (len == NULL) {
      info->length = 256;
    } else {
      info->length = atoi((const char*)len);
      if (info->length <= 0) {
        info->length = 256;
      }
    }
  } else {
    info->length = 256;
  }
  retlist = g_list_append(list,info);
  return retlist;
}

static GList*
_get_dict_list(xmlNodePtr node,xmlChar *path,GList *list)
{
  GList *retlist = list;
  xmlNodePtr child,embed;
  xmlChar *name;
  xmlChar *newpath;
  xmlChar *prop;
  int i;
  int occurs;

  child = node->children;
  while(child != NULL) {
    if (!xmlStrcmp(child->name,BAD_CAST("element"))) {
      name = xmlGetProp(child,BAD_CAST("name"));
      if (name == NULL) {
        g_warning("element name = NULL");
        break;
      }
      occurs = 0;
      prop = xmlGetProp(child,BAD_CAST("occurs"));
      if (prop != NULL) {
        occurs = atoi((const char*)prop);
      }
      if (occurs <= 1) {
        if (path == NULL) {
          newpath = BAD_CAST(g_strdup_printf("%s",name));
        } else {
          newpath = BAD_CAST(g_strdup_printf("%s.%s",path,name));
        }
        retlist = _get_dict_list(child,newpath,retlist);
      } else {
        for(i=0;i<occurs;i++) {
          if (path == NULL) {
            newpath = BAD_CAST(g_strdup_printf("%s[%d]",name,i));
          } else {
            newpath = BAD_CAST(g_strdup_printf("%s.%s[%d]",path,name,i));
          }
          retlist = _get_dict_list(child,newpath,retlist);
        }
      }
    } else if (!xmlStrcmp(child->name,BAD_CAST("appinfo"))) {
      embed = child->children;
      while (embed != NULL) {
        if (!xmlStrcmp(embed->name,BAD_CAST("embed"))) {
          retlist = append_dict_list(embed,path,retlist);
        }
        embed = embed->next;
      }
    }
    child = child->next;
  }
  return retlist;
}

static GList*
get_dict_list(xmlDocPtr doc)
{
  GList *list = NULL;
  xmlXPathContextPtr xpathCtx; 
  xmlXPathObjectPtr xpathObj; 
  xmlChar *xpathExpr = BAD_CAST("//dictionarydata");
  xmlNodeSetPtr nodes;

  int i,size;

  if (doc == NULL) {
    g_error("doc = NULL");
  }

  xpathCtx = xmlXPathNewContext(doc);
  if(xpathCtx == NULL) {
    g_error("error: unable to create new XPath context");
  }

  /* Evaluate xpath expression */
  xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
  if(xpathObj != NULL) {
    nodes = xpathObj->nodesetval;
    size = (nodes) ? nodes->nodeNr : 0;
    
    for(i = 0; i < size; i++) {
      if (nodes->nodeTab[i] == NULL) {
        continue;
      }
      if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE) {
        list = _get_dict_list(nodes->nodeTab[i],NULL,list);
      }
    }
  }
  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx); 

  return list;
}

static gboolean
eval_cb(
  const GMatchInfo *info,
  GString *res,
  gpointer data)
{
  gchar *match;
  guchar c[2];

  match = g_match_info_fetch(info, 1);
  c[0] = (guchar)atoi(match);
  c[1] = 0;
  g_string_append (res, (const gchar *)c);
  g_free(match);
  return FALSE;
}

static gchar *
fix_character_encoding(gchar *in)
{
  gchar *buf1;
  gchar *buf2;
  gchar *buf3;
  gchar *buf4;
  gchar *buf5;
  GRegex *reg;
  GError *err = NULL;
  gsize br,bw;

  reg = g_regex_new("&#([0-9]+);",G_REGEX_MULTILINE,0,NULL); 
  buf1 = g_regex_replace_eval(reg,in,-1,0,0,eval_cb,NULL,&err);
  if (err != NULL) {
    g_error("error:g_regex_replace_eval");
  }
  g_regex_unref(reg);

  reg = g_regex_new("<dia:",0,0,NULL); 
  buf2 = g_regex_replace(reg,buf1,-1,0,"<",0,&err);
  if (err != NULL) {
    g_error("error:g_regex_replace");
  }
  g_regex_unref(reg);

  reg = g_regex_new("<\\?xml.*\\?>",0,0,NULL);
  buf3 = g_regex_replace(reg,buf2,-1,0,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>",0,&err);
  if (err != NULL) {
    g_error("error:g_regex_replace");
  }
  g_regex_unref(reg);

  reg = g_regex_new("</dia:",0,0,NULL); 
  buf4 = g_regex_replace(reg,buf3,-1,0,"</",0,&err);
  if (err != NULL) {
    g_error("error:g_regex_replace");
  }
  g_regex_unref(reg);

  buf5 = g_convert(buf4,-1,"utf-8", "euc-jisx0213",&br, &bw, &err);
  if (err != NULL) {
    g_error("error:g_convert");
  }

  g_free(buf1);
  g_free(buf2);
  g_free(buf3);
  g_free(buf4);
  return buf5;
}

#define GZ_MODE "rb6f"
#define BUF_SIZE (512)

static gchar *
read_red(gchar *fname)
{
  gint fd;
  gzFile gzfile;
  gsize readbytes;
  gsize size;
  gchar buf[BUF_SIZE];
  gchar *ret;

  fd = g_open(fname,O_RDONLY);
  if (fd < 0) {
     return g_strdup("");
  }
  gzfile = gzdopen(fd, GZ_MODE);
  if (gzfile != NULL) {
    /* dummy read to get deflate size*/
    size = 0;
	while((readbytes = gzread(gzfile, buf, sizeof(buf))) > 0 ) {
      size += readbytes;
    }
    gzrewind(gzfile);
    ret = g_malloc(size + 1);
    ret[size] = 0;
    gzread(gzfile,ret,size);
    gzclose(gzfile);
  } else {
    /* normal read */
    gchar *buf;
    g_file_get_contents(fname,&buf,&size,NULL);
    ret = g_realloc(buf,size + 1);
    ret[size] = 0;
    g_free(buf);
  }
  return ret;
}

static void
set_name_space(xmlNodePtr node,xmlNs *ns)
{
  xmlNodePtr child;

  if (node == NULL) {
    return ;
  }

  /* hey libxml2 does not add prefix ;I */
  if (!g_regex_match_simple("<dia:",CAST_BAD(node->name),0,0)) {
    gchar *name = g_strdup_printf("dia:%s",CAST_BAD(node->name));
    xmlNodeSetName(node,BAD_CAST(name));
    g_free(name);
  }

  for(child = node->children; child != NULL; child = child->next)
  {
    set_name_space(child, ns);
  }
}

static void
red2conv(char *infile)
{
  xmlDocPtr doc;
  xmlNodePtr root;
  gchar *buf1;
  gchar *buf2;
  GList *list;
  xmlNs *ns;

  xmlChar *outmem;
  int outsize;

  xmlInitParser();
  LIBXML_TEST_VERSION

  /* preprocess fix character encoding */

  buf1 = read_red(infile);
  if (buf1 == NULL) {
    g_error("read_red failure\n");
  }
  buf2 = fix_character_encoding(buf1);
  if (buf2 == NULL) {
  	g_error("convert_red_encoding\n");
  }

  doc = xmlReadMemory((const char*)buf2,strlen(buf2),
    "",NULL, XML_PARSE_NOBLANKS|XML_PARSE_NOENT);
  g_free(buf2);
  if (doc == NULL) {
  	g_error("xmlReadMemory fail\n");
  }

  root = xmlDocGetRootElement(doc);

  /* dnode_path list */
  list = get_dict_list(doc);

  /* add embed attribute */
  modify_embed_object(root,list);

  /* fix font */
  fix_font(root);

  /* set name space */
  ns = xmlNewNs(doc->xmlRootNode,
    BAD_CAST("http://www.lysator.liu.se/~alla/dia/"),
    BAD_CAST("dia"));
  set_name_space(doc->xmlRootNode, ns);

  /* output*/
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

  ctx = g_option_context_new(" <.red>");
  g_option_context_add_main_entries(ctx, entries, NULL);
  if (!g_option_context_parse(ctx,&argc,&argv,&error)) {
    g_print("option parsing failed:%s\n",error->message);
    exit(1);
  }
  if (argc < 2) {
    g_print("%s",g_option_context_get_help(ctx,TRUE,NULL));
    exit(1);
  }
  red2conv(argv[1]);
  return 0;
}
