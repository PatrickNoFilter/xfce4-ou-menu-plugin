/*
 * OU Menu customization: Wallpaper sub-page.
 *
 * Lets OU Setting override both backgrounds in one place:
 *   - XFCE desktop background (xfconf, no privileges required)
 *   - LightDM greeter / login background (root helper via pkexec)
 *
 * Licensed under GPL-2.0-or-later, same as the upstream MeowMenu project.
 */

#include "settings-dialog.h"

#include "ui/properties/common.h"
#include "ui/slot.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <xfconf/xfconf.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <vector>

using namespace WhiskerMenu;

namespace
{

/* Root-side helper installed with the OU live image. Only the login
 * background needs elevated privileges; desktop changes go straight to
 * xfconf from this process. */
const gchar* const LOGIN_HELPER = "/usr/local/libexec/ou-wallpaper-helper";

const gchar* const GREETER_CONF = "/etc/lightdm/lightdm-gtk-greeter.conf";

const gchar* const CUSTOM_DIR = "/usr/share/backgrounds/ou-custom";

/* Remember images chosen from disk so they stay in the selector after a dialog
 * restart; the helper also copies them into CUSTOM_DIR for the greeter. */
std::string custom_config_file()
{
	gchar* path = g_build_filename(g_get_user_config_dir(), "ou-wallpapers", nullptr);
	const std::string result(path);
	g_free(path);
	return result;
}

void load_custom_paths(std::string& desktop, std::string& login)
{
	gchar* contents = nullptr;
	if (!g_file_get_contents(custom_config_file().c_str(), &contents, nullptr, nullptr)
		|| contents == nullptr)
		return;

	gchar** lines = g_strsplit(contents, "\n", -1);
	for (int i = 0; lines[i] != nullptr; ++i)
	{
		if (g_str_has_prefix(lines[i], "desktop="))
			desktop = lines[i] + std::strlen("desktop=");
		else if (g_str_has_prefix(lines[i], "login="))
			login = lines[i] + std::strlen("login=");
	}
	g_strfreev(lines);
	g_free(contents);
}

void save_custom_paths(const std::string& desktop, const std::string& login)
{
	const std::string file = custom_config_file();
	gchar* dir = g_path_get_dirname(file.c_str());
	g_mkdir_with_parents(dir, 0700);
	g_free(dir);

	gchar* contents = g_strdup_printf("desktop=%s\nlogin=%s\n",
		desktop.c_str(), login.c_str());
	g_file_set_contents(file.c_str(), contents, -1, nullptr);
	g_free(contents);
}

/* Copy a user-chosen image into a world-readable folder via the root helper so
 * the LightDM greeter (a different user) can render it before login. */
std::string install_custom_image(const std::string& path, std::string& message)
{
	gchar* quoted = g_shell_quote(path.c_str());
	gchar* command = g_strdup_printf("pkexec %s install %s", LOGIN_HELPER, quoted);
	gchar* stdout_text = nullptr;
	gchar* stderr_text = nullptr;
	gint status = 0;
	GError* error = nullptr;
	const bool ok = g_spawn_command_line_sync(command, &stdout_text, &stderr_text,
		&status, &error);

	std::string result;
	if (ok && status == 0 && stdout_text != nullptr && *stdout_text != '\0')
	{
		result = stdout_text;
		while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
			result.pop_back();
		message = "Custom image installed for both backgrounds.";
	}
	else
	{
		message = error != nullptr
			? error->message
			: (stderr_text != nullptr && *stderr_text != '\0'
				? std::string(stderr_text)
				: std::string("Could not install the chosen image."));
	}

	if (error != nullptr)
		g_error_free(error);
	g_free(stdout_text);
	g_free(stderr_text);
	g_free(command);
	g_free(quoted);
	return result;
}

std::string browse_image(GtkWidget* parent)
{
	GtkWidget* dialog = gtk_file_chooser_dialog_new(_("Choose wallpaper image"),
		GTK_WINDOW(gtk_widget_get_toplevel(parent)), GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Cancel"), GTK_RESPONSE_CANCEL, _("_Open"), GTK_RESPONSE_ACCEPT, nullptr);

	GtkFileFilter* images = gtk_file_filter_new();
	gtk_file_filter_set_name(images, _("Images"));
	gtk_file_filter_add_mime_type(images, "image/png");
	gtk_file_filter_add_mime_type(images, "image/jpeg");
	gtk_file_filter_add_mime_type(images, "image/svg+xml");
	gtk_file_filter_add_mime_type(images, "image/bmp");
	gtk_file_filter_add_mime_type(images, "image/webp");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), images);

