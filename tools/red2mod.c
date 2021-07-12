/* -*- Mode: C; c-basic-offset: 2 -*-
 * red2mod.c
 * modify redfile: layer operation, set offset
 *
 * $ red2mod in.red -o out.red -L "表示レイヤ1,表示レイヤ2" -x 0.1 -y 0.1
 * $ red2mod in.red -o out.red -H "非表示レイヤ1,非表示レイヤ2" -x 0.1 -y 0.1
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <locale.h>

#include "red2lib.h"

static char *show_layers = NULL;
static char *hide_layers = NULL;
static double x_offset = 0.0;
static double y_offset = 0.0;
static char *outfile = NULL;

static GOptionEntry entries[] =
{
  {"show-layers", 'L', 0, G_OPTION_ARG_STRING, &show_layers,
   "Show only specified layers: name or range 1-5",
   "LAYER,LAYER,..."},
  {"hide-layers", 'H', 0, G_OPTION_ARG_STRING, &hide_layers,
   "Hide only specified layers: name or range 1-5",
   "LAYER,LAYER,..."},
  {"x-offset", 'x', 0, G_OPTION_ARG_DOUBLE, &x_offset,
   "The x cordinate to paper offset[cm]", NULL },
  {"y-offset", 'y', 0, G_OPTION_ARG_DOUBLE, &y_offset,
   "The y cordinate to paper offset[cm]", NULL },
  { "out",'o',0,G_OPTION_ARG_STRING,&outfile,"output filename",NULL},
  { NULL}
};

int main(int argc, char *argv[])
{
  GError *error = NULL;
  GOptionContext *ctx;

  setlocale(LC_CTYPE, "ja_JP.UTF-8");
  ctx = g_option_context_new("<.red> <.red> <.red> ...");
  g_option_context_add_main_entries(ctx, entries, NULL);
  if (!g_option_context_parse(ctx,&argc,&argv,&error)) {
    g_print("option parsing failed:%s\n",error->message);
    exit(1);
  }
  if (argc < 2) {
    g_print("%s",g_option_context_get_help(ctx,TRUE,NULL));
    exit(1);
  }
  red2mod(argv[1], outfile, show_layers, hide_layers, x_offset, y_offset);
  return 0;
}
