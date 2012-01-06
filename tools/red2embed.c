/* -*- Mode: C; c-basic-offset: 2 -*-
 * red2embed.c
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

static void
_red2embed(char *diafile,
  char *datafile)
{
  xmlDocPtr doc;
  gchar *buf;
  gsize size;
  GString *embed;

  xmlInitParser();
  LIBXML_TEST_VERSION

  doc = xmlParseFile(diafile);
  if (doc == NULL) {
    g_error("Error: unable to parse file:%s", diafile);
  }

  if (!g_file_get_contents(datafile,&buf,&size,NULL)) {
    g_error("Error: unable to read data file:%s\n",datafile);
  }

  embed = red2embed(doc,buf);

  if (outfile != NULL) {
    if (!g_file_set_contents(outfile,embed->str,embed->len,NULL)) {
      g_error("Error: unable to write file:%s\n",outfile);
    }
  } else {
    printf("%s",embed->str);
  }

  g_free(buf);
  xmlFreeDoc(doc);
  g_string_free(embed,TRUE);
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

  ctx = g_option_context_new("<.red> <.dat>");
  g_option_context_add_main_entries(ctx, entries, NULL);
  if (!g_option_context_parse(ctx,&argc,&argv,&error)) {
    g_print("option parsing failed:%s\n",error->message);
    exit(1);
  }
  if (argc < 3) {
    g_print("%s",g_option_context_get_help(ctx,TRUE,NULL));
    exit(1);
  }
  _red2embed(argv[1],argv[2]);
  return 0;
}
