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

#include "intl.h"

#include "dic_dialog.h"
#include "persistence.h"
#include "interface.h"

#include "dia-app-icons.h"

struct DicDialog {
  Diagram *diagram;
  GtkWidget *dialog;
  GtkWidget *treeview;
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
  NUM_COLS
} ;

enum
{
  ITEM_TYPE_NODE = 0,
  ITEM_TYPE_STRING,
  ITEM_TYPE_IMAGE
};

#define ICON_NODE GTK_STOCK_DIRECTORY
#define ICON_STRING GTK_STOCK_BOLD
#define ICON_IMAGE GTK_STOCK_CONVERT

static struct DicDialog *dic_dialog = NULL;

static int
get_node_item_type(gchar *stock)
{
  if (!strcmp(stock,ICON_NODE)) {
    return ITEM_TYPE_NODE;
  } else if (!strcmp(stock,ICON_STRING)) {
    return ITEM_TYPE_STRING;
  } else if (!strcmp(stock,ICON_IMAGE)) {
    return ITEM_TYPE_IMAGE;
  }
  return ITEM_TYPE_NODE;
}

static GtkTreeModel *
create_and_fill_model (void)
{
  GtkTreeStore *treestore;
  GtkTreeIter top, child;

  treestore = gtk_tree_store_new(NUM_COLS,
                  G_TYPE_STRING, G_TYPE_STRING,G_TYPE_INT,G_TYPE_STRING);

  gtk_tree_store_append(treestore, &top, NULL);
  gtk_tree_store_set(treestore, &top,
                     COLUMN_ICON, ICON_STRING,
                     COLUMN_TREE, "STRING1",
                     COLUMN_OCCURS, 1,
                     COLUMN_USED, "-",
                     -1);
  gtk_tree_store_append(treestore, &top, NULL);
  gtk_tree_store_set(treestore, &top,
                     COLUMN_ICON, ICON_NODE,
                     COLUMN_TREE, "NODE1",
                     COLUMN_OCCURS, 10,
                     COLUMN_USED, "-",
                     -1);
  gtk_tree_store_append(treestore, &child, &top);
  gtk_tree_store_set(treestore, &child,
                     COLUMN_ICON, ICON_STRING,
                     COLUMN_TREE, "STRING2",
                     COLUMN_OCCURS, 10,
                     COLUMN_USED, "0/10",
                     -1);
  gtk_tree_store_append(treestore, &child, &top);
  gtk_tree_store_set(treestore, &child,
                     COLUMN_ICON, ICON_IMAGE,
                     COLUMN_TREE, "IMAGE1",
                     COLUMN_OCCURS, 10,
                     COLUMN_USED, "0/10",
                     -1);

  return GTK_TREE_MODEL(treestore);
}

