/* -*- Mode: C; c-basic-offset: 2 -*-
 * red2test.c
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <glib.h>

#include "red2lib.h"

static char *outfile = NULL;
static char *imagepath = NULL;

static GOptionEntry entries[] =
{
  { "out",'o',0,G_OPTION_ARG_STRING,&outfile,"output filename",NULL},
  { "image",'i',0,G_OPTION_ARG_STRING,&imagepath,"embed image filename",NULL},
  { NULL}
};

int main(int argc, char *argv[])
{
  gchar *dfile;
  GError *error = NULL;
  GOptionContext *ctx;
  gchar *buf;
  char *nargv[3];

  ctx = g_option_context_new("<.red>");
  g_option_context_add_main_entries(ctx, entries, NULL);
  if (!g_option_context_parse(ctx,&argc,&argv,&error)) {
    g_print("option parsing failed:%s\n",error->message);
    exit(1);
  }
  if (argc < 2) {
    g_print("%s",g_option_context_get_help(ctx,TRUE,NULL));
    exit(1);
  }
  dfile = g_strdup_printf("/tmp/red2test.%d.dat",(int)getpid());
  red2fill(argv[1],imagepath,dfile);
  nargv[0] = "";
  nargv[1] = argv[1];
  nargv[2] = dfile;
  buf = red2embed(3,nargv);
  remove(dfile);
  if (outfile != NULL) {
    if (!g_file_set_contents(outfile,buf,strlen(buf),NULL)) {
      g_error("Error: unable to write file:%s\n",outfile);
    }
  } else {
    printf("%s",buf);
  }
  return 0;
}
