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
#include "pos_dialog.h"
#include "persistence.h"
#include "interface.h"
#include "object.h"
#include "diagram_tree.h"
#include "diagram_tree_window.h"
#include "object_ops.h"
#include "prefs.h"

#include "dia-app-icons.h"

struct PosDialog {
  Diagram *dia;
  GtkWidget *dialog;
  GtkWidget *frame;
  GtkWidget *x1;
  GtkWidget *x2;
  GtkWidget *y1;
  GtkWidget *y2;
};

static struct PosDialog *pos_dialog = NULL;

static gboolean
pos_dialog_delete(GtkWidget *widget, gpointer data)
{
  gtk_widget_hide(widget);
  /* We're caching, so don't destroy */
  return TRUE;
}

static void
cb_value_changed(GtkSpinButton *sb,gpointer data)
{
  GList *objs;
  Rectangle bb;
  DiaObject *obj;
  ObjectChange *change;
  Point delta;
  int i;

  if (pos_dialog->dia == NULL) {
    return;
  }
  objs = pos_dialog->dia->data->selected;
  if (objs == NULL) {
    return;
  }
  for(i=0;i<g_list_length(objs);i++) {
    obj = g_list_nth_data(objs,i);
    if (i == 0) {
      bb.left = obj->bounding_box.left;
      bb.top = obj->bounding_box.top;
      bb.right = obj->bounding_box.right;
      bb.bottom = obj->bounding_box.bottom;
    }
    rectangle_union(&bb,&obj->bounding_box);
  }

  delta.x = 0;
  delta.y = 0;

  if (GTK_WIDGET(sb) == pos_dialog->x1) {
    delta.x = gtk_spin_button_get_value(sb) - bb.left;
  } else if (GTK_WIDGET(sb) == pos_dialog->x2) {
    delta.x = gtk_spin_button_get_value(sb) - bb.right;
  } else if (GTK_WIDGET(sb) == pos_dialog->y1) {
    delta.y = gtk_spin_button_get_value(sb) - bb.top;
  } else if (GTK_WIDGET(sb) == pos_dialog->y2) {
    delta.y = gtk_spin_button_get_value(sb) - bb.bottom;
  }
  
  object_add_updates_list(objs, pos_dialog->dia);
  change = object_list_move_delta(objs, &delta);
  object_add_updates_list(objs, pos_dialog->dia);

  pos_dialog_update();
}

