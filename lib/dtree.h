#ifndef __DTREE_H_INCLUDED_
#define __DTREE_H_INCLUDED_

#include <glib.h> /* GNode, gboolean */

typedef struct _DicNode DicNode;
typedef DicNode DicTree;

typedef enum {
  DIC_NODE_TYPE_NODE = 0,
  DIC_NODE_TYPE_STRING,
  DIC_NODE_TYPE_IMAGE,
  DIC_NODE_TYPE_END
} DicNodeType;

#define DNODE_MAX_HEIGHT 10

struct _DicNode
{
  /* parent class */
  GNode gnode;
  /* node property  */
  char *name;
  int occurs;
  DicNodeType type;
  /* leaf property */
  int length;
  GPtrArray *objects;
};

/* default */

#define DNODE_DEFAULT_NAME_NODE   "NODE"
#define DNODE_DEFAULT_NAME_STRING "STRING"
#define DNODE_DEFAULT_NAME_IMAGE  "IMAGE"
#define DNODE_DEFAULT_OCCURS 1
#define DNODE_DEFAULT_LENGTH 10

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

/* dtree  */
DicNode* dtree_new(void);
void dtree_insert_before(DicNode *parent, DicNode *sibling,
                         DicNode *node);

void dtree_update(DicNode *tree);
void dtree_dump(DicNode *node);

#endif 

/*************************************************************
 * Local Variables:
 * mode:c
 * tab-width:2
 * indent-tabs-mode:nil
 * End:
 ************************************************************/
