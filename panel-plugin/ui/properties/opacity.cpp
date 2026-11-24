/*
 * OU Menu customization: Opacity sub-page.
 * Panel: background alpha + CSS that removes child widget backgrounds so
 *        tasklist buttons and the workspace switcher fade without fading
 *        icons or text.
 * Menu:  the plugin's background-only menu opacity.
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
		G_TYPE_DOUBLE, &r, G_TYPE_DOUBLE, &g,
		G_TYPE_DOUBLE, &b, G_TYPE_DOUBLE, &alpha, G_TYPE_INVALID);

	/* Background colour mode is required for the alpha to be used. */
	xfconf_channel_set_int(channel, "/panels/panel-1/background-style", 1);
	/* Whole-window opacity would fade icons and text; keep it at full. */
	xfconf_channel_set_int(channel, "/panels/panel-1/enter-opacity", 100);
	xfconf_channel_set_int(channel, "/panels/panel-1/leave-opacity", 100);
}

/* Tasklist and pager run in separate panel wrapper processes, so a CSS provider
 * added from this plugin cannot reach them. Write the rule to the shared GTK3
 * user CSS file instead; a panel restart then reloads those widgets. */
const gchar* const CSS_START = "/* OU Menu opacity start */";
const gchar* const CSS_END = "/* OU Menu opacity end */";

const gchar* const PANEL_CSS =
	"/* OU Menu opacity start */\n"
	".xfce4-panel, .xfce4-panel *, .panel, .panel *, "
	".tasklist, .tasklist *, .group-button, .group-button *, "
	"wnck-pager, wnck-pager:selected, wnck-pager:hover { "
	"background-color: transparent; "
	"background-image: none; "
	"box-shadow: none; "
	"border-color: transparent; }\n"
	"/* OU Menu opacity end */\n";

void update_global_panel_css(gboolean transparent)
{
	gchar* dir = g_build_filename(g_get_user_config_dir(), "gtk-3.0", nullptr);
	g_mkdir_with_parents(dir, 0700);
	gchar* path = g_build_filename(dir, "gtk.css", nullptr);
	g_free(dir);

	gchar* contents = nullptr;
	g_file_get_contents(path, &contents, nullptr, nullptr);
	GString* out = g_string_new(contents ? contents : "");
	g_free(contents);

	/* Strip any previous OU Menu block. */
	gchar* begin = g_strstr_len(out->str, out->len, CSS_START);
	if (begin)
	{
		gchar* end = g_strstr_len(begin, out->len - (begin - out->str), CSS_END);
		if (end)
		{
			end += strlen(CSS_END);
			while (*end == '\n' || *end == '\r')
				++end;
			g_string_erase(out, begin - out->str, end - begin);
		}
	}

	if (transparent)
	{
		if (out->len && out->str[out->len - 1] != '\n')
			g_string_append_c(out, '\n');
		g_string_append(out, PANEL_CSS);
	}

	g_file_set_contents(path, out->str, out->len, nullptr);
	g_string_free(out, TRUE);
	g_free(path);
}

} // namespace

/* init_opacity_tab:
 * Builds the Opacity page. The panel slider fades backgrounds only; the menu
 * slider uses the menu's own background-only opacity setting.
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

	GtkWidget* panel_label = gtk_label_new_with_mnemonic("_Panel background opacity:");
	GtkWidget* panel_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
	gtk_scale_set_value_pos(GTK_SCALE(panel_scale), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(panel_scale), get_panel_background_alpha(panel_channel) * 100.0);
	add_form_row(grid, COLUMN_C1, 0, panel_label, panel_scale, true, labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(panel_label), panel_scale);

	connect(panel_scale, "value-changed", [panel_channel](GtkRange* range)
	{
		const gdouble value = gtk_range_get_value(range);
		set_panel_background_alpha(panel_channel, value / 100.0);
		update_global_panel_css(value < 99.5);
	});

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
		"Only backgrounds are faded. Panel and menu icons, fonts and other "
		"content stay fully opaque.");
	gtk_label_set_line_wrap(GTK_LABEL(note), TRUE);
	gtk_widget_set_halign(note, GTK_ALIGN_START);
	gtk_box_pack_start(page, note, false, false, 0);

	GtkWidget* restart = gtk_button_new_with_label("Restart panel to apply");
	connect(restart, "clicked", [](GtkButton*)
	{
		g_spawn_command_line_async("xfce4-panel -r", nullptr);
	});
	gtk_box_pack_start(page, restart, false, false, 0);

	g_object_unref(labels);
	return wrap_in_scrolled(GTK_WIDGET(page));
}