	GtkFileFilter* all = gtk_file_filter_new();
	gtk_file_filter_set_name(all, _("All files"));
	gtk_file_filter_add_pattern(all, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), all);

	std::string result;
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (filename != nullptr)
		{
			result = filename;
			g_free(filename);
		}
	}
	gtk_widget_destroy(dialog);
	return result;
}

void browse_and_set(GtkComboBoxText* combo, GtkWidget* status)
{
	const std::string chosen = browse_image(GTK_WIDGET(combo));
	if (chosen.empty())
		return;

	std::string message;
	const std::string installed = install_custom_image(chosen, message);
	if (installed.empty())
	{
		gtk_label_set_text(GTK_LABEL(status), message.c_str());
		return;
	}

	gchar* base = g_path_get_basename(installed.c_str());
	gtk_combo_box_text_append(combo, installed.c_str(), base);
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo), installed.c_str());
	g_free(base);
	gtk_label_set_text(GTK_LABEL(status), message.c_str());
}

std::vector<std::string> find_wallpapers()
{
	std::set<std::string> unique;
	const gchar* directories[] = {
		CUSTOM_DIR,
		"/usr/share/backgrounds/ou-batang",
		"/usr/share/backgrounds/ou-akar",
		"/usr/share/backgrounds",
		nullptr
	};

	for (int i = 0; directories[i] != nullptr; ++i)
	{
		GDir* dir = g_dir_open(directories[i], 0, nullptr);
		if (dir == nullptr)
			continue;

		const gchar* name = nullptr;
		while ((name = g_dir_read_name(dir)) != nullptr)
		{
			gchar* lower = g_ascii_strdown(name, -1);
			const bool image = g_str_has_suffix(lower, ".png")
				|| g_str_has_suffix(lower, ".jpg")
				|| g_str_has_suffix(lower, ".jpeg")
				|| g_str_has_suffix(lower, ".svg");
			g_free(lower);
			if (!image)
				continue;

			gchar* full = g_build_filename(directories[i], name, nullptr);
			unique.insert(std::string(full));
			g_free(full);
		}
		g_dir_close(dir);
	}

	return std::vector<std::string>(unique.begin(), unique.end());
}

std::string current_desktop_wallpaper()
{
	XfconfChannel* channel = xfconf_channel_get("xfce4-desktop");
	gchar* value = xfconf_channel_get_string(channel,
		"/backdrop/screen0/monitor0/image-path", nullptr);
	std::string result;
	if (value != nullptr)
	{
		result = value;
		g_free(value);
	}
	if (result.empty())
	{
		value = xfconf_channel_get_string(channel,
			"/backdrop/screen0/monitordefault/image-path", nullptr);
		if (value != nullptr)
		{
			result = value;
			g_free(value);
		}
	}
	return result;
}

std::string current_login_wallpaper()
{
	std::string result;
	gchar* contents = nullptr;
	gsize length = 0;
	if (g_file_get_contents(GREETER_CONF, &contents, &length, nullptr) && contents != nullptr)
	{
		gchar** lines = g_strsplit(contents, "\n", -1);
		for (int i = 0; lines[i] != nullptr; ++i)
		{
			if (g_str_has_prefix(lines[i], "background="))
			{
				result = lines[i] + std::strlen("background=");
				break;
			}
		}
		g_strfreev(lines);
		g_free(contents);
	}
	return result;
}

void set_property(XfconfChannel* channel, const std::string& path,
	const gchar* value)
{
	xfconf_channel_set_string(channel, path.c_str(), value);
}

