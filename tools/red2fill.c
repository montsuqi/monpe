/* -*- Mode: C; c-basic-offset: 2 -*-
 * red2fill.c
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#include "dtree.h"
#include "red2lib.h"

static char *imagepath = NULL;
static char *outfile = NULL;

static void
_red2fill(char *infile)
{
  xmlDocPtr doc;
  GString *fill;

  FILE *fp = NULL;

  xmlInitParser();
  LIBXML_TEST_VERSION

  doc = xmlParseFile(infile);
  if (doc == NULL) {
    fprintf(stderr, "Error: unable to parse file \"%s\"\n", infile);
    return;
  }
  fill = red2fill(doc,imagepath);

  if (outfile != NULL) {
    fp = fopen(outfile,"w");
  }
  if (fp == NULL) {
    fp = stdout;
  } 
  fprintf(fp,"%s",fill->str);
  if (fp != stdout) {
    fclose(fp);
  }
  xmlFreeDoc(doc);
  g_string_free(fill,TRUE);
}

static GOptionEntry entries[] =
{
  { "out",'o',0,G_OPTION_ARG_STRING,&outfile,"output filename",NULL},
  { "image",'i',0,G_OPTION_ARG_STRING,&imagepath,"embed image filename",NULL},
  { NULL}
};

int main(int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *ctx;

  ctx = g_option_context_new("<.red2> <prefix>");
  g_option_context_add_main_entries(ctx, entries, NULL);
  if (!g_option_context_parse(ctx,&argc,&argv,&error)) {
    g_print("option parsing failed:%s\n",error->message);
    exit(1);
  }
  if (argc < 2) {
    g_print("%s",g_option_context_get_help(ctx,TRUE,NULL));
    exit(1);
  }
  _red2fill(argv[1]);
  return 0;
}
