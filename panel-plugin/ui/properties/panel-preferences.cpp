/*
 * OU Menu customization: Panel Preferences tab.
 * Full panel management: add/remove panels plus Display / Appearance / Items
 * sub-tabs backed by the xfce4-panel Xfconf channel.
 * Licensed under GPL-2.0-or-later, same as the upstream MeowMenu/Whisker Menu.
 */

#include "settings-dialog.h"

#include "settings.h"
#include "ui/properties/common.h"
#include "ui/slot.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <xfconf/xfconf.h>
#include <string.h>

using namespace WhiskerMenu;

namespace
{

gchar* panel_property(const gchar* prefix, const gchar* name)
{
	return g_strconcat(prefix, name, nullptr);
}

/* Array helpers: xfconf stores numeric arrays as GValue of G_TYPE_INT. */
GPtrArray* get_int_array(XfconfChannel* channel, const gchar* path)
{
	GPtrArray* source = xfconf_channel_get_arrayv(channel, path);
	GPtrArray* result = g_ptr_array_new();
	if (source)
	{
		for (guint i = 0; i < source->len; ++i)
		{
			GValue* value = static_cast<GValue*>(g_ptr_array_index(source, i));
			if (G_VALUE_HOLDS_INT(value))
			{
				GValue* copy = g_new0(GValue, 1);
				g_value_init(copy, G_TYPE_INT);
				g_value_set_int(copy, g_value_get_int(value));
				g_ptr_array_add(result, copy);
			}
		}
		xfconf_array_free(source);
	}
	return result;
}

void set_int_array(XfconfChannel* channel, const gchar* path, GPtrArray* values)
{
	xfconf_channel_set_arrayv(channel, path, values);
}

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

GtkWidget* add_check(GtkWidget* grid, int row, const gchar* label,
	XfconfChannel* channel, const gchar* key, gboolean fallback, bool sync_struts = false)
{
	GtkWidget* check = gtk_check_button_new_with_mnemonic(label);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
		xfconf_channel_get_bool(channel, key, fallback));
	connect(check, "toggled", [channel, key, sync_struts](GtkToggleButton* button)
	{
		const gboolean value = gtk_toggle_button_get_active(button);
		xfconf_channel_set_bool(channel, key, value);
		if (sync_struts)
		{
			gchar* base = g_strdup(key);
			gchar* cut = g_strrstr(base, "/");
			if (cut)
			{
				*cut = '\0';
				gchar* other = g_strconcat(base,
					g_str_has_suffix(key, "reserve-space") ? "/enable-struts" : "/reserve-space", nullptr);
				xfconf_channel_set_bool(channel, other, value);
				g_free(other);
			}
			g_free(base);
		}
	});
	add_form_row(grid, COLUMN_C1, row, nullptr, check, false, nullptr);
	return check;
}

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

GtkWidget* build_display_tab(XfconfChannel* channel, const gchar* prefix)
{
	GtkWidget* grid = make_two_column_section();
	const gchar* const modes[] = { "Horizontal", "Vertical", nullptr };
	const gchar* const autohide[] = { "Never", "Intelligently", "Always", nullptr };

	gchar* key = panel_property(prefix, "mode");
	add_combo(grid, 0, "_Mode:", channel, key, 0, modes);

	key = panel_property(prefix, "position-locked");
	add_check(grid, 1, "_Lock panel", channel, key, TRUE);

	key = panel_property(prefix, "autohide-behavior");
	add_combo(grid, 2, "Automatically _hide the panel:", channel, key, 0, autohide);

	key = panel_property(prefix, "reserve-space");
	add_check(grid, 3, "_Reserve space on screen edges", channel, key, TRUE, true);

	key = panel_property(prefix, "span-monitors");
	add_check(grid, 4, "Span _monitors", channel, key, FALSE);

	key = panel_property(prefix, "size");
	add_spin(grid, 5, "_Row size (pixels):", channel, key, 20, 128, 33);

	key = panel_property(prefix, "nrows");
	add_spin(grid, 6, "_Number of rows:", channel, key, 1, 3, 1);

	key = panel_property(prefix, "border-width");
	add_spin(grid, 7, "_Border width (pixels):", channel, key, 0, 32, 1);

	key = panel_property(prefix, "length-adjust");
	add_check(grid, 8, "Automatically _increase the length", channel, key, TRUE);

	return wrap_in_scrolled(grid);
}

