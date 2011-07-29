/* -*- Mode: C; c-basic-offset: 2 -*-
 * red2test.c
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#include "red2lib.h"

static char *outfile = NULL;
static char *imagepath = NULL;

static void
_red2test(char *diafile)
{
  xmlDocPtr doc;
  GString *embed;
  GString *fill;

  xmlInitParser();
  LIBXML_TEST_VERSION

  doc = xmlParseFile(diafile);
  if (doc == NULL) {
    g_error("Error: unable to parse file:%s", diafile);
  }

  fill = red2fill(doc,imagepath);
  embed = red2embed(doc,fill->str);

  if (outfile != NULL) {
    if (!g_file_set_contents(outfile,embed->str,embed->len,NULL)) {
      g_error("Error: unable to write file:%s\n",outfile);
    }
  } else {
    printf("%s",embed->str);
  }

  xmlFreeDoc(doc);
  g_string_free(embed,TRUE);
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

  ctx = g_option_context_new("<.red2>");
  g_option_context_add_main_entries(ctx, entries, NULL);
  if (!g_option_context_parse(ctx,&argc,&argv,&error)) {
    g_print("option parsing failed:%s\n",error->message);
    exit(1);
  }
  if (argc < 2) {
    g_print("%s",g_option_context_get_help(ctx,TRUE,NULL));
    exit(1);
  }
  _red2test(argv[1]);
  return 0;
}
