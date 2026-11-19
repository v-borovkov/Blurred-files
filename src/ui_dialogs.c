#include "app.h"
#include <math.h>
#include <string.h>
#include <glib/gstdio.h>

static void on_draw_modal_squircle(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data);

/* --- Persistent Application Settings (~/.config/blurred_files/settings.ini) --- */
static char *get_settings_file_path(void)
{
    char *dir = g_build_filename(g_get_user_config_dir(), "blurred_files", NULL);
    g_mkdir_with_parents(dir, 0755);
    char *path = g_build_filename(dir, "settings.ini", NULL);
    g_free(dir);
    return path;
}

void save_app_settings(AppState *state)
{
    if (!state)
        return;

    GKeyFile *kf = g_key_file_new();
    g_key_file_set_boolean(kf, "General", "is_list_view", state->is_list_view);
    g_key_file_set_boolean(kf, "General", "show_hidden", state->show_hidden);
    g_key_file_set_boolean(kf, "General", "enable_thumbnails", state->enable_thumbnails);
    g_key_file_set_boolean(kf, "General", "double_click_open", state->double_click_open);
    g_key_file_set_integer(kf, "General", "grid_cols", state->grid_cols);
    g_key_file_set_integer(kf, "General", "sort_column", (int)state->sort_column);
    g_key_file_set_boolean(kf, "General", "sort_ascending", state->sort_ascending);

    guint count = g_list_length(state->pinned_folders);
    if (count > 0)
    {
        const char **arr = g_new0(const char *, count + 1);
        guint idx = 0;
        for (GList *l = state->pinned_folders; l != NULL; l = l->next)
        {
            arr[idx++] = (const char *)l->data;
        }
        g_key_file_set_string_list(kf, "Sidebar", "pinned_folders", arr, count);
        g_free(arr);
    }

    char *path = get_settings_file_path();
    g_key_file_save_to_file(kf, path, NULL);
    g_free(path);
    g_key_file_free(kf);
}

void load_app_settings(AppState *state)
{
    if (!state)
        return;

    char *path = get_settings_file_path();
    GKeyFile *kf = g_key_file_new();

    if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL))
    {
        if (g_key_file_has_key(kf, "General", "is_list_view", NULL))
        {
            state->is_list_view = g_key_file_get_boolean(kf, "General", "is_list_view", NULL);
        }
        if (g_key_file_has_key(kf, "General", "show_hidden", NULL))
        {
            state->show_hidden = g_key_file_get_boolean(kf, "General", "show_hidden", NULL);
        }
        if (g_key_file_has_key(kf, "General", "enable_thumbnails", NULL))
        {
            state->enable_thumbnails = g_key_file_get_boolean(kf, "General", "enable_thumbnails", NULL);
        }
        if (g_key_file_has_key(kf, "General", "double_click_open", NULL))
        {
            state->double_click_open = g_key_file_get_boolean(kf, "General", "double_click_open", NULL);
        }
        if (g_key_file_has_key(kf, "General", "grid_cols", NULL))
        {
            int cols = g_key_file_get_integer(kf, "General", "grid_cols", NULL);
            state->grid_cols = CLAMP(cols, 0, 4);
        }
        if (g_key_file_has_key(kf, "General", "sort_column", NULL))
        {
            int sc = g_key_file_get_integer(kf, "General", "sort_column", NULL);
            state->sort_column = (SortColumn)CLAMP(sc, SORT_BY_NAME, SORT_BY_SIZE);
        }
        if (g_key_file_has_key(kf, "General", "sort_ascending", NULL))
        {
            state->sort_ascending = g_key_file_get_boolean(kf, "General", "sort_ascending", NULL);
        }

        gsize len = 0;
        char **pins = g_key_file_get_string_list(kf, "Sidebar", "pinned_folders", &len, NULL);
        if (pins)
        {
            for (gsize i = 0; i < len; i++)
            {
                if (g_file_test(pins[i], G_FILE_TEST_IS_DIR))
                {
                    state->pinned_folders = g_list_append(state->pinned_folders, g_strdup(pins[i]));
                }
            }
            g_strfreev(pins);
        }
    }

    g_key_file_free(kf);
    g_free(path);
}

/* --- Squircle Rename Dialog --- */
typedef struct
{
    AppState *state;
    char *old_path;
    GtkWidget *entry;
    GtkWidget *dialog;
} RenameDialogData;

static void close_rename_dialog_cleanly(RenameDialogData *rdd)
{
    if (!rdd)
        return;
    GtkWidget *dlg = rdd->dialog;
    g_free(rdd->old_path);
    g_free(rdd);
    if (dlg && GTK_IS_WINDOW(dlg))
    {
        gtk_window_destroy(GTK_WINDOW(dlg));
    }
}

