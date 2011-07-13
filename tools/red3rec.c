/* -*- Mode: C; c-basic-offset: 2 -*-
 * dia2rec.c
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#include "dtree.h"
#include "red3lib.h"

static int multiplier = 3;
static char *outfile = NULL;
static gboolean use_as_byte = FALSE;

static void
fprintf_tab(FILE *fp,int l,char *format,...)
{
  int i;
  va_list va;

  for (i=0;i<l;i++) {
    fprintf(fp,"  ");
  }
  va_start(va,format);
  vfprintf(fp,format,va);
  va_end(va);
}

static void
print_rec(FILE *fp,int l,DicNode *node)
{
  DicNode *child;
  gchar *name;

  name = EscapeNodeName(node->name);

  if (node->type == DIC_NODE_TYPE_NODE) {
    fprintf_tab(fp,l,"%s {\n",name);
    for(child=DNODE_CHILDREN(node);child!=NULL;child=DNODE_NEXT(child)) {
      print_rec(fp,l+1,child);
    }
    if (node->occurs > 1) {
      fprintf_tab(fp,l,"}[%d];\n",node->occurs);
    } else {
      fprintf_tab(fp,l,"};\n");
    }
  } else if(node->type == DIC_NODE_TYPE_TEXT){
    if (node->occurs > 1) {
      fprintf_tab(fp,l,"%s varchar(%d)[%d];\n",
        name,
        node->length * multiplier,
        node->occurs);
    } else {
      fprintf_tab(fp,l,"%s varchar(%d);\n",
        name,
        node->length * multiplier);
    }
  } else if(node->type == DIC_NODE_TYPE_IMAGE){
    if (node->occurs > 1) {
      fprintf_tab(fp,l,"%s varchar(%d)[%d];\n",
        name,
        DNODE_IMAGE_PATH_SIZE,
        node->occurs);
    } else {
      fprintf_tab(fp,l,"%s varchar(%d);\n",
        name,
        DNODE_IMAGE_PATH_SIZE);
    }
  }
  g_free(name);
}

static void
red3rec(char *infile)
{
  xmlDocPtr doc;
  xmlNodePtr dict;
  DicNode *dtree;
  DicNode *child;

  FILE *fp = NULL;

  if (use_as_byte) {
    multiplier = 1;
  }

  xmlInitParser();
  LIBXML_TEST_VERSION

  doc = xmlParseFile(infile);
  if (doc == NULL) {
    fprintf(stderr, "Error: unable to parse file \"%s\"\n", infile);
    return;
  }
  dict = FindNodeByTag(doc->xmlRootNode->xmlChildrenNode,
    BAD_CAST(MONPE_XML_DICTIONARY));
  if (dict == NULL) {
    fprintf(stderr,"no dictionary data\n");
    exit(1);
  }
  dtree = dtree_new();
  dtree_new_from_xml(&dtree,dict);

  if (outfile != NULL) {
    fp = fopen(outfile,"w");
  }
  if (fp == NULL) {
    fp = stdout;
  } 

  fprintf(fp,"rec {\n");
  for (child = DNODE_CHILDREN(dtree);child!=NULL;child=DNODE_NEXT(child)) {
    print_rec(fp,1,child);
  }
  fprintf(fp,"};\n");

  if (fp != stdout) {
    fclose(fp);
  }
  xmlFreeDoc(doc);
}

static GOptionEntry entries[] =
{
  { "out",'o',0,G_OPTION_ARG_STRING,&outfile,"output filename",NULL},
  { "as-byte",'b',0,G_OPTION_ARG_NONE,&use_as_byte,"use size as byte size",NULL},
  { NULL}
};

int main(int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *ctx;

  ctx = g_option_context_new("<.red3>");
  g_option_context_add_main_entries(ctx, entries, NULL);
  if (!g_option_context_parse(ctx,&argc,&argv,&error)) {
    g_print("option parsing failed:%s\n",error->message);
    exit(1);
  }
  if (argc < 2) {
    g_print("%s",g_option_context_get_help(ctx,TRUE,NULL));
    exit(1);
  }
  red3rec(argv[1]);
  return 0;
}
