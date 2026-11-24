/*
 * OU Menu customization: System Language Settings tab.
 * Licensed under GPL-2.0-or-later, same as the upstream MeowMenu/Whisker Menu.
 */

#include "settings-dialog.h"

#include "ui/properties/common.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

namespace
{

struct LanguageChoice
{
	const gchar* label;
	const gchar* value;
};

const LanguageChoice language_choices[] = {
	{ "Indonesian", "id" },
	{ "English", "en_US" },
	{ "System default (remove override)", "" },
};

/* apply_language_selection:
 * @button: the Apply button.
 * @data: unused.
 *
 * Rewrites ~/.xsessionrc with the chosen LANGUAGE override. The change takes
 * effect on the next login because gettext reads LANGUAGE at process start.
 */
void apply_language_selection(GtkButton* button, gpointer)
{
	GtkWidget* combo = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "lang-combo"));
	GtkWidget* current_label = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "lang-current"));
	GtkWidget* status = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "lang-status"));

	const gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
	if (active < 0 || active >= static_cast<gint>(G_N_ELEMENTS(language_choices)))
		return;

	const gchar* value = language_choices[active].value;
	gchar* path = g_build_filename(g_get_home_dir(), ".xsessionrc", nullptr);
	gchar* old_contents = nullptr;
	g_file_get_contents(path, &old_contents, nullptr, nullptr);

	GString* contents = g_string_new(nullptr);
	if (old_contents)
	{
		gchar** lines = g_strsplit(old_contents, "\n", -1);
		for (gint i = 0; lines[i]; ++i)
		{
			gchar* line = g_strstrip(lines[i]);
			if (g_str_has_prefix(line, "export LANGUAGE=") ||
				g_str_has_prefix(line, "LANGUAGE="))
				continue;
			if (*line)
				g_string_append_printf(contents, "%s\n", line);
		}
		g_strfreev(lines);
	}
	if (value && *value)
		g_string_append_printf(contents, "export LANGUAGE=%s\n", value);

	const gboolean saved = g_file_set_contents(path, contents->str, contents->len, nullptr);
	g_string_free(contents, TRUE);
	g_free(old_contents);
	g_free(path);

	gtk_label_set_text(GTK_LABEL(current_label), (value && *value) ? value : "(system default)");
	gtk_label_set_text(GTK_LABEL(status), saved
		? "Saved. Log out and log back in to apply the language."
		: "Failed to save ~/.xsessionrc.");
}

} // namespace

/* init_system_language_tab:
 *
 * Builds the "System Language Settings" page. MeowMenu has no per-app locale
 * switch; it follows gettext's LANGUAGE variable. This page edits the user's
 * ~/.xsessionrc so the whole Xfce session (including the menu) picks up the
 * new language at the next login.
 *
 * Returns: the page wrapped for the dialog stack.
 */
GtkWidget* SettingsDialog::init_system_language_tab()
{
	GtkBox* page = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 18));
	gtk_container_set_border_width(GTK_CONTAINER(page), 12);

	GtkWidget* grid = make_two_column_section();
	GtkSizeGroup* labels = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	const gchar* language = g_getenv("LANGUAGE");
	if (!language || !*language)
		language = g_getenv("LANG");
	if (!language || !*language)
		language = "(system default)";

	GtkWidget* current_label = gtk_label_new(language);
	gtk_widget_set_halign(current_label, GTK_ALIGN_START);
	add_form_row(grid, COLUMN_C1, 0,
		gtk_label_new_with_mnemonic("_Current language:"), current_label, true, labels);

	GtkWidget* combo = gtk_combo_box_text_new();
	for (guint i = 0; i < G_N_ELEMENTS(language_choices); ++i)
	{
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo),
			language_choices[i].value, language_choices[i].label);
	}

	const gchar* override = g_getenv("LANGUAGE");
	gint selected = 0;
	for (guint i = 0; i < G_N_ELEMENTS(language_choices); ++i)
	{
		if (g_strcmp0(language_choices[i].value, override ? override : "") == 0)
		{
			selected = static_cast<gint>(i);
			break;
		}
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), selected);
	add_form_row(grid, COLUMN_C1, 1,
		gtk_label_new_with_mnemonic("_Select language:"), combo, true, labels);

	GtkWidget* apply = gtk_button_new_with_label("Apply");
	add_form_row(grid, COLUMN_C1, 2, nullptr, apply, false, nullptr);

	GtkWidget* status = gtk_label_new("Changes take effect after logging out and back in.");
	gtk_label_set_line_wrap(GTK_LABEL(status), TRUE);
	gtk_widget_set_halign(status, GTK_ALIGN_START);
	add_form_row(grid, COLUMN_C1, 3, nullptr, status, false, nullptr);

	g_object_set_data(G_OBJECT(apply), "lang-combo", combo);
	g_object_set_data(G_OBJECT(apply), "lang-current", current_label);
	g_object_set_data(G_OBJECT(apply), "lang-status", status);
	g_signal_connect(apply, "clicked", G_CALLBACK(apply_language_selection), nullptr);

	gtk_box_pack_start(page, make_aligned_frame("System Language Settings", grid), false, false, 0);
	g_object_unref(labels);

	return wrap_in_scrolled(GTK_WIDGET(page));
}