static void on_rename_confirm(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    RenameDialogData *rdd = (RenameDialogData *)user_data;
    const char *new_name = gtk_editable_get_text(GTK_EDITABLE(rdd->entry));

    if (new_name && *new_name != '\0')
    {
        char *parent_dir = g_path_get_dirname(rdd->old_path);
        char *new_path = g_build_filename(parent_dir, new_name, NULL);

        if (g_strcmp0(rdd->old_path, new_path) != 0)
        {
            GFile *src = g_file_new_for_path(rdd->old_path);
            GFile *dst = g_file_new_for_path(new_path);
            GError *err = NULL;

            if (g_file_move(src, dst, G_FILE_COPY_NONE, NULL, NULL, NULL, &err))
            {
                record_undo_action(rdd->state,
                                   UNDO_MOVE_BACK,
                                   g_list_append(NULL, g_strdup(rdd->old_path)),
                                   g_list_append(NULL, g_strdup(new_path)));
                populate_directory(rdd->state, rdd->state->current_path);
            }
            else if (err)
            {
                g_warning("Rename failed: %s", err->message);
                g_error_free(err);
            }

            g_object_unref(src);
            g_object_unref(dst);
        }

        g_free(new_path);
        g_free(parent_dir);
    }

    close_rename_dialog_cleanly(rdd);
}

static void on_rename_entry_activate(GtkEntry *entry, gpointer user_data)
{
    (void)entry;
    on_rename_confirm(NULL, user_data);
}

static void on_rename_cancel_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    close_rename_dialog_cleanly((RenameDialogData *)user_data);
}

static gboolean on_rename_key_pressed(GtkEventControllerKey *ctrl,
                                      guint keyval,
                                      guint keycode,
                                      GdkModifierType mods,
                                      gpointer user_data)
{
    (void)ctrl;
    (void)keycode;
    (void)mods;
    if (keyval == GDK_KEY_Escape)
    {
        close_rename_dialog_cleanly((RenameDialogData *)user_data);
        return TRUE;
    }
    return FALSE;
}

