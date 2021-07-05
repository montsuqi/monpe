bin_PROGRAMS = \
	red2rec \
	red2embed \
	red2conv \
	red2inc \
	red2fill \
	red2test \
	red2cat

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

red2test_SOURCES = red2test.c \
	$(base_sources)

red2test_CFLAGS = \
	$(base_CFLAGS)

red2test_LDADD = \
	$(base_LDADD)

red2fill_SOURCES = red2fill.c \
	$(base_sources)

red2fill_CFLAGS = \
	$(base_CFLAGS)

red2fill_LDADD = \
	$(base_LDADD)

red2rec_SOURCES = red2rec.c \
	$(base_sources)

red2rec_CFLAGS = \
	$(base_CFLAGS)

red2rec_LDADD = \
	$(base_LDADD)

red2inc_SOURCES = red2inc.c \
	$(base_sources)

red2inc_CFLAGS = \
	$(base_CFLAGS)

red2inc_LDADD = \
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

red2cat_SOURCES = red2cat.c \
	$(base_sources)

red2cat_CFLAGS = \
	$(base_CFLAGS)

red2cat_LDADD = \
	$(base_LDADD)
