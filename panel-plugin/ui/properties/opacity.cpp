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

gint get_panel_opacity(XfconfChannel* channel)
{
	return xfconf_channel_get_int(channel, "/panels/panel-1/enter-opacity", 100);
}

/* The native XFCE panel opacity applies to the whole panel window, including
 * tasklist buttons and the workspace switcher. Those widgets paint their own
 * backgrounds, so background-only alpha cannot fade them. */
void set_panel_opacity(XfconfChannel* channel, gint opacity)
{
	xfconf_channel_set_int(channel, "/panels/panel-1/enter-opacity", opacity);
	xfconf_channel_set_int(channel, "/panels/panel-1/leave-opacity", opacity);
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
	gtk_range_set_value(GTK_RANGE(panel_scale), get_panel_opacity(panel_channel));
	add_form_row(grid, COLUMN_C1, 0, panel_label, panel_scale, true, labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(panel_label), panel_scale);

	connect(panel_scale, "value-changed", [panel_channel](GtkRange* range)
	{
		set_panel_opacity(panel_channel, static_cast<gint>(gtk_range_get_value(range)));
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
		"Panel opacity fades the whole panel, including window buttons and the "
		"workspace switcher. Menu opacity fades only the menu background; icons "
		"and text in the menu stay fully opaque.");
	gtk_label_set_line_wrap(GTK_LABEL(note), TRUE);
	gtk_widget_set_halign(note, GTK_ALIGN_START);
	gtk_box_pack_start(page, note, false, false, 0);

	g_object_unref(labels);
	return wrap_in_scrolled(GTK_WIDGET(page));
}
