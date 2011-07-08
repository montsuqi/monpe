/* Dia -- an diagram creation/manipulation program
 * Copyright (C) 1998 Alexander Larsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Parts of this file are derived from the file layers_dialog.c in the Gimp:
 *
 * The GIMP -- an image manipulation program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "intl.h"
#include "message.h"
#include "dic_dialog.h"
#include "persistence.h"
#include "interface.h"
#include "object.h"
#include "diagram_tree.h"
#include "diagram_tree_window.h"
#include "object_ops.h"

#include "dia-app-icons.h"

struct DicDialog {
  Diagram *diagram;
  GtkWidget *dialog;
  GtkWidget *combo;
  GtkTreeView *treeview;
  GtkWidget *frame;
  GtkWidget *name_entry;
  GtkWidget *occurs_spin;
  GtkWidget *length_hbox;
  GtkWidget *length_spin;
};

enum
{
  COLUMN_ICON = 0,
  COLUMN_TREE,
  COLUMN_OCCURS,
  COLUMN_USED,
  COLUMN_NODE,
  NUM_COLS
} ;

enum
{
  ITEM_TYPE_NODE = 0,
  ITEM_TYPE_TEXT,
  ITEM_TYPE_IMAGE
};

#define ICON_NODE GTK_STOCK_DIRECTORY
#define ICON_TEXT GTK_STOCK_BOLD
#define ICON_IMAGE GTK_STOCK_CONVERT

static struct DicDialog *dic_dialog = NULL;

static gboolean 
dnode_prop_name_is_unique(DicNode *node, DicNode *dparent,
                                   const gchar *name)
{
  DicNode *p;
  
  if (dparent == NULL) { return TRUE;}
  
  for (p = DNODE_CHILDREN(dparent); p != NULL; p = DNODE_NEXT(p)) {
    if (p == node) {continue;}
    
    if (g_strcmp0(p->name, name) == 0) {
      return FALSE;
    }
  }

  return TRUE;
}

#define MIN_SUFFIX  1
#define MAX_SUFFIX  65535

static gchar*
dnode_prop_name_unique_dup(DicNode *node, DicNode *dparent,
                                  const gchar *name)
{
  gchar *ret;
  int i;

  for (i = MIN_SUFFIX; i < MAX_SUFFIX; i++) {
    ret = g_strdup_printf("%s-%d",name,i);
    if (dnode_prop_name_is_unique(node, dparent, ret)) {
      return ret;
    }
    g_free(ret);
  }
  fprintf(stderr, "%s isn't unique.\n", name);
  return NULL;
}

static gchar*
dnode_prop_name_unique(DicNode *node,DicNode *parent, gchar *name)
{
  if (dnode_prop_name_is_unique(node, parent, name)) {
    return NULL;
  } 
  return dnode_prop_name_unique_dup(node,parent,name);
}

static int
get_node_item_type(gchar *stock)
{
  if (!strcmp(stock,ICON_NODE)) {
    return ITEM_TYPE_NODE;
  } else if (!strcmp(stock,ICON_TEXT)) {
    return ITEM_TYPE_TEXT;
  } else if (!strcmp(stock,ICON_IMAGE)) {
    return ITEM_TYPE_IMAGE;
  }
  return ITEM_TYPE_NODE;
}

static GtkTreeModel *
create_and_fill_model (void)
{
  GtkTreeStore *treestore;

  treestore = gtk_tree_store_new(NUM_COLS,
                  G_TYPE_STRING, 
                  G_TYPE_STRING,
                  G_TYPE_INT,
                  G_TYPE_STRING,
                  G_TYPE_POINTER);
  return GTK_TREE_MODEL(treestore);
}

static gchar*
treeview_get_used_string(DicNode *node)
{
  return (gchar*)g_strdup_printf("%d/%d",
   dnode_get_n_used_objects(DNODE(node)),
   dnode_get_n_objects(DNODE(node)));
}

static void
treeview_update_used_recursive(GtkTreeModel *model,GtkTreeIter *iter)
{
  GtkTreeIter child;
  DicNode *node;
  gchar *used;
  int i;

  gtk_tree_model_get(model, iter, 
      COLUMN_NODE, &node,
      -1);

  if (node->type == DIC_NODE_TYPE_NODE) {
    for(i=0;i< gtk_tree_model_iter_n_children(model,iter);i++) {
      if (gtk_tree_model_iter_nth_child(model,&child,iter,i)) {
        treeview_update_used_recursive(model,&child);
      }
    }
  } else {
    used = treeview_get_used_string(node);
    gtk_tree_store_set(GTK_TREE_STORE(model), iter,
      COLUMN_USED, used,
      -1);
    g_free(used);
  }
}

void dic_dialog_update_dialog(void)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  model = gtk_tree_view_get_model(dic_dialog->treeview);
  if (gtk_tree_model_get_iter_first(model,&iter)) {
    do {
      treeview_update_used_recursive(model,&iter);
    } while(gtk_tree_model_iter_next(model,&iter));
  }
}

static void
cb_drag_data_get (GtkWidget *widget, GdkDragContext *context,
		    GtkSelectionData *selection_data, guint info,
		    guint32 time, gpointer user_data)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  DicNode *node;

#if 1
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

  if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
    gtk_tree_model_get(model, &iter, 
      COLUMN_NODE, &node,
      -1);
    if (info == 0) {
      gtk_selection_data_set(selection_data, selection_data->target,
      		   8, (guchar *)&node, sizeof(DicNode *));
    }
  }
#endif
}

static gboolean
cb_drag_drop(GtkWidget *widget,
  GdkDragContext *drag_context,
  gint x,
  gint y,
  guint time,
  gpointer data)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreePath *dest_path;
  GtkTreeIter src_iter,dest_iter;
  DicNode *src_node, *dest_node;
  GtkTreeViewDropPosition pos;

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
  if (gtk_tree_selection_get_selected(selection, &model, &src_iter)) {
    gtk_tree_model_get(model, &src_iter, 
      COLUMN_NODE, &src_node,
      -1);
  
    if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
          x,y,&dest_path,&pos)) {
      gtk_tree_model_get_iter(model,&dest_iter,dest_path);
      gtk_tree_model_get(model, &dest_iter, 
        COLUMN_NODE, &dest_node,
        -1);
      if (src_node == dest_node) {
        return TRUE;
      }
      switch(pos) {
      case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
        if (dest_node->type != DIC_NODE_TYPE_NODE) {
          return TRUE;
        }
        if (gtk_tree_store_is_ancestor(GTK_TREE_STORE(model),
          &dest_iter,&src_iter)) {
          return TRUE;
        }
        break;
      case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
        if (dest_node->type != DIC_NODE_TYPE_NODE) {
          return TRUE;
        }
        if (gtk_tree_store_is_ancestor(GTK_TREE_STORE(model),
          &dest_iter,&src_iter)) {
          return TRUE;
        }
        break;
      default:
        break;
      }
    }
  }
  return FALSE;
}

static void
gtk_tree_store_move_recursive(GtkTreeStore *model,
  GtkTreeIter *dest,
  GtkTreeIter *src)
{
  GtkTreeIter src_child,dest_child;
  gchar *icon,*name,*used;
  int occurs;
  DicNode *node;

  while(gtk_tree_model_iter_has_child(GTK_TREE_MODEL(model),src)) {
    gtk_tree_model_iter_children(GTK_TREE_MODEL(model),&src_child,src);
    gtk_tree_store_append(model,&dest_child,dest);
    gtk_tree_store_move_recursive(model,&dest_child,&src_child);
  }
  gtk_tree_model_get(GTK_TREE_MODEL(model), src,
    COLUMN_ICON, &icon,
    COLUMN_TREE, &name,
    COLUMN_OCCURS, &occurs,
    COLUMN_USED, &used,
    COLUMN_NODE, &node,
    -1);
  gtk_tree_store_set(model, dest,
    COLUMN_ICON, icon,
    COLUMN_TREE, name,
    COLUMN_OCCURS, occurs,
    COLUMN_USED, used,
    COLUMN_NODE, node,
    -1);
  gtk_tree_store_remove(model,src);
}

static gboolean
cb_drag_data_received(GtkWidget *widget,
  GdkDragContext *context,
  gint x,
  gint y,
  GtkSelectionData *data,
  guint info,
  guint time,
  gpointer user_data)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreePath *dest_path;
  GtkTreeIter src_iter,dest_iter,child_iter;
  GtkTreeViewDropPosition pos;
  gchar *newname;
  DicNode *src_node, *dest_node, *parent_node = NULL;
  GList *list=NULL;
  int i;
  DiaObject *obj;
  Layer *layer;

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
  if (gtk_tree_selection_get_selected(selection, &model, &src_iter)) {
    if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
          x,y,&dest_path,&pos)) {

      gtk_tree_model_get_iter(model,&dest_iter,dest_path);
      gtk_tree_model_get(model, &dest_iter, 
        COLUMN_NODE, &dest_node,
        -1);

      gtk_tree_model_get(model, &src_iter,
        COLUMN_NODE, &src_node,
        -1);

      switch(pos) {
      case GTK_TREE_VIEW_DROP_BEFORE:
        gtk_tree_store_insert_before(GTK_TREE_STORE(model),
          &child_iter,NULL,&dest_iter);
        parent_node = DNODE_PARENT(dest_node);
        dtree_unlink(src_node);
        dtree_move_before(src_node,DNODE_PARENT(dest_node),dest_node);
        break;
      case GTK_TREE_VIEW_DROP_AFTER:
        gtk_tree_store_insert_after(GTK_TREE_STORE(model),
          &child_iter,NULL,&dest_iter);
        parent_node = DNODE_PARENT(dest_node);
        dtree_move_after(src_node,DNODE_PARENT(dest_node),dest_node);
        break;
      case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
        gtk_tree_store_append(GTK_TREE_STORE(model),&child_iter,&dest_iter);
        parent_node = dest_node;
        dtree_move_before(src_node,dest_node,NULL);
        break;
      case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
        gtk_tree_store_append(GTK_TREE_STORE(model),&child_iter,&dest_iter);
        parent_node = dest_node;
        dtree_move_after(src_node,dest_node,NULL);
        break;
      }

      newname = dnode_prop_name_unique(src_node, parent_node, src_node->name);
      list = dnode_get_objects_recursive(src_node,NULL);
      if (list != NULL) {
        diagram_unselect_objects(dic_dialog->diagram, list);
        for(i=0;i<g_list_length(list);i++){
          obj = (DiaObject*)g_list_nth_data(list,i);
          layer = dia_object_get_parent_layer(obj);
          layer_remove_object(layer, obj);
        }
        object_add_updates_list(list, dic_dialog->diagram);
        diagram_tree_remove_objects(diagram_tree(), list);

        g_list_free(list);
      }
      dnode_reset_objects_recursive(src_node);

      if (newname != NULL) {
        gtk_tree_store_set(GTK_TREE_STORE(model), &src_iter,
          COLUMN_TREE, newname,
          -1);
        g_free(src_node->name);
        src_node->name = newname;
      }

      gtk_tree_store_move_recursive(GTK_TREE_STORE(model), 
        &child_iter, &src_iter);
      treeview_update_used_recursive(model,&child_iter);
    }
  }
  return FALSE;
}

static gboolean
cb_change_button_clicked(GtkWidget *widget,gpointer data)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  DicNode *node;
  gchar *name;
  int occurs, length;

  selection = gtk_tree_view_get_selection(dic_dialog->treeview);
  if (gtk_tree_selection_get_selected(
      selection, &model, &iter)) {
    gtk_tree_model_get(model, &iter, 
      COLUMN_NODE, &node,
      -1);
    /* change name */
    name = (gchar*)gtk_entry_buffer_get_text(
      gtk_entry_get_buffer(GTK_ENTRY(dic_dialog->name_entry)));
    if (strlen(name) > 0 && g_strcmp0(node->name, name)) {
      if (dnode_prop_name_is_unique(node, DNODE_PARENT(node),name)) {
        dnode_update_node_name(node,name);
        gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
          COLUMN_TREE, name,
          -1);
      } else {
        message_error(_("Same name node exist."));
        return FALSE;
      }
    }
    /* change occurs */
    occurs = gtk_spin_button_get_value_as_int(
      GTK_SPIN_BUTTON(dic_dialog->occurs_spin));
    if (node->occurs != occurs && occurs > 0) {
      if (dnode_set_occurs(node,occurs)) {
        gtk_tree_store_set(GTK_TREE_STORE(model), &iter,
          COLUMN_OCCURS, occurs,
          -1);
        treeview_update_used_recursive(model,&iter);
      } else {
        message_error(_("cannot change occurs for existence object."));
        return FALSE;
      }
    }

    /* change length */
    length = gtk_spin_button_get_value_as_int(
      GTK_SPIN_BUTTON(dic_dialog->length_spin));
    if (node->type == DIC_NODE_TYPE_TEXT && 
          node->length != length && 
          length > 0) {
      dnode_text_set_length(node,length);
    }
  }

  return FALSE;
}

