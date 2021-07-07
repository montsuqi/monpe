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

static GOptionEntry entries[] =
{
  { "out",'o',0,G_OPTION_ARG_STRING,&outfile,"output filename",NULL},
  { NULL}
};

int main(int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *ctx;

  ctx = g_option_context_new("<.red> <.dat> <.dat> ...");
  g_option_context_add_main_entries(ctx, entries, NULL);
  if (!g_option_context_parse(ctx,&argc,&argv,&error)) {
    g_print("option parsing failed:%s\n",error->message);
    exit(1);
  }
  if (argc < 3) {
    g_print("%s",g_option_context_get_help(ctx,TRUE,NULL));
    exit(1);
  }
  red2embed(argc,argv,outfile);
  return 0;
}
