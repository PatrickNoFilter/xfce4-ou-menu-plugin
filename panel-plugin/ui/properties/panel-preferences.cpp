/*
 * OU Menu customization: Panel Preferences tab.
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

} // namespace

/* init_panel_preferences_tab:
 *
 * Builds a compact panel-settings page backed by the xfce4-panel Xfconf
 * channel. The full native dialog is still available through the button.
 *
 * Returns: the page wrapped for the dialog stack.
 */
GtkWidget* SettingsDialog::init_panel_preferences_tab()
{
	XfconfChannel* channel = m_settings->channel;

	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	GtkWidget* grid = make_two_column_section();
	GtkSizeGroup* labels = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* Row size */
	gchar* prop = panel_property("size");
	GtkWidget* row_size = gtk_spin_button_new_with_range(20, 128, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(row_size),
		xfconf_channel_get_int(channel, prop, 33));
	connect(row_size, "value-changed", [channel, prop](GtkSpinButton* spin)
	{
		xfconf_channel_set_int(channel, prop, gtk_spin_button_get_value_as_int(spin));
	});
	add_form_row(grid, COLUMN_C1, 0,
		gtk_label_new_with_mnemonic("_Row size (pixels):"), row_size, true, labels);

	/* Number of rows */
	prop = panel_property("nrows");
	GtkWidget* rows = gtk_spin_button_new_with_range(1, 3, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(rows),
		xfconf_channel_get_int(channel, prop, 1));
	connect(rows, "value-changed", [channel, prop](GtkSpinButton* spin)
	{
		xfconf_channel_set_int(channel, prop, gtk_spin_button_get_value_as_int(spin));
	});
	add_form_row(grid, COLUMN_C1, 1,
		gtk_label_new_with_mnemonic("_Number of rows:"), rows, true, labels);

	/* Border width */
	prop = panel_property("border-width");
	GtkWidget* border = gtk_spin_button_new_with_range(0, 32, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(border),
		xfconf_channel_get_int(channel, prop, 1));
	connect(border, "value-changed", [channel, prop](GtkSpinButton* spin)
	{
		xfconf_channel_set_int(channel, prop, gtk_spin_button_get_value_as_int(spin));
	});
	add_form_row(grid, COLUMN_C1, 2,
		gtk_label_new_with_mnemonic("_Border width (pixels):"), border, true, labels);

	/* Lock panel */
	prop = panel_property("position-locked");
	GtkWidget* locked = gtk_check_button_new_with_mnemonic("_Lock panel");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(locked),
		xfconf_channel_get_bool(channel, prop, TRUE));
	connect(locked, "toggled", [channel, prop](GtkToggleButton* button)
	{
		xfconf_channel_set_bool(channel, prop, gtk_toggle_button_get_active(button));
	});
	add_form_row(grid, COLUMN_C1, 3, nullptr, locked, false, nullptr);

	/* Reserve space */
	prop = panel_property("reserve-space");
	GtkWidget* reserve = gtk_check_button_new_with_mnemonic("_Reserve space on screen edges");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reserve),
		xfconf_channel_get_bool(channel, prop, TRUE));
	connect(reserve, "toggled", [channel, prop](GtkToggleButton* button)
	{
		xfconf_channel_set_bool(channel, prop, gtk_toggle_button_get_active(button));
	});
	add_form_row(grid, COLUMN_C1, 4, nullptr, reserve, false, nullptr);

	/* Automatically hide the panel */
	prop = panel_property("autohide-behavior");
	GtkWidget* autohide = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(autohide), "0", "Never");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(autohide), "1", "Intelligently");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(autohide), "2", "Always");
	gtk_combo_box_set_active(GTK_COMBO_BOX(autohide),
		xfconf_channel_get_int(channel, prop, 0));
	connect(autohide, "changed", [channel, prop](GtkComboBox* combo)
	{
		xfconf_channel_set_int(channel, prop, gtk_combo_box_get_active(combo));
	});
	add_form_row(grid, COLUMN_C1, 5,
		gtk_label_new_with_mnemonic("Automatically _hide the panel:"), autohide, true, labels);

	gtk_box_pack_start(page, make_aligned_frame("Panel Preferences", grid), false, false, 0);

	GtkWidget* open = gtk_button_new_with_label("Open Full Panel Preferences...");
	connect(open, "clicked", [](GtkButton*)
	{
		g_spawn_command_line_async("xfce4-panel --preferences=1", nullptr);
	});
	gtk_box_pack_start(page, open, false, false, 0);

	g_object_unref(labels);
	return wrap_in_scrolled(GTK_WIDGET(page));
}