static GtkWidget *
create_view_and_model (void)
{
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;
  GtkWidget *view;
  GtkTreeModel *model;
  GtkTreeSelection* selection;
  const GtkTargetEntry row_targets[] = {
    { "DIC_DIALOG_TREEVIEW", GTK_TARGET_SAME_APP, 0 }
  };

  view = gtk_tree_view_new();
  g_object_set(G_OBJECT(view),"enable-tree-lines",TRUE,NULL);
  gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(view),
    GDK_BUTTON1_MASK,
    row_targets,
    G_N_ELEMENTS (row_targets),
    GDK_ACTION_COPY);
  gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(view),
    row_targets,
    G_N_ELEMENTS (row_targets),
    GDK_ACTION_MOVE);
  g_signal_connect(G_OBJECT(view), "drag-data-get",
    G_CALLBACK(cb_drag_data_get),NULL);
  g_signal_connect(G_OBJECT(view),"drag-drop",
    G_CALLBACK(cb_drag_drop),NULL);
  g_signal_connect(G_OBJECT(view),"drag-data-received",
    G_CALLBACK(cb_drag_data_received),NULL);

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
  gtk_tree_selection_set_mode(selection,GTK_SELECTION_SINGLE);

  /* tree */
  col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(col, "TREE");
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_add_attribute(col, renderer, 
    "stock-id",COLUMN_ICON);

  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_add_attribute(col, renderer, 
    "text", COLUMN_TREE);

  /* occurs */
  col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(col, "OCCURS");
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_add_attribute(col, renderer, 
    "text", COLUMN_OCCURS);

  /* used */
  col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(col, "USED");
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_add_attribute(col, renderer, 
    "text", COLUMN_USED);


  model = create_and_fill_model();
  gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
  g_object_unref(model); 

  return view;
}