GtkWidget* build_appearance_tab(XfconfChannel* channel, const gchar* prefix)
{
	GtkWidget* grid = make_two_column_section();
	const gchar* const styles[] = { "None", "Color", "Image", nullptr };

	gchar* key = panel_property(prefix, "background-style");
	add_combo(grid, 0, "_Style:", channel, key, 0, styles);

	key = panel_property(prefix, "background-rgba");
	gdouble r = 0.0, g = 0.0, b = 0.0, a = 1.0;
	xfconf_channel_get_array(channel, key,
		G_TYPE_DOUBLE, &r, G_TYPE_DOUBLE, &g,
		G_TYPE_DOUBLE, &b, G_TYPE_DOUBLE, &a, G_TYPE_INVALID);
	GdkRGBA rgba = { r, g, b, a };
	GtkWidget* color = gtk_color_button_new_with_rgba(&rgba);
	connect(color, "color-set", [channel, key](GtkColorButton* button)
	{
		GdkRGBA value;
		gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button), &value);
		xfconf_channel_set_array(channel, key,
			G_TYPE_DOUBLE, value.red, G_TYPE_DOUBLE, value.green,
			G_TYPE_DOUBLE, value.blue, G_TYPE_DOUBLE, value.alpha, G_TYPE_INVALID);
	});
	add_form_row(grid, COLUMN_C1, 1, gtk_label_new_with_mnemonic("Background _color:"), color, true, nullptr);

	key = panel_property(prefix, "enter-opacity");
	add_spin(grid, 2, "_Enter opacity:", channel, key, 0, 100, 100);

	key = panel_property(prefix, "leave-opacity");
	add_spin(grid, 3, "_Leave opacity:", channel, key, 0, 100, 100);

	key = panel_property(prefix, "icon-size");
	add_spin(grid, 4, "_Icon size:", channel, key, 0, 128, 48);

	key = panel_property(prefix, "dark-mode");
	add_check(grid, 5, "_Dark mode", channel, key, FALSE);

	return wrap_in_scrolled(grid);
}

