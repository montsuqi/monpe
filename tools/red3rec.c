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

#include "red3parser.h"

static char *outfile = NULL;
static gboolean use_as_byte = FALSE;

static void
red3rec(char *infile)
{
  xmlDocPtr doc;
  GPtrArray *array;
  EmbedInfo *info;
  FILE *fp = NULL;
  int i,j;
  int multiplier = 3;

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
  array = GetEmbedInfoList(doc);

  if (array->len <= 0) {
    fprintf(stderr,"no embed object\n");
    exit(1);
  }

  if (outfile != NULL) {
    fp = fopen(outfile,"w");
  }
  if (fp == NULL) {
    fp = stdout;
  } 

  fprintf(fp,"rec {\n");
  for(i=0;i<array->len;i++) {
    info = (EmbedInfo*)g_ptr_array_index(array,i);
    switch(info->type) {
    case EMBED_TYPE_TEXT:
      fprintf(fp,"  %s varchar(%d);\n",
        info->id,EmbedInfoAttr(info,Text,text_size)*multiplier);
      break;
    case EMBED_TYPE_ARRAY:
      fprintf(fp,"  %s varchar(%d)[%d];\n",
        info->id,EmbedInfoAttr(info,Array,text_size)*multiplier,
        EmbedInfoAttr(info,Array,array_size));
      break;
    case EMBED_TYPE_TABLE:
      fprintf(fp,"  %s {\n", info->id);
      for (j = 0; j < EmbedInfoAttr(info,Table,cols);j++) {
        fprintf(fp,"    col%d varchar(%d);\n",
          j+1,EmbedInfoAttr(info,Table,text_sizes[j])*multiplier);
      }
      fprintf(fp,"  }[%d];\n",EmbedInfoAttr(info,Table,rows));
      break;
    case EMBED_TYPE_IMAGE:
      fprintf(fp,"  %s varchar(%d);\n",
        info->id,EmbedInfoAttr(info,Image,path_size));
      break;
    }
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