static gboolean
cb_drag_drop(GtkWidget *widget,
  GdkDragContext *drag_context,
  gint x,
  gint y,
  guint time,
  gpointer data)
{
  GtkTreeModel *model;
  GtkTreePath *dest;
  GtkTreeIter iter;
  GtkTreeViewDropPosition pos;
  gchar *value;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
  if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
        x,y,&dest,&pos)) {
    if (pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE ||
        pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER) {
      if (gtk_tree_model_get_iter(model,&iter,dest)) {
        gtk_tree_model_get(model, &iter, COLUMN_ICON, &value,-1);
        if (value != NULL && get_node_item_type(value) != ITEM_TYPE_NODE) {
          g_free(value);
          return TRUE;
        }
      }
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

  view = gtk_tree_view_new();
  g_object_set(G_OBJECT(view),"enable-tree-lines",TRUE,NULL);
  gtk_tree_view_set_reorderable(GTK_TREE_VIEW(view),TRUE);
  g_signal_connect(G_OBJECT(view),"drag-drop",
    G_CALLBACK(cb_drag_drop),NULL);

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

static void
cb_delete_button(GtkToolButton *button,
  GtkTreeSelection *select)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeStore *store;

  if (gtk_tree_selection_get_selected(
      select, &model, &iter)) {
    store = GTK_TREE_STORE(model);
    gtk_tree_store_remove(store,&iter);
  }
}

static void
cb_add_button(GtkToolButton *button,
  GtkTreeSelection *select)
{
  GtkTreeIter iter,new;
  GtkTreeModel *model;
  GtkTreeStore *store;
  gchar *stock;
  gchar *parent_type;

  if (gtk_tree_selection_get_selected(
      select, &model, &iter)) {
    gtk_tree_model_get(model, &iter, COLUMN_ICON, &parent_type,  -1);
    store = GTK_TREE_STORE(model);
    if (get_node_item_type(parent_type) == ITEM_TYPE_NODE) {
      gtk_tree_store_insert_after(store, &new, &iter, NULL);
    } else {
      gtk_tree_store_insert_after(store, &new, NULL, &iter);
    }
    g_free(parent_type);
  } else {
    store = GTK_TREE_STORE(model);
    gtk_tree_store_insert_after(store, &new, NULL, NULL);
  }

  stock = (gchar*)gtk_tool_button_get_stock_id(button);

  switch(get_node_item_type(stock)) {
  case ITEM_TYPE_NODE:
    gtk_tree_store_set(store, &new,
      COLUMN_ICON, ICON_NODE,
      COLUMN_TREE, "NODE",
      COLUMN_OCCURS, 10,
      COLUMN_USED, "-",
      -1);
    break;
  case ITEM_TYPE_STRING:
    gtk_tree_store_set(store, &new,
      COLUMN_ICON, ICON_STRING,
      COLUMN_TREE, "STRING",
      COLUMN_OCCURS, 10,
      COLUMN_USED, "-",
      -1);
    break;
  case ITEM_TYPE_IMAGE:
    gtk_tree_store_set(store, &new,
      COLUMN_ICON, ICON_IMAGE,
      COLUMN_TREE, "IMAGE",
      COLUMN_OCCURS, 10,
      COLUMN_USED, "-",
      -1);
    break;
  }
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
    G_CALLBACK(cb_add_button),select);

  str_button = gtk_tool_button_new_from_stock(ICON_STRING);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),GTK_TOOL_ITEM(str_button),1);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(str_button),"ADD STRING");
  g_signal_connect(G_OBJECT(str_button),"clicked",
    G_CALLBACK(cb_add_button),select);

  img_button = gtk_tool_button_new_from_stock(ICON_IMAGE);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),GTK_TOOL_ITEM(img_button),2);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(img_button),"ADD IMAGE");
  g_signal_connect(G_OBJECT(img_button),"clicked",
    G_CALLBACK(cb_add_button),select);

  delete_button = gtk_tool_button_new_from_stock(GTK_STOCK_DELETE);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar),GTK_TOOL_ITEM(delete_button),3);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(delete_button),"DELETE ITEM");
  g_signal_connect(G_OBJECT(delete_button),"clicked",
    G_CALLBACK(cb_delete_button),select);

  return toolbar;
}

static void
cb_selection_changed(GtkTreeSelection *sel,
  gpointer data)
{
  struct DicDialog *dic_dialog;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *icon;
  gchar *tree;
  int occurs;
  gchar *length;

  if (gtk_tree_selection_get_selected(sel,&model,&iter)) {
    dic_dialog = (struct DicDialog*)data;
    gtk_tree_model_get(model, &iter, COLUMN_ICON, &icon,  -1);
    gtk_tree_model_get(model, &iter, COLUMN_TREE, &tree,  -1);
    gtk_tree_model_get(model, &iter, COLUMN_OCCURS, &occurs,  -1);
#if 0
    gtk_tree_model_get(model, &iter, COLUMN_LENGTH, &length,  -1);
#else
    length = g_strdup("10");
#endif

    gtk_entry_set_text(GTK_ENTRY(dic_dialog->name_entry),tree);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dic_dialog->occurs_spin),
      (gdouble)occurs);

    if (length != NULL) {
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(dic_dialog->length_spin),
        (gdouble)atof(length));
    }

    switch(get_node_item_type(icon)) {
    case ITEM_TYPE_NODE:
      gtk_frame_set_label(GTK_FRAME(dic_dialog->frame),_("Node"));
      gtk_widget_hide(dic_dialog->length_hbox);
      break;
    case ITEM_TYPE_STRING:
      gtk_frame_set_label(GTK_FRAME(dic_dialog->frame),_("String"));
      gtk_widget_show(dic_dialog->length_hbox);
      break;
    case ITEM_TYPE_IMAGE:
      gtk_frame_set_label(GTK_FRAME(dic_dialog->frame),_("Image"));
      gtk_widget_hide(dic_dialog->length_hbox);
      break;
    }   
    
    g_free(icon);
    g_free(tree);
    g_free(length);
  }
}

static gboolean
dic_dialog_delete(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(widget);
  /* We're caching, so don't destroy */
  return TRUE;
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

  combo = gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo),"diagram1");
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo),"diagram2");
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo),"diagram3");
  gtk_box_pack_start(GTK_BOX(hbox),combo,TRUE,TRUE,2);

  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

  separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);

  /* treeview */
  treeview = create_view_and_model();
  dic_dialog->treeview = treeview;
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
  /*not implement*/
}

void dic_dialog_set_diagram(Diagram *dia)
{
}