GtkWidget* build_items_tab(XfconfChannel* channel, const gchar* prefix)
{
	GtkBox* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 12));
	gtk_container_set_border_width(GTK_CONTAINER(box), 6);

	GtkListStore* store = gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING);
	GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1, "Panel item",
		gtk_cell_renderer_text_new(), "text", 1, nullptr);

	gchar* ids_path = panel_property(prefix, "plugin-ids");
	GPtrArray* ids = get_int_array(channel, ids_path);
	for (guint i = 0; i < ids->len; ++i)
	{
		const gint id = g_value_get_int(static_cast<GValue*>(g_ptr_array_index(ids, i)));
		gchar* prop = g_strdup_printf("/plugins/plugin-%d", id);
		gchar* name = xfconf_channel_get_string(channel, prop, nullptr);
		GtkTreeIter iter;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, id, 1, name ? name : "unknown", -1);
		g_free(name);
		g_free(prop);
	}
	xfconf_array_free(ids);

	GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scroll), view);
	gtk_box_pack_start(box, scroll, true, true, 0);

	GtkWidget* buttons = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(buttons), GTK_BUTTONBOX_START);
	gtk_box_set_spacing(GTK_BOX(buttons), 6);

	GtkWidget* add = gtk_button_new_with_label("Add...");
	connect(add, "clicked", [prefix](GtkButton*)
	{
		const gint panel = g_ascii_strtoll(prefix + strlen("/panels/panel-"), nullptr, 10);
		gchar* command = g_strdup_printf("xfce4-panel --add-items=%d", panel);
		g_spawn_command_line_async(command, nullptr);
		g_free(command);
	});
	gtk_container_add(GTK_CONTAINER(buttons), add);

	GtkWidget* configure = gtk_button_new_with_label("Configure...");
	connect(configure, "clicked", [](GtkButton*)
	{
		g_spawn_command_line_async("xfce4-panel --preferences=1", nullptr);
	});
	gtk_container_add(GTK_CONTAINER(buttons), configure);

	GtkWidget* remove = gtk_button_new_with_label("Remove");
	connect(remove, "clicked", [channel, store, view, ids_path](GtkButton*)
	{
		GtkTreeIter iter;
		GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
		if (!gtk_tree_selection_get_selected(selection, nullptr, &iter))
			return;
		gint removed_id = 0;
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 0, &removed_id, -1);
		gtk_list_store_remove(store, &iter);

		GPtrArray* next = get_int_array(channel, ids_path);
		for (guint i = 0; i < next->len;)
		{
			if (g_value_get_int(static_cast<GValue*>(g_ptr_array_index(next, i))) == removed_id)
			{
				g_ptr_array_remove_index(next, i);
				continue;
			}
			++i;
		}
		set_int_array(channel, ids_path, next);
		xfconf_array_free(next);
	});
	gtk_container_add(GTK_CONTAINER(buttons), remove);

	gtk_box_pack_start(box, buttons, false, false, 0);
	g_object_unref(store);
	return GTK_WIDGET(box);
}

GtkWidget* build_panel_notebook(XfconfChannel* channel, const gchar* prefix)
{
	GtkWidget* notebook = gtk_notebook_new();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
		build_display_tab(channel, prefix), gtk_label_new("Display"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
		build_appearance_tab(channel, prefix), gtk_label_new("Appearance"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
		build_items_tab(channel, prefix), gtk_label_new("Items"));
	return notebook;
}

} // namespace

/* init_panel_preferences_tab:
 * Builds the panel management page: panel selector, add/remove buttons and
 * Display / Appearance / Items sub-tabs for the selected panel.
 */