static gboolean 
cb_delete_node_sub(DicNode *dnode)
{
  DicNode *p;

  if (dnode == NULL) {
    return FALSE;
  }

  if (dnode_data_is_used(dnode)) {
    return FALSE;
  }
  for (p = DNODE_CHILDREN(dnode); p != NULL; p = DNODE_NEXT(p)) {
    if (!cb_delete_node_sub(p)) {
      return FALSE;
    }
  }
  return TRUE;
}

static void
cb_delete_node(GtkToolButton *button,
  GtkTreeSelection *select)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeStore *store;
  DicNode *node;

  if (dic_dialog->diagram == NULL) {
    return;
  }

  if (gtk_tree_selection_get_selected(
      select, &model, &iter)) {
    store = GTK_TREE_STORE(model);
    gtk_tree_model_get(model, &iter, COLUMN_NODE, &node,  -1);
    if (!cb_delete_node_sub(node)) {
      message_error(_("Some node included deleting node are used."));
    } else {
      gtk_tree_store_remove(store,&iter);
      dtree_unlink(node);
    }
  }
}

static void
cb_add_node(GtkToolButton *button,
  GtkTreeSelection *select)
{
  GtkTreeIter iter,new;
  GtkTreeModel *model;
  GtkTreeStore *store;
  gchar *parent_type, *name;
  DicNode *node, *pnode, *snode;

  if (dic_dialog == NULL || dic_dialog->diagram == NULL) {
    return;
  }

  pnode = DIA_DIAGRAM_DATA(dic_dialog->diagram)->dtree;
  snode = NULL;

  if (gtk_tree_selection_get_selected(
      select, &model, &iter)) {
    gtk_tree_model_get(model, &iter, COLUMN_ICON, &parent_type,  -1);
    store = GTK_TREE_STORE(model);
    if (get_node_item_type(parent_type) == ITEM_TYPE_NODE) {
      gtk_tree_model_get(model, &iter, COLUMN_NODE, &pnode,  -1);
      gtk_tree_store_append(store, &new, &iter);
    } else {
      pnode = NULL;
    }
    g_free(parent_type);
  } else {
    store = GTK_TREE_STORE(model);
    gtk_tree_store_append(store, &new, NULL);
  }

  if (pnode == NULL) {
    return;
  }

  name = dnode_prop_name_unique_dup(NULL, pnode, DNODE_DEFAULT_NAME_NODE);
  node = dnode_new(name, DNODE_DEFAULT_NODE_OCCURS,DIC_NODE_TYPE_NODE,
    DNODE_DEFAULT_LENGTH,pnode,snode);

  gtk_tree_store_set(store, &new,
    COLUMN_ICON, ICON_NODE,
    COLUMN_TREE, name,
    COLUMN_OCCURS, DNODE_DEFAULT_NODE_OCCURS,
    COLUMN_USED, "-",
    COLUMN_NODE,node,
    -1);
  g_free(name);
}