void show_rename_dialog(AppState *state, const char *old_path, const char *current_name)
{
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Rename");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 380, 210);
    gtk_widget_add_css_class(dialog, "settings-modal-window");

    RenameDialogData *rdd = g_new0(RenameDialogData, 1);
    rdd->state = state;
    rdd->old_path = g_strdup(old_path);
    rdd->dialog = dialog;

    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_rename_key_pressed), rdd);
    gtk_widget_add_controller(dialog, key_ctrl);

    GtkWidget *overlay = gtk_overlay_new();
    GtkWidget *bg_canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(bg_canvas), on_draw_modal_squircle, NULL, NULL);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), bg_canvas);

    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(card, "squircle-modal-inner");

    GtkWidget *handle = gtk_window_handle_new();
    GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(hdr, "settings-modal-header");

    GtkWidget *hdr_icon = gtk_image_new_from_icon_name("document-edit-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(hdr_icon), 18);
    gtk_widget_add_css_class(hdr_icon, "settings-header-icon");

    GtkWidget *title_lbl = gtk_label_new("Rename");
    gtk_widget_add_css_class(title_lbl, "settings-modal-title");
    gtk_widget_set_hexpand(title_lbl, TRUE);
    gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);

    GtkWidget *close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(close_btn, "win-ctrl-btn");
    gtk_widget_set_size_request(close_btn, 26, 26);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_rename_cancel_clicked), rdd);

    gtk_box_append(GTK_BOX(hdr), hdr_icon);
    gtk_box_append(GTK_BOX(hdr), title_lbl);
    gtk_box_append(GTK_BOX(hdr), close_btn);
    gtk_window_handle_set_child(GTK_WINDOW_HANDLE(handle), hdr);
    gtk_box_append(GTK_BOX(card), handle);

    GtkWidget *entry = gtk_entry_new();
    gtk_widget_add_css_class(entry, "modal-text-entry");
    gtk_editable_set_text(GTK_EDITABLE(entry), current_name);
    rdd->entry = entry;

    const char *dot = strrchr(current_name, '.');
    if (dot && dot != current_name && !g_file_test(old_path, G_FILE_TEST_IS_DIR))
    {
        glong stem_chars = g_utf8_pointer_to_offset(current_name, dot);
        gtk_editable_select_region(GTK_EDITABLE(entry), 0, (int)stem_chars);
    }
    else
    {
        gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    }

    g_signal_connect(entry, "activate", G_CALLBACK(on_rename_entry_activate), rdd);
    gtk_box_append(GTK_BOX(card), entry);

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(btn_row, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_row, 4);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel_btn, "modal-secondary-btn");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_rename_cancel_clicked), rdd);

    GtkWidget *ok_btn = gtk_button_new_with_label("Rename");
    gtk_widget_add_css_class(ok_btn, "modal-primary-btn");
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_rename_confirm), rdd);
    gtk_box_append(GTK_BOX(btn_row), cancel_btn);
    gtk_box_append(GTK_BOX(btn_row), ok_btn);
    gtk_box_append(GTK_BOX(card), btn_row);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), card);
    gtk_window_set_child(GTK_WINDOW(dialog), overlay);
    gtk_window_present(GTK_WINDOW(dialog));
    gtk_widget_grab_focus(entry);
}
/* --- Squircle Create New Folder / File Dialog --- */
typedef struct
{
    AppState *state;
    gboolean is_folder;
    GtkWidget *entry;
    GtkWidget *dialog;
} CreateItemDialogData;
static void close_create_dialog_cleanly(CreateItemDialogData *cid)
{
    if (!cid)
        return;
    GtkWidget *dlg = cid->dialog;
    g_free(cid);
    if (dlg && GTK_IS_WINDOW(dlg))
    {
        gtk_window_destroy(GTK_WINDOW(dlg));
    }
}
static void on_create_item_confirm(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    CreateItemDialogData *cid = (CreateItemDialogData *)user_data;
    const char *name = gtk_editable_get_text(GTK_EDITABLE(cid->entry));
    if (name && *name != '\0' && cid->state->current_path)
    {
        char *target_path = g_build_filename(cid->state->current_path, name, NULL);
        GFile *target = g_file_new_for_path(target_path);
        GError *err = NULL;
        if (cid->is_folder)
        {
            if (g_file_make_directory(target, NULL, &err))
            {
                populate_directory(cid->state, cid->state->current_path);
            }
            else if (err)
            {
                g_warning("Create folder failed: %s", err->message);
                g_error_free(err);
            }
        }
        else
        {
            GFileOutputStream *stream = g_file_create(target, G_FILE_CREATE_NONE, NULL, &err);
            if (stream)
            {
                g_object_unref(stream);
                populate_directory(cid->state, cid->state->current_path);
            }
            else if (err)
            {
                g_warning("Create file failed: %s", err->message);
                g_error_free(err);
            }
        }
        g_object_unref(target);
        g_free(target_path);
    }
    close_create_dialog_cleanly(cid);
}
static void on_create_entry_activate(GtkEntry *entry, gpointer user_data)
{
    (void)entry;
    on_create_item_confirm(NULL, user_data);
}
static void on_create_cancel_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    close_create_dialog_cleanly((CreateItemDialogData *)user_data);
}
static gboolean on_create_key_pressed(GtkEventControllerKey *ctrl,
                                      guint keyval,
                                      guint keycode,
                                      GdkModifierType mods,
                                      gpointer user_data)
{
    (void)ctrl;
    (void)keycode;
    (void)mods;
    if (keyval == GDK_KEY_Escape)
    {
        close_create_dialog_cleanly((CreateItemDialogData *)user_data);
        return TRUE;
    }
    return FALSE;
}
void show_create_item_dialog(AppState *state, gboolean is_folder)
{
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), is_folder ? "New Folder" : "New File");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 380, 210);
    gtk_widget_add_css_class(dialog, "settings-modal-window");
    CreateItemDialogData *cid = g_new0(CreateItemDialogData, 1);
    cid->state = state;
    cid->is_folder = is_folder;
    cid->dialog = dialog;
    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_create_key_pressed), cid);
    gtk_widget_add_controller(dialog, key_ctrl);
    GtkWidget *overlay = gtk_overlay_new();
    GtkWidget *bg_canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(bg_canvas), on_draw_modal_squircle, NULL, NULL);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), bg_canvas);
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_add_css_class(card, "squircle-modal-inner");
    GtkWidget *handle = gtk_window_handle_new();
    GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(hdr, "settings-modal-header");
    GtkWidget *hdr_icon = gtk_image_new_from_icon_name(
        is_folder ? "folder-new-symbolic" : "document-new-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(hdr_icon), 18);
    gtk_widget_add_css_class(hdr_icon, "settings-header-icon");
    GtkWidget *title_lbl = gtk_label_new(is_folder ? "New Folder" : "New File");
    gtk_widget_add_css_class(title_lbl, "settings-modal-title");
    gtk_widget_set_hexpand(title_lbl, TRUE);
    gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);
    GtkWidget *close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(close_btn, "win-ctrl-btn");
    gtk_widget_set_size_request(close_btn, 26, 26);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_create_cancel_clicked), cid);
    gtk_box_append(GTK_BOX(hdr), hdr_icon);
    gtk_box_append(GTK_BOX(hdr), title_lbl);
    gtk_box_append(GTK_BOX(hdr), close_btn);
    gtk_window_handle_set_child(GTK_WINDOW_HANDLE(handle), hdr);
    gtk_box_append(GTK_BOX(card), handle);
    GtkWidget *entry = gtk_entry_new();
    gtk_widget_add_css_class(entry, "modal-text-entry");
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(entry),
        is_folder ? "Folder name..." : "File name (e.g. notes.txt)...");
    cid->entry = entry;
    g_signal_connect(entry, "activate", G_CALLBACK(on_create_entry_activate), cid);
    gtk_box_append(GTK_BOX(card), entry);
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(btn_row, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_row, 4);
    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel_btn, "modal-secondary-btn");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_create_cancel_clicked), cid);
    GtkWidget *ok_btn = gtk_button_new_with_label(is_folder ? "Create Folder" : "Create File");
    gtk_widget_add_css_class(ok_btn, "modal-primary-btn");
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_create_item_confirm), cid);
    gtk_box_append(GTK_BOX(btn_row), cancel_btn);
    gtk_box_append(GTK_BOX(btn_row), ok_btn);
    gtk_box_append(GTK_BOX(card), btn_row);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), card);
    gtk_window_set_child(GTK_WINDOW(dialog), overlay);
    gtk_window_present(GTK_WINDOW(dialog));
    gtk_widget_grab_focus(entry);
}
/* --- Open With Application Chooser Dialog --- */
typedef struct
{
    char *file_path;
    GtkWidget *dialog;
} OpenWithData;
static void on_open_with_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
    (void)box;
    OpenWithData *owd = (OpenWithData *)user_data;
    GAppInfo *app_info = g_object_get_data(G_OBJECT(row), "app-info");
    if (app_info && owd->file_path)
    {
        GFile *file = g_file_new_for_path(owd->file_path);
        GList *files = g_list_append(NULL, file);
        g_app_info_launch(app_info, files, NULL, NULL);
        g_list_free(files);
        g_object_unref(file);
    }
    GtkWidget *dlg = owd->dialog;
    g_free(owd->file_path);
    g_free(owd);
    if (dlg && GTK_IS_WINDOW(dlg))
    {
        gtk_window_destroy(GTK_WINDOW(dlg));
    }
}
static void on_open_with_close_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    OpenWithData *owd = (OpenWithData *)user_data;
    GtkWidget *dlg = owd->dialog;
    g_free(owd->file_path);
    g_free(owd);
    if (dlg && GTK_IS_WINDOW(dlg))
    {
        gtk_window_destroy(GTK_WINDOW(dlg));
    }
}
void show_open_with_dialog(AppState *state, const char *file_path)
{
    GFile *file = g_file_new_for_path(file_path);
    GFileInfo *info = g_file_query_info(file, "standard::content-type", G_FILE_QUERY_INFO_NONE, NULL, NULL);
    const char *ctype = info ? g_file_info_get_content_type(info) : "application/octet-stream";
    GList *apps = g_app_info_get_all_for_type(ctype);
    if (!apps)
    {
        apps = g_app_info_get_all();
    }
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Open With");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 380, 420);
    gtk_widget_add_css_class(dialog, "settings-modal-window");
    OpenWithData *owd = g_new0(OpenWithData, 1);
    owd->file_path = g_strdup(file_path);
    owd->dialog = dialog;
    GtkWidget *overlay = gtk_overlay_new();
    GtkWidget *bg_canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(bg_canvas), on_draw_modal_squircle, NULL, NULL);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), bg_canvas);
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(card, "squircle-modal-inner");
    GtkWidget *handle = gtk_window_handle_new();
    GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *title_lbl = gtk_label_new("Open With Application");
    gtk_widget_add_css_class(title_lbl, "settings-modal-title");
    gtk_widget_set_hexpand(title_lbl, TRUE);
    gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);
    GtkWidget *close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(close_btn, "win-ctrl-btn");
    gtk_widget_set_size_request(close_btn, 26, 26);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_open_with_close_clicked), owd);
    gtk_box_append(GTK_BOX(hdr), title_lbl);
    gtk_box_append(GTK_BOX(hdr), close_btn);
    gtk_window_handle_set_child(GTK_WINDOW_HANDLE(handle), hdr);
    gtk_box_append(GTK_BOX(card), handle);
    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(sw, TRUE);
    GtkWidget *listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox), GTK_SELECTION_NONE);
    g_signal_connect(listbox, "row-activated", G_CALLBACK(on_open_with_row_activated), owd);
    for (GList *l = apps; l != NULL; l = l->next)
    {
        GAppInfo *ai = G_APP_INFO(l->data);
        if (!g_app_info_should_show(ai))
            continue;
        GtkWidget *row = gtk_list_box_row_new();
        g_object_set_data_full(G_OBJECT(row), "app-info", g_object_ref(ai), g_object_unref);
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_add_css_class(hbox, "settings-row");
        GIcon *gicon = g_app_info_get_icon(ai);
        GtkWidget *icon = gicon
                              ? gtk_image_new_from_gicon(gicon)
                              : gtk_image_new_from_icon_name("application-x-executable");
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
        GtkWidget *lbl = gtk_label_new(g_app_info_get_display_name(ai));
        gtk_widget_add_css_class(lbl, "settings-row-title");
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(hbox), icon);
        gtk_box_append(GTK_BOX(hbox), lbl);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), hbox);
        gtk_list_box_append(GTK_LIST_BOX(listbox), row);
    }
    g_list_free_full(apps, g_object_unref);
    if (info)
        g_object_unref(info);
    g_object_unref(file);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), listbox);
    gtk_box_append(GTK_BOX(card), sw);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), card);
    gtk_window_set_child(GTK_WINDOW(dialog), overlay);
    gtk_window_present(GTK_WINDOW(dialog));
}
/* --- Corner-Matched Squircle Modal Canvas --- */
static void draw_squircle_corner_arc(cairo_t *cr,
                                     double cx,
                                     double cy,
                                     double rx,
                                     double ry,
                                     double start_angle,
                                     double end_angle,
                                     double n)
{
    const int steps = 160;
    for (int i = 0; i <= steps; i++)
    {
        double t = start_angle + (end_angle - start_angle) * ((double)i / steps);
        double cos_t = cos(t);
        double sin_t = sin(t);
        double sx = (cos_t >= 0.0 ? 1.0 : -1.0) * pow(fabs(cos_t), 2.0 / n);
        double sy = (sin_t >= 0.0 ? 1.0 : -1.0) * pow(fabs(sin_t), 2.0 / n);
        cairo_line_to(cr, cx + sx * rx, cy + sy * ry);
    }
}
static void on_draw_modal_squircle(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data)
{
    (void)area;
    (void)data;
    if (width <= 0 || height <= 0)
        return;
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);
    double pad = 8.0;
    double x = pad;
    double y = pad;
    double w = width - pad * 2.0;
    double h = height - pad * 2.0;
    if (w <= 0 || h <= 0)
        return;
    double r = MIN(38.0, MIN(w, h) * 0.16);
    double n = 4.2;
    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);
    cairo_new_path(cr);
    cairo_move_to(cr, x + r, y);
    cairo_line_to(cr, x + w - r, y);
    draw_squircle_corner_arc(cr, x + w - r, y + r, r, r, -G_PI_2, 0.0, n);
    cairo_line_to(cr, x + w, y + h - r);
    draw_squircle_corner_arc(cr, x + w - r, y + h - r, r, r, 0.0, G_PI_2, n);
    cairo_line_to(cr, x + r, y + h);
    draw_squircle_corner_arc(cr, x + r, y + h - r, r, r, G_PI_2, G_PI, n);
    cairo_line_to(cr, x, y + r);
    draw_squircle_corner_arc(cr, x + r, y + r, r, r, G_PI, 1.5 * G_PI, n);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, 0.97, 0.975, 0.98, 0.98);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.09);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
}
/* --- Squircle Properties Dialog --- */
typedef struct
{
    GtkWidget *dialog;
    GtkWidget *size_lbl;
    GCancellable *cancellable;
    char *target_path;
} PropertiesDialogData;
typedef struct
{
    guint64 total_bytes;
    guint64 file_count;
    guint64 dir_count;
} FolderStatsResult;
static void accumulate_folder_stats(GFile *dir, FolderStatsResult *stats, GCancellable *cancel)
{
    if (g_cancellable_is_cancelled(cancel))
        return;
    GFileEnumerator *en = g_file_enumerate_children(
        dir,
        "standard::name,standard::type,standard::size",
        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
        cancel,
        NULL);
    if (!en)
        return;
    GFileInfo *info = NULL;
    while (!g_cancellable_is_cancelled(cancel) &&
           (info = g_file_enumerator_next_file(en, cancel, NULL)) != NULL)
    {
        GFileType ftype = g_file_info_get_file_type(info);
        if (ftype == G_FILE_TYPE_DIRECTORY)
        {
            stats->dir_count++;
            GFile *child = g_file_get_child(dir, g_file_info_get_name(info));
            accumulate_folder_stats(child, stats, cancel);
            g_object_unref(child);
        }
        else
        {
            stats->file_count++;
            stats->total_bytes += (guint64)MAX(0, g_file_info_get_size(info));
        }
        g_object_unref(info);
    }
    g_object_unref(en);
}
static void folder_size_thread_func(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
    (void)source_object;
    const char *path = (const char *)task_data;
    FolderStatsResult *res = g_new0(FolderStatsResult, 1);
    GFile *root = g_file_new_for_path(path);
    accumulate_folder_stats(root, res, cancellable);
    g_object_unref(root);
    g_task_return_pointer(task, res, g_free);
}
static void on_folder_size_ready(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    (void)source_object;
    PropertiesDialogData *pdd = (PropertiesDialogData *)user_data;
    FolderStatsResult *stats = g_task_propagate_pointer(G_TASK(res), NULL);
    if (stats && pdd->size_lbl && GTK_IS_LABEL(pdd->size_lbl))
    {
        char *fmt_size = g_format_size(stats->total_bytes);
        char *text = g_strdup_printf("%s  (%lu files, %lu folders)",
                                     fmt_size,
                                     (unsigned long)stats->file_count,
                                     (unsigned long)stats->dir_count);
        gtk_label_set_text(GTK_LABEL(pdd->size_lbl), text);
        g_free(fmt_size);
        g_free(text);
    }
    g_free(stats);
}
static void on_properties_close_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    PropertiesDialogData *pdd = (PropertiesDialogData *)user_data;
    if (pdd->cancellable)
    {
        g_cancellable_cancel(pdd->cancellable);
        g_object_unref(pdd->cancellable);
    }
    GtkWidget *dlg = pdd->dialog;
    g_free(pdd->target_path);
    g_free(pdd);
    if (dlg && GTK_IS_WINDOW(dlg))
    {
        gtk_window_destroy(GTK_WINDOW(dlg));
    }
}
static GtkWidget *add_prop_row(GtkWidget *parent, const char *key_text, const char *val_text)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(row, "settings-row");
    GtkWidget *k_lbl = gtk_label_new(key_text);
    gtk_widget_add_css_class(k_lbl, "prop-key-label");
    gtk_widget_set_size_request(k_lbl, 95, -1);
    gtk_widget_set_halign(k_lbl, GTK_ALIGN_START);
    GtkWidget *v_lbl = gtk_label_new(val_text);
    gtk_widget_add_css_class(v_lbl, "prop-val-label");
    gtk_widget_set_hexpand(v_lbl, TRUE);
    gtk_widget_set_halign(v_lbl, GTK_ALIGN_START);
    gtk_label_set_selectable(GTK_LABEL(v_lbl), TRUE);
    gtk_label_set_wrap(GTK_LABEL(v_lbl), TRUE);
    gtk_box_append(GTK_BOX(row), k_lbl);
    gtk_box_append(GTK_BOX(row), v_lbl);
    gtk_box_append(GTK_BOX(parent), row);
    return v_lbl;
}
void show_properties_dialog(AppState *state, const char *path)
{
    if (!path)
        return;
    GFile *file = g_file_new_for_path(path);
    GFileInfo *info = g_file_query_info(
        file,
        "standard::display-name,standard::type,standard::size,standard::content-type,standard::icon,time::modified,unix::mode",
        G_FILE_QUERY_INFO_NONE,
        NULL,
        NULL);
    const char *name = info ? g_file_info_get_display_name(info) : path;
    gboolean is_dir = info && (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY);
    const char *ctype = info ? g_file_info_get_content_type(info) : NULL;
    char *type_desc = is_dir ? g_strdup("Folder") : (ctype ? g_content_type_get_description(ctype) : g_strdup("File"));
    char *parent_dir = g_path_get_dirname(path);
    char *mod_str = g_strdup("Unknown");
    if (info)
    {
        GDateTime *dt = g_file_info_get_modification_date_time(info);
        if (dt)
        {
            GDateTime *local = g_date_time_to_local(dt);
            g_free(mod_str);
            mod_str = g_date_time_format(local, "%B %d, %Y at %H:%M:%S");
            g_date_time_unref(local);
            g_date_time_unref(dt);
        }
    }
    guint32 mode = info ? g_file_info_get_attribute_uint32(info, "unix::mode") : 0;
    char perm_str[32];
    snprintf(perm_str, sizeof(perm_str), "%c%c%c%c%c%c%c%c%c (%04o)",
             (mode & 0400) ? 'r' : '-', (mode & 0200) ? 'w' : '-', (mode & 0100) ? 'x' : '-',
             (mode & 0040) ? 'r' : '-', (mode & 0020) ? 'w' : '-', (mode & 0010) ? 'x' : '-',
             (mode & 0004) ? 'r' : '-', (mode & 0002) ? 'w' : '-', (mode & 0001) ? 'x' : '-',
             mode & 07777);
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Properties");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 420, 390);
    gtk_widget_add_css_class(dialog, "settings-modal-window");
    PropertiesDialogData *pdd = g_new0(PropertiesDialogData, 1);
    pdd->dialog = dialog;
    pdd->target_path = g_strdup(path);
    GtkWidget *overlay = gtk_overlay_new();
    GtkWidget *bg_canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(bg_canvas), on_draw_modal_squircle, NULL, NULL);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), bg_canvas);
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(card, "squircle-modal-inner");
    GtkWidget *handle = gtk_window_handle_new();
    GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *title_lbl = gtk_label_new("Properties");
    gtk_widget_add_css_class(title_lbl, "settings-modal-title");
    gtk_widget_set_hexpand(title_lbl, TRUE);
    gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);
    GtkWidget *close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(close_btn, "win-ctrl-btn");
    gtk_widget_set_size_request(close_btn, 26, 26);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_properties_close_clicked), pdd);
    gtk_box_append(GTK_BOX(hdr), title_lbl);
    gtk_box_append(GTK_BOX(hdr), close_btn);
    gtk_window_handle_set_child(GTK_WINDOW_HANDLE(handle), hdr);
    gtk_box_append(GTK_BOX(card), handle);
    /* Hero Header with Icon & Name */
    GtkWidget *hero = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_bottom(hero, 4);
    GIcon *gicon = info ? g_file_info_get_icon(info) : NULL;
    GtkWidget *hero_icon = gicon
                               ? gtk_image_new_from_gicon(gicon)
                               : gtk_image_new_from_icon_name(is_dir ? "folder" : "text-x-generic");
    gtk_image_set_pixel_size(GTK_IMAGE(hero_icon), 44);
    GtkWidget *hero_name = gtk_label_new(name);
    gtk_widget_add_css_class(hero_name, "prop-hero-name");
    gtk_widget_set_halign(hero_name, GTK_ALIGN_START);
    gtk_label_set_wrap(GTK_LABEL(hero_name), TRUE);
    gtk_box_append(GTK_BOX(hero), hero_icon);
    gtk_box_append(GTK_BOX(hero), hero_name);
    gtk_box_append(GTK_BOX(card), hero);
    add_prop_row(card, "Type", type_desc);
    add_prop_row(card, "Location", parent_dir);
    if (is_dir)
    {
        pdd->size_lbl = add_prop_row(card, "Size", "Calculating...");
        pdd->cancellable = g_cancellable_new();
        GTask *task = g_task_new(NULL, pdd->cancellable, on_folder_size_ready, pdd);
        g_task_set_task_data(task, g_strdup(path), g_free);
        g_task_run_in_thread(task, folder_size_thread_func);
        g_object_unref(task);
    }
    else
    {
        goffset fsize = info ? g_file_info_get_size(info) : 0;
        char *sz_str = g_format_size((guint64)MAX(0, fsize));
        add_prop_row(card, "Size", sz_str);
        g_free(sz_str);
    }
    add_prop_row(card, "Modified", mod_str);
    add_prop_row(card, "Permissions", perm_str);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), card);
    gtk_window_set_child(GTK_WINDOW(dialog), overlay);
    g_free(type_desc);
    g_free(parent_dir);
    g_free(mod_str);
    if (info)
        g_object_unref(info);
    g_object_unref(file);
    gtk_window_present(GTK_WINDOW(dialog));
}
/* --- Squircle Preferences / Settings Dialog (Includes Icon & List Sorting) --- */
static void on_settings_hidden_toggled(GtkSwitch *sw, gboolean state_val, gpointer user_data)
{
    (void)sw;
    AppState *state = (AppState *)user_data;
    state->show_hidden = state_val;
    save_app_settings(state);
    if (state->current_path)
        populate_directory(state, state->current_path);
}
static void on_settings_thumbs_toggled(GtkSwitch *sw, gboolean state_val, gpointer user_data)
{
    (void)sw;
    AppState *state = (AppState *)user_data;
    state->enable_thumbnails = state_val;
    save_app_settings(state);
    if (state->current_path)
        populate_directory(state, state->current_path);
}
static void on_settings_dblclick_toggled(GtkSwitch *sw, gboolean state_val, gpointer user_data)
{
    (void)sw;
    AppState *state = (AppState *)user_data;
    state->double_click_open = state_val;
    save_app_settings(state);
    if (state->current_path)
        populate_directory(state, state->current_path);
}
static void on_settings_sort_col_changed(GObject *dropdown, GParamSpec *pspec, gpointer user_data)
{
    (void)pspec;
    AppState *state = (AppState *)user_data;
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(dropdown));
    if (selected != GTK_INVALID_LIST_POSITION && (int)selected != (int)state->sort_column)
    {
        state->sort_column = (SortColumn)selected;
        save_app_settings(state);
        if (state->current_path)
            populate_directory(state, state->current_path);
    }
}
static void on_settings_sort_order_toggled(GtkToggleButton *btn, gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    gboolean asc = gtk_toggle_button_get_active(btn);
    gtk_button_set_label(GTK_BUTTON(btn), asc ? "Ascending ▲" : "Descending ▼");
    if (state->sort_ascending != asc)
    {
        state->sort_ascending = asc;
        save_app_settings(state);
        if (state->current_path)
            populate_directory(state, state->current_path);
    }
}
static void on_settings_close_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    GtkWidget *dlg = GTK_WIDGET(user_data);
    if (dlg && GTK_IS_WINDOW(dlg))
    {
        gtk_window_destroy(GTK_WINDOW(dlg));
    }
}
static gboolean on_settings_key_pressed(GtkEventControllerKey *ctrl,
                                        guint keyval,
                                        guint keycode,
                                        GdkModifierType mods,
                                        gpointer user_data)
{
    (void)ctrl;
    (void)keycode;
    (void)mods;
    if (keyval == GDK_KEY_Escape)
    {
        GtkWidget *dlg = GTK_WIDGET(user_data);
        if (dlg && GTK_IS_WINDOW(dlg))
        {
            gtk_window_destroy(GTK_WINDOW(dlg));
        }
        return TRUE;
    }
    return FALSE;
}
static GtkWidget *add_settings_switch_row(GtkWidget *parent,
                                          const char *title_text,
                                          const char *sub_text,
                                          gboolean initial_val,
                                          GCallback toggle_cb,
                                          AppState *state)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(row, "settings-row");
    GtkWidget *txt_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(txt_box, TRUE);
    GtkWidget *t_lbl = gtk_label_new(title_text);
    gtk_widget_add_css_class(t_lbl, "settings-row-title");
    gtk_widget_set_halign(t_lbl, GTK_ALIGN_START);
    GtkWidget *s_lbl = gtk_label_new(sub_text);
    gtk_widget_add_css_class(s_lbl, "settings-row-sub");
    gtk_widget_set_halign(s_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(txt_box), t_lbl);
    gtk_box_append(GTK_BOX(txt_box), s_lbl);
    GtkWidget *sw = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw), initial_val);
    gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
    g_signal_connect(sw, "state-set", toggle_cb, state);
    gtk_box_append(GTK_BOX(row), txt_box);
    gtk_box_append(GTK_BOX(row), sw);
    gtk_box_append(GTK_BOX(parent), row);
    return sw;
}
void show_settings_dialog(AppState *state)
{
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Preferences");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 550, 390);
    gtk_widget_add_css_class(dialog, "settings-modal-window");
    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_settings_key_pressed), dialog);
    gtk_widget_add_controller(dialog, key_ctrl);
    GtkWidget *overlay = gtk_overlay_new();
    GtkWidget *bg_canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(bg_canvas), on_draw_modal_squircle, NULL, NULL);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), bg_canvas);
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_add_css_class(card, "squircle-modal-inner");
    /* Draggable Squircle Modal Header */
    GtkWidget *handle = gtk_window_handle_new();
    GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(hdr, "settings-modal-header");
    GtkWidget *hdr_icon = gtk_image_new_from_icon_name("preferences-system-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(hdr_icon), 18);
    gtk_widget_add_css_class(hdr_icon, "settings-header-icon");
    GtkWidget *title_lbl = gtk_label_new("Preferences");
    gtk_widget_add_css_class(title_lbl, "settings-modal-title");
    gtk_widget_set_hexpand(title_lbl, TRUE);
    gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);
    GtkWidget *close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(close_btn, "win-ctrl-btn");
    gtk_widget_set_size_request(close_btn, 26, 26);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_settings_close_clicked), dialog);
    gtk_box_append(GTK_BOX(hdr), hdr_icon);
    gtk_box_append(GTK_BOX(hdr), title_lbl);
    gtk_box_append(GTK_BOX(hdr), close_btn);
    gtk_window_handle_set_child(GTK_WINDOW_HANDLE(handle), hdr);
    gtk_box_append(GTK_BOX(card), handle);
    /* Sort Items By Row (Applies to Icon Grid & List View) */
    GtkWidget *sort_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(sort_row, "settings-row");
    GtkWidget *sort_txt_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(sort_txt_box, TRUE);
    GtkWidget *sort_title = gtk_label_new("Sort Items By");
    gtk_widget_add_css_class(sort_title, "settings-row-title");
    gtk_widget_set_halign(sort_title, GTK_ALIGN_START);
    GtkWidget *sort_sub = gtk_label_new("Order files & folders in Icon and List views");
    gtk_widget_add_css_class(sort_sub, "settings-row-sub");
    gtk_widget_set_halign(sort_sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(sort_txt_box), sort_title);
    gtk_box_append(GTK_BOX(sort_txt_box), sort_sub);
    const char *sort_options[] = {"Name", "Type", "Date Modified", "Size", NULL};
    GtkWidget *sort_dropdown = gtk_drop_down_new_from_strings(sort_options);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(sort_dropdown), (guint)state->sort_column);
    gtk_widget_set_valign(sort_dropdown, GTK_ALIGN_CENTER);
    g_signal_connect(sort_dropdown, "notify::selected", G_CALLBACK(on_settings_sort_col_changed), state);
    GtkWidget *order_btn = gtk_toggle_button_new_with_label(
        state->sort_ascending ? "Ascending ▲" : "Descending ▼");
    gtk_widget_add_css_class(order_btn, "modal-secondary-btn");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(order_btn), state->sort_ascending);
    gtk_widget_set_valign(order_btn, GTK_ALIGN_CENTER);
    g_signal_connect(order_btn, "toggled", G_CALLBACK(on_settings_sort_order_toggled), state);
    gtk_box_append(GTK_BOX(sort_row), sort_txt_box);
    gtk_box_append(GTK_BOX(sort_row), sort_dropdown);
    gtk_box_append(GTK_BOX(sort_row), order_btn);
    gtk_box_append(GTK_BOX(card), sort_row);
    /* Behavior & Display Switches */
    add_settings_switch_row(card,
                            "Show Hidden Files",
                            "Display dotfiles and hidden directories (Ctrl+H)",
                            state->show_hidden,
                            G_CALLBACK(on_settings_hidden_toggled),
                            state);
    add_settings_switch_row(card,
                            "Image Thumbnails",
                            "Generate async preview thumbnails for image files",
                            state->enable_thumbnails,
                            G_CALLBACK(on_settings_thumbs_toggled),
                            state);
    add_settings_switch_row(card,
                            "Double-Click to Open",
                            "Single-click selects items; double-click opens them",
                            state->double_click_open,
                            G_CALLBACK(on_settings_dblclick_toggled),
                            state);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), card);
    gtk_window_set_child(GTK_WINDOW(dialog), overlay);
    gtk_window_present(GTK_WINDOW(dialog));
}