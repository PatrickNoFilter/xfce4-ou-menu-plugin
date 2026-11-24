/*
 * OU Menu customization: Opacity sub-page.
 * Controls the background opacity of the Xfce panel and the OU Menu window
 * without fading icons or text.
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

/* Panel background alpha lives in the RGBA colour property. */
gdouble get_panel_background_alpha(XfconfChannel* channel)
{
	gdouble r = 0.0, g = 0.0, b = 0.0, a = 1.0;
	xfconf_channel_get_array(channel, "/panels/panel-1/background-rgba",
		G_TYPE_DOUBLE, &r, G_TYPE_DOUBLE, &g,
		G_TYPE_DOUBLE, &b, G_TYPE_DOUBLE, &a, G_TYPE_INVALID);
	return a;
}

void set_panel_background_alpha(XfconfChannel* channel, gdouble alpha)
{
	gdouble r = 0.0, g = 0.0, b = 0.0, a = 1.0;
	xfconf_channel_get_array(channel, "/panels/panel-1/background-rgba",
		G_TYPE_DOUBLE, &r, G_TYPE_DOUBLE, &g,
		G_TYPE_DOUBLE, &b, G_TYPE_DOUBLE, &a, G_TYPE_INVALID);

	xfconf_channel_set_array(channel, "/panels/panel-1/background-rgba",
		G_TYPE_DOUBLE, r, G_TYPE_DOUBLE, g,
		G_TYPE_DOUBLE, b, G_TYPE_DOUBLE, alpha, G_TYPE_INVALID);

	/* Background colour mode is required for the alpha to be used. */
	xfconf_channel_set_int(channel, "/panels/panel-1/background-style", 1);

	/* Whole-window opacity would fade icons and text, so neutralise it. */
	xfconf_channel_set_int(channel, "/panels/panel-1/enter-opacity", 100);
	xfconf_channel_set_int(channel, "/panels/panel-1/leave-opacity", 100);
}

} // namespace

/* init_opacity_tab:
 * Builds the Opacity page. Both sliders affect only the window background;
 * icons and fonts remain fully opaque.
 *
 * Returns: the page wrapped for the dialog stack.
 */
GtkWidget* SettingsDialog::init_opacity_tab()
{
	XfconfChannel* panel_channel = xfconf_channel_get("xfce4-panel");

	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	GtkWidget* grid = make_two_column_section();
	GtkSizeGroup* labels = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* Panel background opacity */
	GtkWidget* panel_label = gtk_label_new_with_mnemonic("_Panel background opacity:");
	GtkWidget* panel_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
	gtk_scale_set_value_pos(GTK_SCALE(panel_scale), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(panel_scale), get_panel_background_alpha(panel_channel) * 100.0);
	add_form_row(grid, COLUMN_C1, 0, panel_label, panel_scale, true, labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(panel_label), panel_scale);

	connect(panel_scale, "value-changed", [panel_channel](GtkRange* range)
	{
		set_panel_background_alpha(panel_channel, gtk_range_get_value(range) / 100.0);
	});

	/* Menu background opacity */
	GtkWidget* menu_label = gtk_label_new_with_mnemonic("_Menu background opacity:");
	GtkWidget* menu_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
	gtk_scale_set_value_pos(GTK_SCALE(menu_scale), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(menu_scale), m_settings->menu_opacity);
	add_form_row(grid, COLUMN_C1, 1, menu_label, menu_scale, true, labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(menu_label), menu_scale);

	connect(menu_scale, "value-changed", [this](GtkRange* range)
	{
		m_settings->menu_opacity = static_cast<int>(gtk_range_get_value(range));
		m_settings->dispatch_property_change("/menu-opacity");
		refresh_customized_indicator();
	});

	gtk_box_pack_start(page, make_aligned_frame("Opacity", grid), false, false, 0);

	GtkWidget* note = gtk_label_new(
		"Only the panel and menu backgrounds are faded. Icons and text stay fully opaque.");
	gtk_label_set_line_wrap(GTK_LABEL(note), TRUE);
	gtk_widget_set_halign(note, GTK_ALIGN_START);
	gtk_box_pack_start(page, note, false, false, 0);

	g_object_unref(labels);
	return wrap_in_scrolled(GTK_WIDGET(page));
}
