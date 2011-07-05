#ifndef __DTREE_H_INCLUDED_
#define __DTREE_H_INCLUDED_

#include <glib.h> /* GNode, gboolean */

typedef struct _DicNode DicNode;
typedef DicNode DicTree;

typedef enum {
  DIC_NODE_TYPE_NODE = 0,
  DIC_NODE_TYPE_TEXT,
  DIC_NODE_TYPE_IMAGE,
  DIC_NODE_TYPE_END
} DicNodeType;

#define DNODE_MAX_HEIGHT 10

struct _DicNode
{
  /* parent class */
  GNode gnode;
  /* node property  */
  gboolean istop;
  char *name;
  int occurs;
  DicNodeType type;
  /* leaf property */
  int length;
  GList *objects;
};

/* default */

#define DNODE_DEFAULT_NAME_NODE   "NODE"
#define DNODE_DEFAULT_NAME_TEXT   "TEXT"
#define DNODE_DEFAULT_NAME_IMAGE  "IMAGE"
#define DNODE_DEFAULT_NODE_OCCURS 10
#define DNODE_DEFAULT_OCCURS 1
#define DNODE_DEFAULT_LENGTH 10

#define DNODE_MAX_DEPTH 100

#define G_NODE(x)         ((GNode *)x)
#define DNODE(x)          ((DicNode *)x)
#define DNODE_PARENT(x)   (DNODE(G_NODE(x)->parent))
#define DNODE_CHILDREN(x) (DNODE(G_NODE(x)->children))
#define DNODE_NEXT(x)     (DNODE(G_NODE(x)->next))
#define DNODE_PREV(x)     (DNODE(G_NODE(x)->prev))
#define DNODE_DATA(x)     (DNODE(G_NODE(x)->data))

/***** dnode *****/
DicNode *dnode_new(char *name, 
                   int occurs,
                   DicNodeType type,
                   int length,
                   DicNode *parent,
                   DicNode *sibling);

void dnode_initialize(DicNode *node,
                      char *name, 
                      int occurs,
                      DicNodeType type,
                      int length,
                      DicNode *parent,
                      DicNode *sibling);

void dnode_update(DicNode *node);
void dnode_dump(DicNode *node);

int dnode_get_n_objects(DicNode *node);
int dnode_get_n_used_objects(DicNode *node);
gboolean dnode_data_is_used(DicNode *node);
int dnode_data_get_empty_index(DicNode *node);
gchar* dnode_data_get_longname(DicNode *node,int i);
int dnode_calc_total_occurs(DicNode *node);
int dnode_calc_occurs_upto_parent(DicNode *parent,DicNode *node);
int dnode_calc_occurs_upto_before_parent(DicNode *parent,DicNode *node);

void dnode_reset_objects(DicNode *node);
void dnode_reset_objects_recursive(DicNode *node);
GList * dnode_get_objects_recursive(DicNode *node,GList *list);
GList * dnode_new_objects(int size);
void dnode_set_object(DicNode *node,int index,gpointer object);

gboolean dnode_set_occurs(DicNode *node, int occurs);
void dnode_set_length(DicNode *node, int length);

/* dtree  */
DicNode* dtree_new(void);
void dtree_insert_before(DicNode *parent, DicNode *sibling,
                         DicNode *node);

void dtree_update(DicNode *tree);
void dtree_dump(DicNode *node);
void dtree_unlink(DicNode *node);
void dtree_move_before(DicNode *node,DicNode *parent,DicNode *sibling);
void dtree_move_after(DicNode *node,DicNode *parent,DicNode *sibling);

#endif 

/*************************************************************
 * Local Variables:
 * mode:c
 * tab-width:2
 * indent-tabs-mode:nil
 * End:
 ************************************************************/
