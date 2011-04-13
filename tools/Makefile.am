bin_PROGRAMS = \
	red3rec \
	red3embed \
	red3conv

base_sources =	\
	red3parser.c	red3parser.h

base_CFLAGS = \
	-I$(top_srcdir)		\
	$(M3TOOLS_CFLAGS)

base_LDADD = \
	-lm					\
	$(M3TOOLS_LIBS)

red3rec_SOURCES = red3rec.c \
	$(base_sources)

red3rec_CFLAGS = \
	$(base_CFLAGS)

red3rec_LDADD = \
	$(base_LDADD)

red3embed_SOURCES = red3embed.c \
	$(base_sources)

red3embed_CFLAGS = \
	$(base_CFLAGS)

red3embed_LDADD = \
	$(base_LDADD)

red3conv_SOURCES = red3conv.c \
	$(base_sources)

red3conv_CFLAGS = \
	$(base_CFLAGS)

red3conv_LDADD = \
	$(base_LDADD)
