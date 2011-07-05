## Process this file with automake to produce Makefile.in
pkglib_LTLIBRARIES = 	libembed_objects.la
libembed_objects_la_SOURCES = \
	embed.c \
	etextobj.c \
	etextobj.h \
	eimage.c \
	eimage.h \
	textcircle.c \
	utils.h \
	utils.c

libembed_objects_la_LDFLAGS = -export-dynamic -module -avoid-version

INCLUDES = -I$(top_srcdir)/intl -I$(srcdir)/../../lib \
	$(DEBUG_FLAGS) $(GTK_CFLAGS)

EXTRA_DIST = \
	pixmaps/eimage.xpm \
	pixmaps/etext.xpm \
	pixmaps/textcircle.xpm
