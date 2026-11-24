/*
 * OU Menu customization: Panel Preferences tab.
 * Provides Display / Appearance / Items sub-tabs backed by the xfce4-panel
 * Xfconf channel, mirroring the native Panel Preferences dialog.
 * Licensed under GPL-2.0-or-later, same as the upstream MeowMenu/Whisker Menu.
 */

#include "settings-dialog.h"

#include "settings.h"
#include "ui/properties/common.h"
#include "ui/slot.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <xfconf/xfconf.h>

using namespace WhiskerMenu;

namespace
{

const gchar* const PANEL_PREFIX = "/panels/panel-1/";

gchar* panel_property(const gchar* name)
{
	return g_strconcat(PANEL_PREFIX, name, nullptr);
}

/* add_spin:
 * Adds a labelled integer spin button bound to a panel xfconf property.
 */
GtkWidget* add_spin(GtkWidget* grid, int row, const gchar* label,
	XfconfChannel* channel, const gchar* key, int min, int max, int fallback)
{
	GtkWidget* spin = gtk_spin_button_new_with_range(min, max, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin),
		xfconf_channel_get_int(channel, key, fallback));
	connect(spin, "value-changed", [channel, key](GtkSpinButton* button)
	{
		xfconf_channel_set_int(channel, key, gtk_spin_button_get_value_as_int(button));
	});
	add_form_row(grid, COLUMN_C1, row, gtk_label_new_with_mnemonic(label), spin, true, nullptr);
	return spin;
}

/* add_check:
 * Adds a check button bound to a boolean panel xfconf property.
 */
GtkWidget* add_check(GtkWidget* grid, int row, const gchar* label,
	XfconfChannel* channel, const gchar* key, gboolean fallback)
{
	GtkWidget* check = gtk_check_button_new_with_mnemonic(label);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
		xfconf_channel_get_bool(channel, key, fallback));
	connect(check, "toggled", [channel, key](GtkToggleButton* button)
	{
		const gboolean value = gtk_toggle_button_get_active(button);
		xfconf_channel_set_bool(channel, key, value);
		/* XFCE 4.20 renamed reserve-space to enable-struts; keep both in sync. */
		if (g_str_has_suffix(key, "reserve-space") ||
			g_str_has_suffix(key, "enable-struts"))
		{
			gchar* other = g_strconcat(PANEL_PREFIX,
				g_str_has_suffix(key, "reserve-space") ? "enable-struts" : "reserve-space", nullptr);
			xfconf_channel_set_bool(channel, other, value);
			g_free(other);
		}
	});
	add_form_row(grid, COLUMN_C1, row, nullptr, check, false, nullptr);
	return check;
}

/* add_combo:
 * Adds a labelled combo bound to an integer panel xfconf property.
 * Labels are indexed by their position in the NULL-terminated array.
 */
GtkWidget* add_combo(GtkWidget* grid, int row, const gchar* label,
	XfconfChannel* channel, const gchar* key, int fallback,
	const gchar* const* labels)
{
	GtkWidget* combo = gtk_combo_box_text_new();
	for (int i = 0; labels[i]; ++i)
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), nullptr, labels[i]);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo),
		xfconf_channel_get_int(channel, key, fallback));
	connect(combo, "changed", [channel, key](GtkComboBox* box)
	{
		xfconf_channel_set_int(channel, key, gtk_combo_box_get_active(box));
	});
	add_form_row(grid, COLUMN_C1, row, gtk_label_new_with_mnemonic(label), combo, true, nullptr);
	return combo;
}

GtkWidget* build_display_tab(XfconfChannel* channel)
{
	GtkWidget* grid = make_two_column_section();
	const gchar* const modes[] = { "Horizontal", "Vertical", nullptr };
	const gchar* const autohide[] = { "Never", "Intelligently", "Always", nullptr };

	gchar* key = panel_property("mode");
	add_combo(grid, 0, "_Mode:", channel, key, 0, modes);

	key = panel_property("position-locked");
	add_check(grid, 1, "_Lock panel", channel, key, TRUE);

	key = panel_property("autohide-behavior");
	add_combo(grid, 2, "Automatically _hide the panel:", channel, key, 0, autohide);

	key = panel_property("reserve-space");
	add_check(grid, 3, "_Reserve space on screen edges", channel, key, TRUE);

	key = panel_property("span-monitors");
	add_check(grid, 4, "Span _monitors", channel, key, FALSE);

	key = panel_property("size");
	add_spin(grid, 5, "_Row size (pixels):", channel, key, 20, 128, 33);

	key = panel_property("nrows");
	add_spin(grid, 6, "_Number of rows:", channel, key, 1, 3, 1);

	key = panel_property("border-width");
	add_spin(grid, 7, "_Border width (pixels):", channel, key, 0, 32, 1);

	key = panel_property("length-adjust");
	add_check(grid, 8, "Automatically _increase the length", channel, key, TRUE);

	return wrap_in_scrolled(grid);
}