static void
cb_add_string(GtkToolButton *button,
  GtkTreeSelection *select)
{
  GtkTreeIter iter,new;
  GtkTreeModel *model;
  GtkTreeStore *store;
  gchar *parent_type, *name, *used;
  DicNode *node, *pnode, *snode;

  if (dic_dialog == NULL || dic_dialog->diagram == NULL) {
    return;
  }

  pnode = DIA_DIAGRAM_DATA(dic_dialog->diagram)->dtree;
  snode = NULL;

  if (gtk_tree_selection_get_selected(
      select, &model, &iter)) {
    gtk_tree_model_get(model, &iter, COLUMN_ICON, &parent_type,  -1);
    store = GTK_TREE_STORE(model);
    if (get_node_item_type(parent_type) == ITEM_TYPE_NODE) {
      gtk_tree_model_get(model, &iter, COLUMN_NODE, &pnode,  -1);
      gtk_tree_store_append(store, &new, &iter);
    } else {
      pnode = NULL;
    }
    g_free(parent_type);
  } else {
    store = GTK_TREE_STORE(model);
    gtk_tree_store_append(store, &new, NULL);
  }

  if (pnode == NULL) {
    return;
  } 

  name = dnode_prop_name_unique_dup(NULL, pnode, DNODE_DEFAULT_NAME_TEXT);
  node = dnode_new(name, DNODE_DEFAULT_OCCURS,DIC_NODE_TYPE_TEXT,
    DNODE_DEFAULT_LENGTH,pnode,snode);

  used = treeview_get_used_string(node);

  gtk_tree_store_set(store, &new,
    COLUMN_ICON, ICON_TEXT,
    COLUMN_TREE, name,
    COLUMN_OCCURS, DNODE_DEFAULT_OCCURS,
    COLUMN_USED, used,
    COLUMN_NODE,node,
    -1);
  g_free(name);
  g_free(used);
}


