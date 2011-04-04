#ifndef EMBED_UTILS_H
#define EMBED_UTILS_H

#include <glib.h>

extern gchar * get_default_embed_id(gchar *id);
extern void register_embed_id(gchar *id);

#undef	GLOBAL
#ifdef	EMBED_UTILS_MAIN
#define	GLOBAL		/*	*/
#else
#define	GLOBAL		extern
#endif
GLOBAL	GHashTable *EmbedIDTable;

#endif
