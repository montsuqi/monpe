bin_PROGRAMS = \
	red2rec \
	red2embed \
	red2conv

base_sources =	\
	red2lib.c	red2lib.h

base_CFLAGS = \
	-I$(top_srcdir)		\
	-I$(top_srcdir)/lib	\
	$(M3TOOLS_CFLAGS)

base_LDADD = \
	-lm					\
	../lib/libmonpe.la	\
	$(M3TOOLS_LIBS)

red2rec_SOURCES = red2rec.c \
	$(base_sources)

red2rec_CFLAGS = \
	$(base_CFLAGS)

red2rec_LDADD = \
	$(base_LDADD)

red2embed_SOURCES = red2embed.c \
	$(base_sources)

red2embed_CFLAGS = \
	$(base_CFLAGS)

red2embed_LDADD = \
	$(base_LDADD)

red2conv_SOURCES = red2conv.c \
	$(base_sources)

red2conv_CFLAGS = \
	$(base_CFLAGS)

red2conv_LDADD = \
	$(base_LDADD)