static void
cb_add_image(GtkToolButton *button,
  GtkTreeSelection *select)
{
  GtkTreeIter iter,new;
  GtkTreeModel *model;
  GtkTreeStore *store;
  gchar *parent_type, *name, *used;
  DicNode *node, *pnode, *snode;

  if (dic_dialog == NULL || dic_dialog->diagram == NULL) {
    return;
  }

  pnode = DIA_DIAGRAM_DATA(dic_dialog->diagram)->dtree;
  snode = NULL;

  if (gtk_tree_selection_get_selected(
      select, &model, &iter)) {
    gtk_tree_model_get(model, &iter, COLUMN_ICON, &parent_type,  -1);
    store = GTK_TREE_STORE(model);
    if (get_node_item_type(parent_type) == ITEM_TYPE_NODE) {
      gtk_tree_model_get(model, &iter, COLUMN_NODE, &pnode,  -1);
      gtk_tree_store_append(store, &new, &iter);
    } else {
      pnode = NULL;
    }
    g_free(parent_type);
  } else {
    store = GTK_TREE_STORE(model);
    gtk_tree_store_append(store, &new, NULL);
  }

  if (pnode == NULL) {
    return ;
  }

  name = dnode_prop_name_unique_dup(NULL, pnode, DNODE_DEFAULT_NAME_IMAGE);
  node = dnode_new(name, DNODE_DEFAULT_OCCURS,DIC_NODE_TYPE_IMAGE,
    DNODE_DEFAULT_LENGTH,pnode,snode);

  used = treeview_get_used_string(node);

  gtk_tree_store_set(store, &new,
    COLUMN_ICON, ICON_IMAGE,
    COLUMN_TREE, name,
    COLUMN_OCCURS, DNODE_DEFAULT_OCCURS,
    COLUMN_USED, used,
    COLUMN_NODE,node,
    -1);
  g_free(name);
  g_free(used);
}

