#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

/* glib */
#include <glib.h>
#include "dtree.h"

DicNode *
dnode_new(char *name, int occurs, DicNodeType type, int length,
  DicNode *parent, DicNode *sibling)
{
  DicNode *node;

  node = g_malloc0(sizeof(DicNode));
  g_assert(node != NULL);

  dnode_initialize(node, name, occurs, type, length,parent, sibling);

  return node;
}

void
dnode_initialize(DicNode *this, char *name, int occurs, 
                 DicNodeType type, int length,
                 DicNode *parent, DicNode *sibling)
{
  GNode *g;
  g_assert(this != NULL);

  /* initialize parent (GNode) */
  g           = G_NODE(this);
  g->data     = NULL;
  g->next     = NULL;    
  g->prev     = NULL;
  g->parent   = NULL;
  g->children = NULL;
  
  this->istop = FALSE;
  this->name = g_strdup(name);
  this->occurs = occurs;
  this->type = type;
  this->length = length;
  this->objects = NULL;
  if (parent != NULL) {
    dtree_insert_before(parent, sibling, this);
  }
}

void
dnode_update(DicNode *node)
{
  /* leaf data arrange*/
#if 0
  GList *plist, *ilist;
  MPtrArray *m;

  /* if node isn't leaf then return */
  if (!node->is_leaf) return;

  plist = dnode_get_parent_list(node);
  ilist = dnode_parent_list_to_index_list(plist);
  g_list_free(plist);

  m = mpary_new_with_list(ilist);
  g_list_free(ilist);

  if (dleaf_data(node) != NULL) {
    int index[100];
    int i, j;

    if (dleaf_data_dim(node) != m->dim || dleaf_data_all(node) == m->all) {
      /* change structure */
      mpary_copy(m, dleaf_data(node));
    } else {
      /* resize */
      for (i=0; i<dleaf_data_all(node); i++) {
        gpointer data;
        pos2index(dleaf_data(node), index, i);
        data = mpary_get_data(dleaf_data(node), index);
        mpary_set_data_with_pos(m, data, index2pos(m, index));
      }
      mpary_update(m);
    }
    mpary_delete(dleaf_data(node));
  }
  node->leaf.objects = m;
#endif
}

void
dnode_dump(DicNode *node)
{
  printf("dnode:%s(%d) ", node->name, node->occurs);
  switch (node->type) {
  case DIC_NODE_TYPE_NODE:
    printf("\n");
    break;
  case DIC_NODE_TYPE_STRING:
    printf("string:%d\n",node->length);
    break;
  case DIC_NODE_TYPE_IMAGE:
    printf("image:%d\n",node->length);
    break;
  default: /* error */
    printf("\n");
  }
}

int
dnode_get_n_objects(DicNode *node)
{
  if (node->objects == NULL) {
    return 0;
  }
  return g_list_length(node->objects);
}

int 
dnode_get_n_used_objects(DicNode * node)
{
  int i,n;

  if (node->objects == NULL) {
    return 0;
  }
  for(i=n=0;i<g_list_length(node->objects);i++){
    if (g_list_nth_data(node->objects,i) != NULL) {
      n++;
    }
  }
  return n;
}

int 
dnode_calc_total_occurs(DicNode *node)
{
  if (node == NULL || DNODE_PARENT(node) == NULL) {
    return 1;
  }
  return node->occurs * dnode_calc_total_occurs(DNODE_PARENT(node));
}

gboolean
dnode_data_is_used(DicNode *node)
{
  int i;

  if (node->objects == NULL) {
    return FALSE;
  }
  for(i;i<g_list_length(node->objects);i++){
    if (g_list_nth_data(node->objects,i) != NULL) {
      return TRUE;
    }
  }
  return FALSE;
}

GList *
dnode_new_objects(int size)
{
  GList *list = NULL;
  int i;
  
  for(i=0;i<size;i++) {
    list = g_list_append(list,NULL);
  }
  return list;
}

void
dnode_reset_objects(DicNode *node)
{
  int i;

  if (node->objects != NULL) {
    for(i=0;i<g_list_length(node->objects);i++) {
      /* remove objects */
    }
    g_list_free(node->objects);
  }
  node->objects = dnode_new_objects(dnode_calc_total_occurs(node));
}

/* dtree */

DicNode *
dtree_new(void)
{
  DicNode *node;
  node = dnode_new(NULL, 1, DIC_NODE_TYPE_NODE,0, NULL, NULL);
  node->istop = TRUE;
  return node;
}

void
dtree_insert_before(DicNode *parent, DicNode *sibling, DicNode *node)
{
  g_node_insert_before(G_NODE(parent), G_NODE(sibling), G_NODE(node));
  dtree_update(node);
}

void
dtree_update(DicNode *tree)
{
  DicNode *p;
  
  dnode_update(tree);
  for (p = DNODE_CHILDREN(tree); p != NULL; p = DNODE_NEXT(p)) {
    dtree_update(p);
  }
}

static void
tree_dump_sub(GNode *node, gpointer data)
{
  gint indent = GPOINTER_TO_INT(data);
  int i;

  for(i=0;i<indent;i++) {
    printf("  ");
  }
  dnode_dump(DNODE(node));
  g_node_children_foreach(node, G_TRAVERSE_ALL, tree_dump_sub,
                          GINT_TO_POINTER(indent+1));
}

void
dtree_dump(DicNode *tree)
{
  gint indent = 0;
  
  printf("-----\n");
  g_node_children_foreach(G_NODE(tree), G_TRAVERSE_ALL, tree_dump_sub,
                          GINT_TO_POINTER(indent));
}

void
dtree_unlink(DicNode *tree)
{
  g_node_unlink(G_NODE(tree));
}

void 
dtree_move_before(DicNode *node,DicNode *parent,DicNode *sibling)
{
  dtree_unlink(node);
  g_node_insert_before(parent,sibling,node);
}

void 
dtree_move_after(DicNode *node,DicNode *parent,DicNode *sibling)
{
  dtree_unlink(node);
  g_node_insert_after(parent,sibling,node);
}

/*************************************************************
 * Local Variables:
 * mode:c
 * tab-width:2
 * indent-tabs-mode:nil
 * End:
 ************************************************************/