void apply_desktop_wallpaper(const std::string& path)
{
	XfconfChannel* channel = xfconf_channel_get("xfce4-desktop");

	/* XFCE names backdrops monitor<connector>, e.g. monitorHDMI-1. Include
	 * the connected connectors plus the generic aliases XFCE also writes. */
	std::set<std::string> monitors = {"monitor0", "monitor1", "monitordefault", "monitor"};
	gchar* xrandr_output = nullptr;
	if (g_spawn_command_line_sync("xrandr --query", &xrandr_output, nullptr, nullptr, nullptr)
		&& xrandr_output != nullptr)
	{
		gchar** lines = g_strsplit(xrandr_output, "\n", -1);
		for (int i = 0; lines[i] != nullptr; ++i)
		{
			if (g_strstr_len(lines[i], -1, " connected") == nullptr)
				continue;
			gchar** fields = g_strsplit(lines[i], " ", -1);
			if (fields[0] != nullptr && fields[0][0] != '\0')
				monitors.insert(std::string("monitor") + fields[0]);
			g_strfreev(fields);
		}
		g_strfreev(lines);
		g_free(xrandr_output);
	}

	const gint workspaces = xfconf_channel_get_int(
		xfconf_channel_get("xfwm4"), "/general/workspace_count", 4);
	const gint count = (workspaces > 0 && workspaces <= 16) ? workspaces : 4;

	for (const std::string& monitor : monitors)
	{
		const std::string base = "/backdrop/screen0/" + monitor;
		set_property(channel, base + "/last-image", path.c_str());
		set_property(channel, base + "/image-path", path.c_str());
		xfconf_channel_set_int(channel, (base + "/image-style").c_str(), 5);
		xfconf_channel_set_int(channel, (base + "/color-style").c_str(), 0);

		for (gint workspace = 0; workspace < count; ++workspace)
		{
			const std::string ws = base + "/workspace" + std::to_string(workspace);
			set_property(channel, ws + "/last-image", path.c_str());
			set_property(channel, ws + "/image-path", path.c_str());
			xfconf_channel_set_int(channel, (ws + "/image-style").c_str(), 5);
			xfconf_channel_set_int(channel, (ws + "/color-style").c_str(), 0);
		}
	}

	g_spawn_command_line_async("xfdesktop --reload", nullptr);
}

bool apply_login_wallpaper(const std::string& path, std::string& message)
{
	gchar* quoted = g_shell_quote(path.c_str());
	gchar* command = g_strdup_printf("pkexec %s login %s", LOGIN_HELPER, quoted);
	gchar* stdout_text = nullptr;
	gchar* stderr_text = nullptr;
	gint exit_status = 0;
	GError* error = nullptr;
	const bool ok = g_spawn_command_line_sync(command, &stdout_text, &stderr_text,
		&exit_status, &error);

	if (!ok || exit_status != 0)
	{
		message = error != nullptr
			? error->message
			: (stderr_text != nullptr && *stderr_text != '\0'
				? std::string(stderr_text)
				: std::string("Login background was not changed."));
	}
	else
	{
		message = "Login background updated. It appears at the next login.";
	}

	if (error != nullptr)
		g_error_free(error);
	g_free(stdout_text);
	g_free(stderr_text);
	g_free(command);
	g_free(quoted);
	return ok && exit_status == 0;
}

} // namespace

/* init_wallpaper_tab:
 * Builds the Wallpaper page: two selectors (desktop / login), an Apply
 * button and a status line. Choices are taken from installed OU artwork.
 */