static GtkWidget*
create_toolbar(GtkTreeSelection *select)
{
  GtkWidget *toolbar;
  GtkToolItem *node_button;
  GtkToolItem *str_button;
  GtkToolItem *img_button;
  GtkToolItem *delete_button;

  toolbar = gtk_toolbar_new();

  node_button = gtk_tool_button_new_from_stock(ICON_NODE);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),GTK_TOOL_ITEM(node_button),0);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(node_button),"ADD NODE");
  g_signal_connect(G_OBJECT(node_button),"clicked",
    G_CALLBACK(cb_add_node),select);

  str_button = gtk_tool_button_new_from_stock(ICON_TEXT);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),GTK_TOOL_ITEM(str_button),1);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(str_button),"ADD STRING");
  g_signal_connect(G_OBJECT(str_button),"clicked",
    G_CALLBACK(cb_add_string),select);

  img_button = gtk_tool_button_new_from_stock(ICON_IMAGE);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),GTK_TOOL_ITEM(img_button),2);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(img_button),"ADD IMAGE");
  g_signal_connect(G_OBJECT(img_button),"clicked",
    G_CALLBACK(cb_add_image),select);

  delete_button = gtk_tool_button_new_from_stock(GTK_STOCK_DELETE);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),GTK_TOOL_ITEM(delete_button),3);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(delete_button),"DELETE ITEM");
  g_signal_connect(G_OBJECT(delete_button),"clicked",
    G_CALLBACK(cb_delete_node),select);

  return toolbar;
}

static void
cb_selection_changed(GtkTreeSelection *sel,
  gpointer data)
{
  struct DicDialog *dic_dialog;
  GtkTreeModel *model;
  GtkTreeIter iter;
  DicNode *node;
  gchar *icon;

  if (gtk_tree_selection_get_selected(sel,&model,&iter)) {
    dic_dialog = (struct DicDialog*)data;
    gtk_tree_model_get(model, &iter, 
      COLUMN_ICON, &icon,
      COLUMN_NODE, &node,
      -1);

    gtk_entry_set_text(GTK_ENTRY(dic_dialog->name_entry),node->name);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dic_dialog->occurs_spin),
      (gdouble)node->occurs);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dic_dialog->length_spin),
      (gdouble)node->length);

    switch(get_node_item_type(icon)) {
    case ITEM_TYPE_NODE:
      gtk_frame_set_label(GTK_FRAME(dic_dialog->frame),_("Node"));
      gtk_widget_hide(dic_dialog->length_hbox);
      break;
    case ITEM_TYPE_TEXT:
      gtk_frame_set_label(GTK_FRAME(dic_dialog->frame),_("String"));
      gtk_widget_show(dic_dialog->length_hbox);
      break;
    case ITEM_TYPE_IMAGE:
      gtk_frame_set_label(GTK_FRAME(dic_dialog->frame),_("Image"));
      gtk_widget_hide(dic_dialog->length_hbox);
      break;
    }   
    g_free(icon);
  }
}

static gboolean
dic_dialog_delete(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(widget);
  /* We're caching, so don't destroy */
  return TRUE;
}

static void
dic_dialog_select_diagram_callback(GtkWidget *widget, gpointer gdata)
{
  GList *dia_list;
  Diagram *dia, *selectdia = NULL;
  char *filename;
  char *selectname;

  if (dic_dialog == NULL || dic_dialog->dialog == NULL) {
    if (!dia_open_diagrams())
      return; /* shortcut; maybe session end w/o this dialog */
    else
      create_dic_dialog();
  }

  selectname = gtk_combo_box_get_active_text(GTK_COMBO_BOX(dic_dialog->combo));

  dia_list = dia_open_diagrams();
  while (dia_list != NULL) {
    dia = (Diagram *) dia_list->data;
    filename = strrchr(dia->filename, G_DIR_SEPARATOR);
    if (filename==NULL) {
      filename = dia->filename;
    } else {
      filename++;
    }
    if (filename!=NULL && selectname != NULL && !strcmp(filename,selectname)) {
      selectdia = dia;
    }
    dia_list = g_list_next(dia_list);
  }
  dic_dialog_set_diagram(selectdia);
}