GtkWidget* build_appearance_tab(XfconfChannel* channel)
{
	GtkWidget* grid = make_two_column_section();
	const gchar* const styles[] = { "None", "Color", "Image", nullptr };

	gchar* key = panel_property("background-style");
	add_combo(grid, 0, "_Style:", channel, key, 0, styles);

	/* Background colour */
	key = panel_property("background-rgba");
	gdouble r = 0.0, g = 0.0, b = 0.0, a = 1.0;
	xfconf_channel_get_array(channel, key,
		G_TYPE_DOUBLE, &r, G_TYPE_DOUBLE, &g,
		G_TYPE_DOUBLE, &b, G_TYPE_DOUBLE, &a, G_TYPE_INVALID);
	GdkRGBA rgba = { r, g, b, a };
	GtkWidget* color = gtk_color_button_new_with_rgba(&rgba);
	connect(color, "color-set", [channel, key](GtkColorButton* button)
	{
		GdkRGBA value;
		gtk_color_button_get_rgba(button, &value);
		xfconf_channel_set_array(channel, key,
			G_TYPE_DOUBLE, value.red, G_TYPE_DOUBLE, value.green,
			G_TYPE_DOUBLE, value.blue, G_TYPE_DOUBLE, value.alpha, G_TYPE_INVALID);
	});
	add_form_row(grid, COLUMN_C1, 1, gtk_label_new_with_mnemonic("Background _color:"), color, true, nullptr);

	key = panel_property("enter-opacity");
	add_spin(grid, 2, "_Enter opacity:", channel, key, 0, 100, 100);

	key = panel_property("leave-opacity");
	add_spin(grid, 3, "_Leave opacity:", channel, key, 0, 100, 100);

	key = panel_property("icon-size");
	add_spin(grid, 4, "_Icon size:", channel, key, 0, 128, 48);

	key = panel_property("dark-mode");
	add_check(grid, 5, "_Dark mode", channel, key, FALSE);

	return wrap_in_scrolled(grid);
}

GtkWidget* build_items_tab(XfconfChannel* channel)
{
	GtkBox* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 12));
	gtk_container_set_border_width(GTK_CONTAINER(box), 6);

	GtkListStore* store = gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING);
	GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, "Panel item",
		renderer, "text", 1, nullptr);

	GPtrArray* ids = xfconf_channel_get_arrayv(channel, "/panels/panel-1/plugin-ids");
	if (ids)
	{
		for (guint i = 0; i < ids->len; ++i)
		{
			GValue* value = static_cast<GValue*>(g_ptr_array_index(ids, i));
			if (!G_VALUE_HOLDS_INT(value))
				continue;
			const gint id = g_value_get_int(value);
			gchar* prop = g_strdup_printf("/plugins/plugin-%d", id);
			gchar* name = xfconf_channel_get_string(channel, prop, nullptr);
			g_free(prop);
			GtkTreeIter iter;
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, 0, id, 1, name ? name : "unknown", -1);
			g_free(name);
		}
		xfconf_array_free(ids);
	}

	GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scroll), view);
	gtk_box_pack_start(box, scroll, true, true, 0);

	GtkWidget* buttons = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(buttons), GTK_BUTTONBOX_START);
	gtk_box_set_spacing(GTK_BOX(buttons), 6);

	GtkWidget* add = gtk_button_new_with_label("Add...");
	connect(add, "clicked", [](GtkButton*)
	{
		g_spawn_command_line_async("xfce4-panel --add-items=1", nullptr);
	});
	gtk_container_add(GTK_CONTAINER(buttons), add);

	GtkWidget* configure = gtk_button_new_with_label("Configure...");
	connect(configure, "clicked", [](GtkButton*)
	{
		g_spawn_command_line_async("xfce4-panel --preferences=1", nullptr);
	});
	gtk_container_add(GTK_CONTAINER(buttons), configure);

	GtkWidget* remove = gtk_button_new_with_label("Remove");
	connect(remove, "clicked", [channel, store, view](GtkButton*)
	{
		GtkTreeIter iter;
		GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
		if (!gtk_tree_selection_get_selected(selection, nullptr, &iter))
			return;
		gint removed_id = 0;
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, &removed_id, -1);
		gtk_list_store_remove(store, &iter);

		GPtrArray* array = g_ptr_array_new();
		GPtrArray* current = xfconf_channel_get_arrayv(channel, "/panels/panel-1/plugin-ids");
		if (current)
		{
			for (guint i = 0; i < current->len; ++i)
			{
				GValue* old = static_cast<GValue*>(g_ptr_array_index(current, i));
				if (!G_VALUE_HOLDS_INT(old) || g_value_get_int(old) == removed_id)
					continue;
				GValue* copy = g_new0(GValue, 1);
				g_value_init(copy, G_TYPE_INT);
				g_value_set_int(copy, g_value_get_int(old));
				g_ptr_array_add(array, copy);
			}
			xfconf_array_free(current);
		}
		xfconf_channel_set_arrayv(channel, "/panels/panel-1/plugin-ids", array);
		xfconf_array_free(array);
	});
	gtk_container_add(GTK_CONTAINER(buttons), remove);

	gtk_box_pack_start(box, buttons, false, false, 0);
	g_object_unref(store);
	return GTK_WIDGET(box);
}

} // namespace

/* init_panel_preferences_tab:
 *
 * Builds the "Panel Preferences" page with Display / Appearance / Items
 * sub-tabs, equivalent to the native Xfce Panel Preferences dialog.
 *
 * Returns: the page wrapped for the dialog stack.
 */
GtkWidget* SettingsDialog::init_panel_preferences_tab()
{
	XfconfChannel* channel = m_settings->channel;

	GtkWidget* notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
		build_display_tab(channel), gtk_label_new("Display"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
		build_appearance_tab(channel), gtk_label_new("Appearance"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
		build_items_tab(channel), gtk_label_new("Items"));

	return notebook;
}
