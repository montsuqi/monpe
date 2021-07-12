/* -*- Mode: C; c-basic-offset: 2 -*-
 * red2info.c
 * show red meta info
 *
 * $ red2info in.red
 * A4,portrait
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#include "red2lib.h"

int main(int argc, char *argv[])
{
  if (argc < 2) {
    printf("$ red2info in.red\n");
    exit(1);
  }
  red2info(argv[1]);
  return 0;
}