void
create_dic_dialog(void)
{
  GtkWidget *dialog, *vbox;
  GtkWidget *hbox, *label, *combo, *separator;
  GtkWidget *toolbar, *treeview;
  GtkTreeSelection *selection; 
  GtkWidget *frame,*vbox2;
  GtkWidget *name_entry,*occurs_spin,*length_spin;
  GtkWidget *change_button;

  dic_dialog = g_new(struct DicDialog, 1);
  dic_dialog->diagram = NULL;

  /* dialog */
  dialog = gtk_dialog_new();
  dic_dialog->dialog = dialog;

  gtk_window_set_title(GTK_WINDOW(dialog), _("Dictionary"));
  gtk_window_set_role(GTK_WINDOW(dialog), "dictionary_window");
  gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_default_size(GTK_WINDOW(dialog),250,600);

  g_signal_connect (GTK_OBJECT (dialog), "delete_event",
                      G_CALLBACK(dic_dialog_delete), NULL);
  g_signal_connect (GTK_OBJECT (dialog), "destroy",
                      G_CALLBACK(gtk_widget_destroyed), 
		      &(dic_dialog->dialog));

  vbox = GTK_DIALOG(dialog)->vbox;
  
  /* diagram list */
  hbox = gtk_hbox_new(FALSE,1);
  label = gtk_label_new(_("Diagram:"));
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,2);

  dic_dialog->combo = combo = gtk_combo_box_new_text();
  g_signal_connect(G_OBJECT(combo),"changed",
    G_CALLBACK(dic_dialog_select_diagram_callback),NULL);
  gtk_box_pack_start(GTK_BOX(hbox),combo,TRUE,TRUE,2);

  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);

  /* treeview */
  treeview = create_view_and_model();
  dic_dialog->treeview = GTK_TREE_VIEW(treeview);
  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
  g_signal_connect(G_OBJECT(selection),"changed",
    G_CALLBACK(cb_selection_changed),dic_dialog);

  /* toolbar */
  toolbar = create_toolbar(selection);

  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, TRUE, 1);
  gtk_box_pack_start(GTK_BOX(vbox), treeview, TRUE, TRUE, 1);

  /* frame */
  frame = gtk_frame_new("");
  dic_dialog->frame = frame;
  vbox2 = gtk_vbox_new(FALSE,0);
  gtk_container_add(GTK_CONTAINER(frame),vbox2);

  hbox = gtk_hbox_new(FALSE,0);
  label = gtk_label_new(_("Name"));
  gtk_box_pack_start(GTK_BOX(hbox),label,TRUE,FALSE,0);
  name_entry = gtk_entry_new();
  dic_dialog->name_entry = name_entry;
  gtk_box_pack_start(GTK_BOX(hbox),name_entry,TRUE,TRUE,0);
  gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);

  hbox = gtk_hbox_new(FALSE,0);
  label = gtk_label_new(_("Occurs"));
  gtk_box_pack_start(GTK_BOX(hbox),label,TRUE,FALSE,0);
  occurs_spin = gtk_spin_button_new_with_range(1,1000,1);
  dic_dialog->occurs_spin = occurs_spin;
  gtk_box_pack_start(GTK_BOX(hbox),occurs_spin,TRUE,TRUE,0);
  gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);

  dic_dialog->length_hbox = hbox = gtk_hbox_new(FALSE,0);
  label = gtk_label_new(_("Length"));
  gtk_box_pack_start(GTK_BOX(hbox),label,TRUE,FALSE,0);
  length_spin = gtk_spin_button_new_with_range(1,1000,1);
  dic_dialog->length_spin = length_spin;
  gtk_box_pack_start(GTK_BOX(hbox),length_spin,TRUE,TRUE,0);
  gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, TRUE, 0);
  
  separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox2), separator, FALSE, FALSE, 0);

  hbox = gtk_hbox_new(FALSE,0);
  change_button = gtk_button_new_with_label(_("Change"));
  gtk_box_pack_end(GTK_BOX(vbox2), change_button, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(change_button),"clicked",
    G_CALLBACK(cb_change_button_clicked),NULL);

  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, TRUE, 1);

  gtk_widget_show_all(dialog);

  persistence_register_window(GTK_WINDOW(dialog));

  dic_dialog_update_diagram_list();
}