void
create_pos_dialog(void)
{
  GtkWidget *dialog,*vbox,*table,*label;

  pos_dialog = g_new(struct PosDialog, 1);
  pos_dialog->dia = NULL;

  /* dialog */
  dialog = gtk_dialog_new();
  pos_dialog->dialog = dialog;

  gtk_window_set_title(GTK_WINDOW(dialog), _("Object Position"));
  gtk_window_set_role(GTK_WINDOW(dialog), "position_window");
  gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_default_size(GTK_WINDOW(dialog),150,150);
  gtk_window_set_type_hint(GTK_WINDOW(dialog),GDK_WINDOW_TYPE_HINT_NORMAL);

  g_signal_connect (GTK_OBJECT (dialog), "delete_event",
                      G_CALLBACK(pos_dialog_delete), NULL);
  g_signal_connect (GTK_OBJECT (dialog), "destroy",
                      G_CALLBACK(gtk_widget_destroyed), 
		      &(pos_dialog->dialog));

  vbox = GTK_DIALOG(dialog)->vbox;

  pos_dialog->x1 = gtk_spin_button_new_with_range(G_MINFLOAT,G_MAXFLOAT,0.01);
  pos_dialog->x2 = gtk_spin_button_new_with_range(G_MINFLOAT,G_MAXFLOAT,0.01);
  pos_dialog->y1 = gtk_spin_button_new_with_range(G_MINFLOAT,G_MAXFLOAT,0.01);
  pos_dialog->y2 = gtk_spin_button_new_with_range(G_MINFLOAT,G_MAXFLOAT,0.01);

  g_signal_connect(G_OBJECT(pos_dialog->x1),"value-changed",
   G_CALLBACK(cb_value_changed),NULL);
  g_signal_connect(G_OBJECT(pos_dialog->x2),"value-changed",
   G_CALLBACK(cb_value_changed),NULL);
  g_signal_connect(G_OBJECT(pos_dialog->y1),"value-changed",
   G_CALLBACK(cb_value_changed),NULL);
  g_signal_connect(G_OBJECT(pos_dialog->y2),"value-changed",
   G_CALLBACK(cb_value_changed),NULL);

  table = gtk_table_new(2,4,FALSE);
  gtk_container_set_border_width(GTK_CONTAINER(table), 5);
  gtk_table_set_row_spacings(GTK_TABLE(table), 5);
  gtk_table_set_col_spacings(GTK_TABLE(table), 5);
  gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, TRUE, 1);

  label = gtk_label_new(_("Left"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
  gtk_table_attach(GTK_TABLE(table), label, 0,1, 0,1,
		   GTK_FILL, GTK_FILL|GTK_EXPAND, 0, 0);
  gtk_table_attach(GTK_TABLE(table), pos_dialog->x1, 1,2, 0,1,
		   GTK_FILL, GTK_FILL|GTK_EXPAND, 0, 0);

  label = gtk_label_new(_("Top"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
  gtk_table_attach(GTK_TABLE(table), label, 0,1, 1,2,
		   GTK_FILL, GTK_FILL|GTK_EXPAND, 0, 0);
  gtk_table_attach(GTK_TABLE(table), pos_dialog->y1, 1,2, 1,2,
		   GTK_FILL, GTK_FILL|GTK_EXPAND, 0, 0);

  label = gtk_label_new(_("Right"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
  gtk_table_attach(GTK_TABLE(table), label, 0,1, 2,3,
		   GTK_FILL, GTK_FILL|GTK_EXPAND, 0, 0);
  gtk_table_attach(GTK_TABLE(table), pos_dialog->x2, 1,2, 2,3,
		   GTK_FILL, GTK_FILL|GTK_EXPAND, 0, 0);

  label = gtk_label_new(_("Bottom"));
  gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
  gtk_table_attach(GTK_TABLE(table), label, 0,1, 3,4,
		   GTK_FILL, GTK_FILL|GTK_EXPAND, 0, 0);
  gtk_table_attach(GTK_TABLE(table), pos_dialog->y2, 1,2, 3,4,
		   GTK_FILL, GTK_FILL|GTK_EXPAND, 0, 0);
  
  gtk_widget_show_all(dialog);
  persistence_register_window(GTK_WINDOW(dialog));
}

void
pos_dialog_show()
{
  if (is_integrated_ui () == FALSE)
  {   
    if (pos_dialog == NULL || pos_dialog->dialog == NULL)
      create_pos_dialog();
    gtk_window_present(GTK_WINDOW(pos_dialog->dialog));
  }
}

static void
cb_selection_changed(Diagram* dia, int n,gpointer data)
{
  if (n == 0) {
    gtk_widget_set_sensitive(pos_dialog->x1,FALSE);
    gtk_widget_set_sensitive(pos_dialog->x2,FALSE);
    gtk_widget_set_sensitive(pos_dialog->y1,FALSE);
    gtk_widget_set_sensitive(pos_dialog->y2,FALSE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(pos_dialog->x1),0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(pos_dialog->x2),0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(pos_dialog->y1),0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(pos_dialog->y2),0);
  } else {
    gtk_widget_set_sensitive(pos_dialog->x1,TRUE);
    gtk_widget_set_sensitive(pos_dialog->x2,TRUE);
    gtk_widget_set_sensitive(pos_dialog->y1,TRUE);
    gtk_widget_set_sensitive(pos_dialog->y2,TRUE);
    pos_dialog_update();
  }
}

void 
pos_dialog_set_diagram(Diagram *dia)
{
  if (pos_dialog == NULL) {
    create_pos_dialog();
  }

  if (pos_dialog->dia != NULL) {
    g_signal_handlers_disconnect_by_func (pos_dialog->dia,
      cb_selection_changed,NULL);
  }
  pos_dialog->dia = dia;
  if (dia != NULL) {
    g_signal_connect (dia,"selection_changed",
      G_CALLBACK(cb_selection_changed),NULL);
  }
}

Diagram *
pos_dialog_get_diagram(void)
{
  if (pos_dialog != NULL) {
    return pos_dialog->dia;
  }
  return NULL;
}

void
pos_dialog_update(void)
{
  GList *objs;
  Rectangle bb;
  DiaObject *obj;
  int i;

  if (pos_dialog == NULL) {
    create_pos_dialog();
  }
  if (pos_dialog->dia == NULL) {
    return;
  }
  objs = pos_dialog->dia->data->selected;
  if (objs == NULL) {
    return;
  }
  for(i=0;i<g_list_length(objs);i++) {
    obj = g_list_nth_data(objs,i);
    if (i == 0) {
      bb.left = obj->bounding_box.left;
      bb.top = obj->bounding_box.top;
      bb.right = obj->bounding_box.right;
      bb.bottom = obj->bounding_box.bottom;
    }
    rectangle_union(&bb,&obj->bounding_box);
  }

  g_signal_handlers_block_by_func(G_OBJECT(pos_dialog->x1),
    cb_value_changed,NULL);
  g_signal_handlers_block_by_func(G_OBJECT(pos_dialog->x2),
    cb_value_changed,NULL);
  g_signal_handlers_block_by_func(G_OBJECT(pos_dialog->y1),
    cb_value_changed,NULL);
  g_signal_handlers_block_by_func(G_OBJECT(pos_dialog->y2),
    cb_value_changed,NULL);

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(pos_dialog->x1),bb.left);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(pos_dialog->x2),bb.right);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(pos_dialog->y1),bb.top);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(pos_dialog->y2),bb.bottom);

  g_signal_handlers_unblock_by_func(G_OBJECT(pos_dialog->x1),
    cb_value_changed,NULL);
  g_signal_handlers_unblock_by_func(G_OBJECT(pos_dialog->x2),
    cb_value_changed,NULL);
  g_signal_handlers_unblock_by_func(G_OBJECT(pos_dialog->y1),
    cb_value_changed,NULL);
  g_signal_handlers_unblock_by_func(G_OBJECT(pos_dialog->y2),
    cb_value_changed,NULL);
}