GtkWidget* SettingsDialog::init_wallpaper_tab()
{
	const std::vector<std::string> wallpapers = find_wallpapers();
	const std::string desktop_current = current_desktop_wallpaper();
	const std::string login_current = current_login_wallpaper();

	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	GtkWidget* grid = make_two_column_section();
	GtkSizeGroup* labels = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	GtkWidget* desktop_label = gtk_label_new_with_mnemonic("_Desktop background:");
	GtkWidget* desktop_combo = gtk_combo_box_text_new();
	GtkWidget* login_label = gtk_label_new_with_mnemonic("_Login background:");
	GtkWidget* login_combo = gtk_combo_box_text_new();

	for (const std::string& path : wallpapers)
	{
		gchar* basename = g_path_get_basename(path.c_str());
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(desktop_combo),
			path.c_str(), basename);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(login_combo),
			path.c_str(), basename);
		g_free(basename);
	}

	std::string saved_desktop;
	std::string saved_login;
	load_custom_paths(saved_desktop, saved_login);
	for (const std::string& custom : {saved_desktop, saved_login})
	{
		if (custom.empty() || !g_file_test(custom.c_str(), G_FILE_TEST_IS_REGULAR))
			continue;
		if (std::find(wallpapers.begin(), wallpapers.end(), custom) != wallpapers.end())
			continue;
		gchar* base = g_path_get_basename(custom.c_str());
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(desktop_combo), custom.c_str(), base);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(login_combo), custom.c_str(), base);
		g_free(base);
	}

	if (!desktop_current.empty())
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(desktop_combo), desktop_current.c_str());
	if (!login_current.empty())
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(login_combo), login_current.c_str());

	add_form_row(grid, COLUMN_C1, 0, desktop_label, desktop_combo, true, labels);
	add_form_row(grid, COLUMN_C1, 1, login_label, login_combo, true, labels);
	gtk_label_set_mnemonic_widget(GTK_LABEL(desktop_label), desktop_combo);
	gtk_label_set_mnemonic_widget(GTK_LABEL(login_label), login_combo);

	GtkWidget* status = gtk_label_new("");
	gtk_label_set_line_wrap(GTK_LABEL(status), TRUE);
	gtk_label_set_xalign(GTK_LABEL(status), 0.0);
	gtk_widget_set_halign(status, GTK_ALIGN_START);

	GtkWidget* desktop_browse = gtk_button_new_with_label(_("Browse..."));
	GtkWidget* login_browse = gtk_button_new_with_label(_("Browse..."));
	add_form_row(grid, COLUMN_C2, 0, nullptr, desktop_browse, false, nullptr);
	add_form_row(grid, COLUMN_C2, 1, nullptr, login_browse, false, nullptr);

	connect(desktop_browse, "clicked", [desktop_combo, status](GtkButton*)
	{
		browse_and_set(GTK_COMBO_BOX_TEXT(desktop_combo), status);
	});
	connect(login_browse, "clicked", [login_combo, status](GtkButton*)
	{
		browse_and_set(GTK_COMBO_BOX_TEXT(login_combo), status);
	});

	gtk_box_pack_start(GTK_BOX(page), make_aligned_frame("Wallpaper", grid), false, false, 0);

	GtkWidget* apply = gtk_button_new_with_mnemonic("_Apply wallpapers");
	connect(apply, "clicked", [desktop_combo, login_combo, status](GtkButton*)
	{
		const gchar* desktop_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(desktop_combo));
		const gchar* login_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(login_combo));
		save_custom_paths(desktop_id != nullptr ? desktop_id : "",
			login_id != nullptr ? login_id : "");

		if (desktop_id != nullptr)
		{
			apply_desktop_wallpaper(desktop_id);
			gtk_label_set_text(GTK_LABEL(status), "Desktop background updated.");
		}

		if (login_id != nullptr)
		{
			std::string message;
			const bool ok = apply_login_wallpaper(login_id, message);
			if (desktop_id != nullptr)
				gtk_label_set_text(GTK_LABEL(status), message.c_str());
			else if (!ok)
				gtk_label_set_text(GTK_LABEL(status), message.c_str());
		}
	});
	gtk_box_pack_start(GTK_BOX(page), apply, false, false, 0);
	gtk_box_pack_start(GTK_BOX(page), status, false, false, 0);

	GtkWidget* note = gtk_label_new(
		"Desktop changes apply immediately. The login background is used by "
		"the LightDM greeter on the next login. Pick installed OU artwork or "
		"use Browse to choose any image from your files; custom images are "
		"copied to a folder the greeter can read.");
	gtk_label_set_line_wrap(GTK_LABEL(note), TRUE);
	gtk_label_set_xalign(GTK_LABEL(note), 0.0);
	gtk_widget_set_halign(note, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(page), note, false, false, 0);

	g_object_unref(labels);
	return wrap_in_scrolled(GTK_WIDGET(page));
}