void
dic_dialog_show()
{
  if (is_integrated_ui () == FALSE)
  {   
    if (dic_dialog == NULL || dic_dialog->dialog == NULL)
      create_dic_dialog();
    gtk_window_present(GTK_WINDOW(dic_dialog->dialog));
  }
}

void
dic_dialog_update_diagram_list()
{
  GList *dia_list;
  Diagram *dia;
  char *filename;
  int i;
  int current_nr;
  GtkTreeModel *model;

  if (dic_dialog == NULL || dic_dialog->dialog == NULL) {
    if (!dia_open_diagrams())
      return; /* shortcut; maybe session end w/o this dialog */
    else
      create_dic_dialog();
  }
  current_nr = -1;

  model = gtk_combo_box_get_model(GTK_COMBO_BOX(dic_dialog->combo)); 
  gtk_list_store_clear(GTK_LIST_STORE(model));

  i = 0;
  dia_list = dia_open_diagrams();
  while (dia_list != NULL) {
    dia = (Diagram *) dia_list->data;
    if (dia == dic_dialog->diagram) {
      current_nr = i;
    }
    filename = strrchr(dia->filename, G_DIR_SEPARATOR);
    if (filename==NULL) {
      filename = dia->filename;
    } else {
      filename++;
    }
    gtk_combo_box_append_text(GTK_COMBO_BOX(dic_dialog->combo),filename);
    dia_list = g_list_next(dia_list);
    i++;
  }
  if (current_nr != -1) {
    gtk_combo_box_set_active(GTK_COMBO_BOX(dic_dialog->combo),current_nr);
  }
}

static void
dic_dialog_set_tree_store(GNode *node,GtkTreeIter *iter)
{
  GtkTreeStore *model;
  gchar *icon = ICON_NODE;
  gchar *used = NULL;

  model = GTK_TREE_STORE(gtk_tree_view_get_model(dic_dialog->treeview));
  switch(DNODE(node)->type) {
  case DIC_NODE_TYPE_NODE:
    icon = ICON_NODE;
    used = g_strdup("-");
    break;
  case DIC_NODE_TYPE_TEXT:
    icon = ICON_TEXT;
    used = treeview_get_used_string(DNODE(node));
    break;
  case DIC_NODE_TYPE_IMAGE:
    icon = ICON_IMAGE;
    used = treeview_get_used_string(DNODE(node));
    break;
  default:
    break;
  }
  gtk_tree_store_set(model, iter,
    COLUMN_ICON, icon,
    COLUMN_TREE, DNODE(node)->name,
    COLUMN_OCCURS, DNODE(node)->occurs,
    COLUMN_USED, used,
    COLUMN_NODE, node,
    -1);
  g_free(used);
}

static void
dic_dialog_append_node(GNode *node, GtkTreeIter *iter,GtkTreeIter *parent)
{
  GtkTreeStore *model;

  model = GTK_TREE_STORE(gtk_tree_view_get_model(dic_dialog->treeview));
  gtk_tree_store_append(model,iter,parent);
  dic_dialog_set_tree_store(node,iter);
}

static void
dic_dialog_set_diagram_sub(GNode *node, gpointer data)
{
  GtkTreeStore *model;
  GtkTreeIter iter,*parent;

  parent = (GtkTreeIter*)data;
  model = GTK_TREE_STORE(gtk_tree_view_get_model(dic_dialog->treeview));

  dic_dialog_append_node(node,&iter,parent);

  g_node_children_foreach(node, G_TRAVERSE_ALL, dic_dialog_set_diagram_sub,
                          &iter);
}

void dic_dialog_set_diagram(Diagram *dia)
{
  GtkTreeModel *model;

  model = gtk_tree_view_get_model(dic_dialog->treeview);
  gtk_tree_store_clear(GTK_TREE_STORE(model));

  if (dia == NULL) {
    return;
  }
  dic_dialog->diagram = dia;

  g_node_children_foreach(G_NODE(DIA_DIAGRAM_DATA(dia)->dtree), G_TRAVERSE_ALL, 
    dic_dialog_set_diagram_sub,
    NULL);
  gtk_tree_view_expand_all(dic_dialog->treeview);
}

Diagram *
dic_dialog_get_diagram(void)
{
  if (dic_dialog != NULL) {
    return dic_dialog->diagram;
  }
  return NULL;
}