GtkWidget* SettingsDialog::init_panel_preferences_tab()
{
	XfconfChannel* channel = xfconf_channel_get("xfce4-panel");

	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 12));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	GtkWidget* selector_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget* combo = gtk_combo_box_text_new();
	GtkWidget* add_panel = gtk_button_new_with_label("+");
	GtkWidget* remove_panel = gtk_button_new_with_label("-");
	gtk_widget_set_tooltip_text(add_panel, "Add a new panel");
	gtk_widget_set_tooltip_text(remove_panel, "Remove selected panel");
	gtk_box_pack_start(GTK_BOX(selector_box), combo, true, true, 0);
	gtk_box_pack_start(GTK_BOX(selector_box), add_panel, false, false, 0);
	gtk_box_pack_start(GTK_BOX(selector_box), remove_panel, false, false, 0);
	gtk_box_pack_start(page, selector_box, false, false, 0);

	GtkWidget* holder = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(page, holder, true, true, 0);

	GtkWidget* status = gtk_label_new("Panel changes may require restarting the panel (xfce4-panel -r).");
	gtk_label_set_line_wrap(GTK_LABEL(status), TRUE);
	gtk_box_pack_start(page, status, false, false, 0);

	/* Keep the currently selected panel number in widget data. */
	g_object_set_data(G_OBJECT(page), "ou-current-panel", GINT_TO_POINTER(1));

	auto rebuild = [channel, holder, combo, page]()
	{
		GList* children = gtk_container_get_children(GTK_CONTAINER(holder));
		for (GList* item = children; item; item = item->next)
			gtk_widget_destroy(GTK_WIDGET(item->data));
		g_list_free(children);

		const gint panel = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(page), "ou-current-panel"));
		gchar* prefix = g_strdup_printf("/panels/panel-%d/", panel);
		gtk_box_pack_start(GTK_BOX(holder), build_panel_notebook(channel, prefix), true, true, 0);
		gtk_widget_show_all(holder);
		g_free(prefix);
	};

	auto refresh_panels = [channel, combo, page, rebuild]()
	{
		gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(combo))));
		GPtrArray* panels = get_int_array(channel, "/panels");
		gint first = 1;
		for (guint i = 0; i < panels->len; ++i)
		{
			const gint id = g_value_get_int(static_cast<GValue*>(g_ptr_array_index(panels, i)));
			gchar* label = g_strdup_printf("Panel %d", id);
			gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), nullptr, label);
			g_free(label);
			if (i == 0)
				first = id;
		}
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo), panels->len ? 0 : -1);
		g_object_set_data(G_OBJECT(page), "ou-current-panel", GINT_TO_POINTER(first));
		xfconf_array_free(panels);
		rebuild();
	};

	connect(combo, "changed", [combo, page, rebuild](GtkComboBox* box)
	{
		const gint active = gtk_combo_box_get_active(box);
		if (active < 0)
			return;
		gchar* text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
		if (text)
		{
			const gint panel = g_ascii_strtoll(text + strlen("Panel "), nullptr, 10);
			g_object_set_data(G_OBJECT(page), "ou-current-panel", GINT_TO_POINTER(panel));
			g_free(text);
			rebuild();
		}
	});

	connect(add_panel, "clicked", [channel, refresh_panels, status](GtkButton*)
	{
		GPtrArray* panels = get_int_array(channel, "/panels");
		gint next = 1;
		for (guint i = 0; i < panels->len; ++i)
		{
			const gint id = g_value_get_int(static_cast<GValue*>(g_ptr_array_index(panels, i)));
			if (id >= next)
				next = id + 1;
		}
		GValue* value = g_new0(GValue, 1);
		g_value_init(value, G_TYPE_INT);
		g_value_set_int(value, next);
		g_ptr_array_add(panels, value);
		set_int_array(channel, "/panels", panels);
		xfconf_array_free(panels);

		gchar* prefix = g_strdup_printf("/panels/panel-%d/", next);
		gchar* prop = panel_property(prefix, "size");
		xfconf_channel_set_int(channel, prop, 33);
		prop = panel_property(prefix, "nrows");
		xfconf_channel_set_int(channel, prop, 1);
		GPtrArray* empty = g_ptr_array_new();
		prop = panel_property(prefix, "plugin-ids");
		set_int_array(channel, prop, empty);
		xfconf_array_free(empty);
		g_free(prefix);

		gtk_label_set_text(GTK_LABEL(status), "Panel added. Restart the panel to see it.");
		refresh_panels();
	});

	connect(remove_panel, "clicked", [channel, page, refresh_panels, status](GtkButton*)
	{
		const gint panel = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(page), "ou-current-panel"));
		GPtrArray* panels = get_int_array(channel, "/panels");
		for (guint i = 0; i < panels->len;)
		{
			if (g_value_get_int(static_cast<GValue*>(g_ptr_array_index(panels, i))) == panel)
			{
				g_ptr_array_remove_index(panels, i);
				continue;
			}
			++i;
		}
		set_int_array(channel, "/panels", panels);
		xfconf_array_free(panels);

		gchar* prefix = g_strdup_printf("/panels/panel-%d", panel);
		xfconf_channel_reset_property(channel, prefix, TRUE);
		g_free(prefix);

		gtk_label_set_text(GTK_LABEL(status), "Panel removed. Restart the panel to apply.");
		refresh_panels();
	});

	refresh_panels();
	gtk_widget_show_all(GTK_WIDGET(page));
	return GTK_WIDGET(page);
}
