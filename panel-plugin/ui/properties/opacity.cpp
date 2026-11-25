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
#include <gio/gio.h>

using namespace WhiskerMenu;

namespace
{

gdouble get_panel_background_alpha(XfconfChannel* channel)
{
	gdouble r = 0.101961, g = 0.372549, b = 0.705882, a = 1.0;
	xfconf_channel_get_array(channel, "/panels/panel-1/background-rgba",
		G_TYPE_DOUBLE, &r, G_TYPE_DOUBLE, &g,
		G_TYPE_DOUBLE, &b, G_TYPE_DOUBLE, &a, G_TYPE_INVALID);
	return a;
}

void set_panel_background_alpha(XfconfChannel* channel, gdouble alpha)
{
	gdouble r = 0.101961, g = 0.372549, b = 0.705882, a = 1.0;
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
	"/* Do not style the panel window itself: its xfconf alpha is the opacity. */\n"
	".xfce4-panel *, .panel *, "
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

const gchar* const MENU_CSS_START = "/* OU Menu desktop opacity start */";
const gchar* const MENU_CSS_END = "/* OU Menu desktop opacity end */";

/* Desktop right-click menus are GTK popups from xfdesktop, so their background
 * alpha is controlled through the shared GTK3 CSS file as well. */
void update_desktop_menu_css(gboolean enabled, gint opacity)
{
	gchar* dir = g_build_filename(g_get_user_config_dir(), "gtk-3.0", nullptr);
	g_mkdir_with_parents(dir, 0700);
	gchar* path = g_build_filename(dir, "gtk.css", nullptr);
	g_free(dir);

	gchar* contents = nullptr;
	g_file_get_contents(path, &contents, nullptr, nullptr);
	GString* out = g_string_new(contents ? contents : "");
	g_free(contents);

	gchar* begin = g_strstr_len(out->str, out->len, MENU_CSS_START);
	if (begin)
	{
		gchar* end = g_strstr_len(begin, out->len - (begin - out->str), MENU_CSS_END);
		if (end)
		{
			end += strlen(MENU_CSS_END);
			while (*end == '\n' || *end == '\r')
				++end;
			g_string_erase(out, begin - out->str, end - begin);
		}
	}

	if (enabled)
	{
		gchar alpha[16];
		g_snprintf(alpha, sizeof(alpha), "%d.%02d", opacity / 100, opacity % 100);
		gchar* rule = g_strdup_printf(
			"%s\nmenu, .menu, GtkMenu, .xfce4-panel menu { "
			"background-color: rgba(0, 0, 0, %s); }\n%s\n",
			MENU_CSS_START, alpha, MENU_CSS_END);
		if (out->len && out->str[out->len - 1] != '\n')
			g_string_append_c(out, '\n');
		g_string_append(out, rule);
		g_free(rule);
	}

	g_file_set_contents(path, out->str, out->len, nullptr);
	g_string_free(out, TRUE);
	g_free(path);
}

void set_night_mode(gboolean enabled)
{
	const gchar* theme = enabled ? "Adwaita-dark" : "Adwaita";
	gchar* command = g_strdup_printf(
		"gsettings set org.gnome.desktop.interface gtk-theme %s", theme);
	g_spawn_command_line_async(command, nullptr);
	g_free(command);

	command = g_strdup_printf(
		"xfconf-query -c xsettings -p /Net/ThemeName -s %s --create", theme);
	g_spawn_command_line_async(command, nullptr);
	g_free(command);
}

/* Desktop icon labels are drawn by xfdesktop from its own RGBA xfconf
 * properties, so their alpha must be set there rather than through CSS. */
void set_desktop_icon_label_alpha(gdouble alpha)
{
	XfconfChannel* desktop = xfconf_channel_get("xfce4-desktop");
	const gchar* properties[] = {
		"/desktop-icons/label-background-color",
		"/desktop-icons/label-text-color",
		nullptr
	};

	for (int i = 0; properties[i]; ++i)
	{
		gdouble r = 0.0, g = 0.0, b = 0.0, a = 1.0;
		xfconf_channel_get_array(desktop, properties[i],
			G_TYPE_DOUBLE, &r, G_TYPE_DOUBLE, &g,
			G_TYPE_DOUBLE, &b, G_TYPE_DOUBLE, &a, G_TYPE_INVALID);
		xfconf_channel_set_array(desktop, properties[i],
			G_TYPE_DOUBLE, &r, G_TYPE_DOUBLE, &g,
			G_TYPE_DOUBLE, &b, G_TYPE_DOUBLE, &alpha, G_TYPE_INVALID);
	}

	/* xfdesktop only honours its custom label colours when these flags are set. */
	xfconf_channel_set_bool(desktop,
		"/desktop-icons/custom-label-text-color", alpha < 0.995);
	xfconf_channel_set_bool(desktop,
		"/desktop-icons/custom-label-background-color", alpha < 0.995);
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

	/* System-wide Day / Night mode choice. */
	GSettings* settings = g_settings_new("org.gnome.desktop.interface");
	gchar* theme = g_settings_get_string(settings, "gtk-theme");
	const gboolean dark = theme && g_strstr_len(theme, -1, "dark");
	g_free(theme);
	g_object_unref(settings);

	GtkWidget* day = gtk_radio_button_new_with_mnemonic(nullptr, "_Day mode");
	GtkWidget* night = gtk_radio_button_new_with_mnemonic_from_widget(
		GTK_RADIO_BUTTON(day), "_Night mode");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dark ? night : day), TRUE);

	GtkWidget* mode_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start(GTK_BOX(mode_box), day, false, false, 0);
	gtk_box_pack_start(GTK_BOX(mode_box), night, false, false, 0);
	GtkWidget* mode_label = gtk_label_new_with_mnemonic("_Color mode:");
	add_form_row(grid, COLUMN_C1, 2, mode_label, mode_box, true, labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(mode_label), day);

	connect(day, "toggled", [](GtkToggleButton* button)
	{
		if (gtk_toggle_button_get_active(button))
			set_night_mode(FALSE);
	});
	connect(night, "toggled", [](GtkToggleButton* button)
	{
		if (gtk_toggle_button_get_active(button))
			set_night_mode(TRUE);
	});

	/* Desktop right-click menu opacity is always available. */
	GtkWidget* desktop_label = gtk_label_new_with_mnemonic("_Desktop menu opacity:");
	GtkWidget* desktop_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
	gtk_scale_set_value_pos(GTK_SCALE(desktop_scale), GTK_POS_RIGHT);
	gtk_range_set_value(GTK_RANGE(desktop_scale), 100.0);
	add_form_row(grid, COLUMN_C1, 3, desktop_label, desktop_scale, true, labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(desktop_label), desktop_scale);

	connect(desktop_scale, "value-changed", [](GtkRange* range)
	{
		const gint opacity = static_cast<gint>(gtk_range_get_value(range));
		update_desktop_menu_css(TRUE, opacity);
		set_desktop_icon_label_alpha(opacity / 100.0);
	});

	gtk_box_pack_start(page, make_aligned_frame("Opacity", grid), false, false, 0);

	GtkWidget* note = gtk_label_new(
		"Opacity: 0% = fully transparent background, 100% = solid background. "
		"Icons and text stay fully opaque. Click 'Restart panel to apply' after "
		"changing the panel value.");
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
