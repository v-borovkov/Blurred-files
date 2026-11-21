#include "app.h"
#include <string.h>

typedef struct
{
    AppState *state;
    char *full_path;
    char *display_name;
    gboolean is_dir;
    GtkWidget *card_widget;
} ItemData;

static void free_item_data(gpointer data, GClosure *closure)
{
    (void)closure;
    ItemData *id = (ItemData *)data;
    g_free(id->full_path);
    g_free(id->display_name);
    g_free(id);
}

static void destroy_item_data(gpointer data)
{
    free_item_data(data, NULL);
}

static gboolean is_supported_archive_name(const char *name)
{
    if (!name)
        return FALSE;
    return g_str_has_suffix(name, ".zip") ||
           g_str_has_suffix(name, ".tar.gz") ||
           g_str_has_suffix(name, ".tgz") ||
           g_str_has_suffix(name, ".tar.xz") ||
           g_str_has_suffix(name, ".tar.bz2") ||
           g_str_has_suffix(name, ".tar");
}

static void open_item_action(ItemData *id)
{
    if (id->is_dir)
    {
        if (id->state->is_desktop_mode)
        {
            const char *exe = g_file_test("./build/blurred_files", G_FILE_TEST_IS_EXECUTABLE)
                                  ? "./build/blurred_files"
                                  : "blurred_files";
            const char *argv[] = {exe, id->full_path, NULL};
            g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
        }
        else
        {
            navigate_to_path(id->state, id->full_path, TRUE);
        }
        return;
    }

    char *uri = g_filename_to_uri(id->full_path, NULL, NULL);
    GError *err = NULL;

    if (uri && g_app_info_launch_default_for_uri(uri, NULL, &err))
    {
        g_free(uri);
        return;
    }

    if (err)
    {
        g_error_free(err);
    }
    g_free(uri);

    const char *argv[] = {"xdg-open", id->full_path, NULL};
    g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

/* --- Context Menu Callbacks --- */
static void on_ctx_open_clicked(GtkButton *btn, gpointer user_data)
{
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    open_item_action((ItemData *)user_data);
}

static void on_ctx_open_with_clicked(GtkButton *btn, gpointer user_data)
{
    ItemData *id = (ItemData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    show_open_with_dialog(id->state, id->full_path);
}

static void on_ctx_open_terminal_clicked(GtkButton *btn, gpointer user_data)
{
    ItemData *id = (ItemData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    open_directory_in_terminal(id->full_path);
}

static void on_ctx_pin_toggle_clicked(GtkButton *btn, gpointer user_data)
{
    ItemData *id = (ItemData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    toggle_pin_folder(id->state, id->full_path);
}

static void on_ctx_rename_clicked(GtkButton *btn, gpointer user_data)
{
    ItemData *id = (ItemData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    show_rename_dialog(id->state, id->full_path, id->display_name);
}

static void on_ctx_copy_clicked(GtkButton *btn, gpointer user_data)
{
    ItemData *id = (ItemData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    bulk_copy_selected_items(id->state, FALSE);
}

static void on_ctx_cut_clicked(GtkButton *btn, gpointer user_data)
{
    ItemData *id = (ItemData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    bulk_copy_selected_items(id->state, TRUE);
}

static void on_ctx_compress_zip_clicked(GtkButton *btn, gpointer user_data)
{
    ItemData *id = (ItemData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    bulk_compress_selected_items(id->state, FALSE);
}

static void on_ctx_compress_targz_clicked(GtkButton *btn, gpointer user_data)
{
    ItemData *id = (ItemData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    bulk_compress_selected_items(id->state, TRUE);
}

static void on_ctx_extract_clicked(GtkButton *btn, gpointer user_data)
{
    ItemData *id = (ItemData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    bulk_extract_selected_archives(id->state);
}

static void on_ctx_restore_clicked(GtkButton *btn, gpointer user_data)
{
    ItemData *id = (ItemData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    bulk_restore_selected_trash_items(id->state);
}

static void on_ctx_trash_clicked(GtkButton *btn, gpointer user_data)
{
    ItemData *id = (ItemData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    bulk_trash_selected_items(id->state);
}

static void on_ctx_properties_clicked(GtkButton *btn, gpointer user_data)
{
    ItemData *id = (ItemData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    show_properties_dialog(id->state, id->full_path);
}

static GtkWidget *make_menu_btn(const char *label, gboolean is_danger, GCallback cb, gpointer data)
{
    GtkWidget *btn = gtk_button_new_with_label(label);
    gtk_widget_add_css_class(btn, "context-menu-btn");
    if (is_danger)
        gtk_widget_add_css_class(btn, "danger");
    g_signal_connect(btn, "clicked", cb, data);
    return btn;
}

static void on_ctx_open_new_window_clicked(GtkButton *btn, gpointer user_data)
{
    ItemData *id = (ItemData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));

    if (!id || !id->is_dir || !id->full_path)
        return;

    const char *exe = g_file_test("./build/blurred_files", G_FILE_TEST_IS_EXECUTABLE)
                          ? "./build/blurred_files"
                          : "blurred_files";
    const char *argv[] = {exe, id->full_path, NULL};
    g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

static void on_item_right_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    (void)gesture;
    (void)n_press;
    ItemData *id = (ItemData *)user_data;

    if (!gtk_widget_has_css_class(id->card_widget, "selected"))
    {
        clear_item_selection(id->state);
        gtk_widget_add_css_class(id->card_widget, "selected");
        update_status_bar(id->state);
    }

    GList *sel_list = get_selected_paths(id->state);
    guint sel_count = g_list_length(sel_list);
    gboolean any_archive = FALSE;
    for (GList *l = sel_list; l != NULL; l = l->next)
    {
        if (is_supported_archive_name((const char *)l->data))
        {
            any_archive = TRUE;
            break;
        }
    }
    g_list_free_full(sel_list, g_free);

    gboolean in_trash = is_current_dir_trash(id->state);

    GtkWidget *popover = gtk_popover_new();
    gtk_widget_set_parent(popover, id->card_widget);
    g_signal_connect(popover, "closed", G_CALLBACK(gtk_widget_unparent), NULL);
    gtk_widget_add_css_class(popover, "context-popover");

    GdkRectangle rect = {(int)x, (int)y, 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);

    GtkWidget *menu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(menu_box, "context-menu-box");

    if (in_trash)
    {
        char *restore_lbl = (sel_count > 1) ? g_strdup_printf("Restore %u Items", sel_count) : g_strdup("Restore");
        char *del_lbl = (sel_count > 1) ? g_strdup_printf("Delete %u Permanently", sel_count) : g_strdup("Delete Permanently");
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn(restore_lbl, FALSE, G_CALLBACK(on_ctx_restore_clicked), id));
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn(del_lbl, TRUE, G_CALLBACK(on_ctx_trash_clicked), id));
        g_free(restore_lbl);
        g_free(del_lbl);
    }
    else if (sel_count > 1)
    {
        char *copy_lbl = g_strdup_printf("Copy %u Items", sel_count);
        char *cut_lbl = g_strdup_printf("Cut %u Items", sel_count);
        char *trash_lbl = g_strdup_printf("Move %u Items to Trash", sel_count);

        gtk_box_append(GTK_BOX(menu_box), make_menu_btn(copy_lbl, FALSE, G_CALLBACK(on_ctx_copy_clicked), id));
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn(cut_lbl, FALSE, G_CALLBACK(on_ctx_cut_clicked), id));
        if (any_archive)
        {
            gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Extract Here", FALSE, G_CALLBACK(on_ctx_extract_clicked), id));
        }
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Compress to .zip", FALSE, G_CALLBACK(on_ctx_compress_zip_clicked), id));
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Compress to .tar.gz", FALSE, G_CALLBACK(on_ctx_compress_targz_clicked), id));
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn(trash_lbl, TRUE, G_CALLBACK(on_ctx_trash_clicked), id));

        g_free(copy_lbl);
        g_free(cut_lbl);
        g_free(trash_lbl);
    }
    else
    {
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Open", FALSE, G_CALLBACK(on_ctx_open_clicked), id));
        if (id->is_dir)
        {
            if (!id->state->is_desktop_mode)
            {
                gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Open in New Window", FALSE, G_CALLBACK(on_ctx_open_new_window_clicked), id));
            }
            gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Open in Terminal", FALSE, G_CALLBACK(on_ctx_open_terminal_clicked), id));
            if (!id->state->is_desktop_mode)
            {
                const char *pin_lbl = is_folder_pinned(id->state, id->full_path)
                                          ? "Unpin from Sidebar"
                                          : "Pin to Sidebar";

                gtk_box_append(GTK_BOX(menu_box), make_menu_btn(pin_lbl, FALSE, G_CALLBACK(on_ctx_pin_toggle_clicked), id));
            }
        }
        else
        {
            gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Open With...", FALSE, G_CALLBACK(on_ctx_open_with_clicked), id));
        }
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Copy", FALSE, G_CALLBACK(on_ctx_copy_clicked), id));
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Cut", FALSE, G_CALLBACK(on_ctx_cut_clicked), id));
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Rename", FALSE, G_CALLBACK(on_ctx_rename_clicked), id));
        if (any_archive)
        {
            gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Extract Here", FALSE, G_CALLBACK(on_ctx_extract_clicked), id));
        }
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Compress to .zip", FALSE, G_CALLBACK(on_ctx_compress_zip_clicked), id));
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Compress to .tar.gz", FALSE, G_CALLBACK(on_ctx_compress_targz_clicked), id));
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Move to Trash", TRUE, G_CALLBACK(on_ctx_trash_clicked), id));
        gtk_box_append(GTK_BOX(menu_box), make_menu_btn("Properties", FALSE, G_CALLBACK(on_ctx_properties_clicked), id));
    }
    gtk_popover_set_child(GTK_POPOVER(popover), menu_box);
    gtk_popover_popup(GTK_POPOVER(popover));
}
/* --- Drag Source & Folder Drop Target --- */
static GdkContentProvider *on_drag_prepare(GtkDragSource *source, double x, double y, gpointer user_data)
{
    (void)x;
    (void)y;
    ItemData *id = (ItemData *)user_data;
    if (!gtk_widget_has_css_class(id->card_widget, "selected"))
    {
        clear_item_selection(id->state);
        gtk_widget_add_css_class(id->card_widget, "selected");
        update_status_bar(id->state);
    }
    GList *sel_paths = get_selected_paths(id->state);
    GSList *gfiles = NULL;
    for (GList *l = sel_paths; l != NULL; l = l->next)
    {
        gfiles = g_slist_prepend(gfiles, g_file_new_for_path((const char *)l->data));
    }
    gfiles = g_slist_reverse(gfiles);
    g_list_free_full(sel_paths, g_free);
    GtkIconTheme *theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
    const char *icon_name = id->is_dir ? "folder" : "text-x-generic";
    GtkIconPaintable *paintable = gtk_icon_theme_lookup_icon(
        theme,
        icon_name,
        NULL,
        48,
        1,
        GTK_TEXT_DIR_NONE,
        0);
    if (paintable)
    {
        gtk_drag_source_set_icon(source, GDK_PAINTABLE(paintable), 24, 24);
        g_object_unref(paintable);
    }
    GdkFileList *file_list = gdk_file_list_new_from_list(gfiles);
    GdkContentProvider *provider = gdk_content_provider_new_typed(GDK_TYPE_FILE_LIST, file_list);
    g_boxed_free(GDK_TYPE_FILE_LIST, file_list);
    g_slist_free_full(gfiles, g_object_unref);
    return provider;
}
static gboolean on_folder_drop(GtkDropTarget *target, const GValue *value, double x, double y, gpointer user_data)
{
    (void)target;
    (void)x;
    (void)y;
    ItemData *dest_folder = (ItemData *)user_data;
    gtk_widget_remove_css_class(dest_folder->card_widget, "drop-target-hover");
    if (!dest_folder->is_dir)
        return FALSE;
    return move_dropped_files_to_directory(dest_folder->state, value, dest_folder->full_path, FALSE);
}
static GdkDragAction on_folder_drop_enter(GtkDropTarget *target, double x, double y, gpointer user_data)
{
    (void)target;
    (void)x;
    (void)y;
    ItemData *dest_folder = (ItemData *)user_data;
    gtk_widget_add_css_class(dest_folder->card_widget, "drop-target-hover");
    return GDK_ACTION_MOVE;
}
static void on_folder_drop_leave(GtkDropTarget *target, gpointer user_data)
{
    (void)target;
    ItemData *dest_folder = (ItemData *)user_data;
    gtk_widget_remove_css_class(dest_folder->card_widget, "drop-target-hover");
}
/* --- Unified Primary Click Handler (Single-Click, Double-Click, Ctrl+Click) --- */
static void on_item_primary_pressed(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    (void)x;
    (void)y;
    ItemData *id = (ItemData *)user_data;
    GdkModifierType mods = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(gesture));
    if ((mods & GDK_CONTROL_MASK) != 0)
    {
        if (gtk_widget_has_css_class(id->card_widget, "selected"))
        {
            gtk_widget_remove_css_class(id->card_widget, "selected");
        }
        else
        {
            gtk_widget_add_css_class(id->card_widget, "selected");
        }
        update_status_bar(id->state);
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
        return;
    }
    if (id->state->double_click_open)
    {
        if (n_press == 1)
        {
            clear_item_selection(id->state);
            gtk_widget_add_css_class(id->card_widget, "selected");
            update_status_bar(id->state);
        }
        else if (n_press == 2)
        {
            gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
            open_item_action(id);
        }
    }
    else
    {
        if (n_press == 1)
        {
            gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
            open_item_action(id);
        }
    }
}
/* --- Nautilus-Style 5-Tier Discrete Zoom Metrics --- */
static const int ZOOM_ICON_LEVELS[] = {36, 48, 68, 84, 108};
static const int ZOOM_CARD_WIDTHS[] = {76, 96, 144, 168, 196};
static const int ZOOM_MAX_CHARS[] = {8, 10, 14, 16, 19};
static void get_nautilus_zoom_metrics(AppState *state,
                                      int *out_card_w,
                                      int *out_icon_px,
                                      int *out_slot_w,
                                      int *out_slot_h,
                                      int *out_max_chars)
{
    int tier = CLAMP(state->grid_cols, 0, 4);
    int icon_px = ZOOM_ICON_LEVELS[tier];
    int card_w = ZOOM_CARD_WIDTHS[tier];
    int max_chars = ZOOM_MAX_CHARS[tier];
    int slot_w = card_w - 16;
    int slot_h = (int)(icon_px * 1.05);
    if (out_card_w)
        *out_card_w = card_w;
    if (out_icon_px)
        *out_icon_px = icon_px;
    if (out_slot_w)
        *out_slot_w = slot_w;
    if (out_slot_h)
        *out_slot_h = slot_h;
    if (out_max_chars)
        *out_max_chars = max_chars;
}
void apply_grid_column_zoom(AppState *state)
{
    if (!state || !state->flowbox || !GTK_IS_FLOW_BOX(state->flowbox) || state->is_list_view)
        return;
    int card_w, base_icon, slot_w, slot_h, max_chars;
    get_nautilus_zoom_metrics(state, &card_w, &base_icon, &slot_w, &slot_h, &max_chars);
    if (state->is_desktop_mode)
    {
        gtk_orientable_set_orientation(GTK_ORIENTABLE(state->flowbox), GTK_ORIENTATION_VERTICAL);
        gtk_widget_set_halign(state->flowbox, GTK_ALIGN_START);
        gtk_widget_set_valign(state->flowbox, GTK_ALIGN_FILL);
        gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(state->flowbox), TRUE);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(state->flowbox), 1);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(state->flowbox), 12);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(state->flowbox), 16);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(state->flowbox), 14);
    }
    else
    {
        gtk_orientable_set_orientation(GTK_ORIENTABLE(state->flowbox), GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_halign(state->flowbox, GTK_ALIGN_FILL);
        gtk_widget_set_valign(state->flowbox, GTK_ALIGN_START);
        gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(state->flowbox), TRUE);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(state->flowbox), 1);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(state->flowbox), 20);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(state->flowbox), 12);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(state->flowbox), 12);
    }
    GtkIconTheme *theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
    for (GtkWidget *child = gtk_widget_get_first_child(state->flowbox);
         child != NULL;
         child = gtk_widget_get_next_sibling(child))
    {
        if (!GTK_IS_FLOW_BOX_CHILD(child))
            continue;
        GtkWidget *btn = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(child));
        if (!btn || !GTK_IS_BUTTON(btn))
            continue;
        gtk_widget_set_halign(btn, GTK_ALIGN_CENTER);
        gtk_widget_set_size_request(btn, card_w, -1);
        GtkWidget *slot = g_object_get_data(G_OBJECT(btn), "visual-slot");
        GtkWidget *lbl = g_object_get_data(G_OBJECT(btn), "label-widget");
        GtkWidget *visual_child = (slot && GTK_IS_WIDGET(slot)) ? gtk_widget_get_first_child(slot) : NULL;
        if (slot && GTK_IS_WIDGET(slot))
        {
            gtk_widget_set_size_request(slot, slot_w, slot_h);
            gtk_widget_set_overflow(slot, GTK_OVERFLOW_HIDDEN);
        }
        if (visual_child && GTK_IS_IMAGE(visual_child))
        {
            GIcon *gicon = g_object_get_data(G_OBJECT(btn), "gicon");
            if (gicon)
            {
                int scale = gtk_widget_get_scale_factor(visual_child);
                GtkIconPaintable *paintable = gtk_icon_theme_lookup_by_gicon(
                    theme, gicon, base_icon, scale, GTK_TEXT_DIR_NONE, 0);
                if (paintable)
                {
                    gtk_image_set_from_paintable(GTK_IMAGE(visual_child), GDK_PAINTABLE(paintable));
                    g_object_unref(paintable);
                }
            }
            gtk_image_set_pixel_size(GTK_IMAGE(visual_child), base_icon);
            gtk_widget_set_size_request(visual_child, base_icon, base_icon);
        }
        else if (visual_child && GTK_IS_PICTURE(visual_child))
        {
            gtk_picture_set_can_shrink(GTK_PICTURE(visual_child), TRUE);
            gtk_widget_set_size_request(visual_child, -1, -1);
            gtk_widget_set_hexpand(visual_child, TRUE);
            gtk_widget_set_vexpand(visual_child, TRUE);
        }
        if (lbl && GTK_IS_LABEL(lbl))
        {
            gtk_label_set_max_width_chars(GTK_LABEL(lbl), max_chars);
        }
    }
    gtk_widget_queue_resize(state->flowbox);
}
/* --- Card & List Row Construction --- */
static char *format_file_type_label(GFileInfo *info, gboolean is_dir)
{
    if (is_dir)
        return g_strdup("Folder");
    const char *ctype = g_file_info_get_content_type(info);
    if (!ctype)
        return g_strdup("File");
    char *desc = g_content_type_get_description(ctype);
    if (desc && *desc != '\0')
        return desc;
    g_free(desc);
    return g_strdup(ctype);
}
static char *format_modified_date(GFileInfo *info)
{
    GDateTime *dt = g_file_info_get_modification_date_time(info);
    if (!dt)
        return g_strdup("—");
    GDateTime *local = g_date_time_to_local(dt);
    char *formatted = g_date_time_format(local, "%b %d, %Y  %H:%M");
    g_date_time_unref(local);
    g_date_time_unref(dt);
    return formatted ? formatted : g_strdup("—");
}
GtkWidget *create_real_file_item(AppState *state, GFileInfo *info, const char *parent_path)
{
    const char *name = g_file_info_get_display_name(info);
    GFileType type = g_file_info_get_file_type(info);
    gboolean is_dir = (type == G_FILE_TYPE_DIRECTORY);
    char *full_path = g_build_filename(parent_path, name, NULL);
    const char *content_type = g_file_info_get_content_type(info);
    goffset file_size = g_file_info_get_size(info);
    GtkWidget *btn = gtk_button_new();
    gtk_widget_add_css_class(btn, "file-card-btn");
    if (state->is_desktop_mode)
    {
        gtk_widget_add_css_class(btn, "desktop-file-card");
    }
    gtk_widget_set_cursor_from_name(btn, "pointer");
    g_object_set_data_full(G_OBJECT(btn), "full-path", g_strdup(full_path), g_free);
    GIcon *gicon = g_file_info_get_icon(info);
    if (gicon)
    {
        g_object_set_data_full(G_OBJECT(btn), "gicon", g_object_ref(gicon), g_object_unref);
    }
    gboolean can_thumb = (!is_dir && state->enable_thumbnails && content_type &&
                          g_str_has_prefix(content_type, "image/") &&
                          file_size > 0 && file_size < (goffset)200 * 1024 * 1024);
    if (state->is_list_view)
    {
        gtk_widget_set_hexpand(btn, TRUE);
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
        gtk_widget_add_css_class(row, "file-list-row");
        GtkWidget *visual_slot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(visual_slot, 28, 28);
        gtk_widget_set_overflow(visual_slot, GTK_OVERFLOW_HIDDEN);
        gtk_widget_set_valign(visual_slot, GTK_ALIGN_CENTER);
        GtkWidget *icon = gicon
                              ? gtk_image_new_from_gicon(gicon)
                              : gtk_image_new_from_icon_name(is_dir ? "folder" : "text-x-generic");
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
        gtk_box_append(GTK_BOX(visual_slot), icon);
        if (can_thumb)
        {
            queue_async_thumbnail(state, visual_slot, full_path, 56, 56);
        }
        GtkWidget *name_lbl = gtk_label_new(name);
        gtk_widget_add_css_class(name_lbl, "file-list-label");
        gtk_widget_set_hexpand(name_lbl, TRUE);
        gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(name_lbl), PANGO_ELLIPSIZE_END);
        char *type_str = format_file_type_label(info, is_dir);
        GtkWidget *type_lbl = gtk_label_new(type_str);
        gtk_widget_add_css_class(type_lbl, "file-list-col-type");
        gtk_widget_set_size_request(type_lbl, 130, -1);
        gtk_widget_set_halign(type_lbl, GTK_ALIGN_START);
        gtk_label_set_ellipsize(GTK_LABEL(type_lbl), PANGO_ELLIPSIZE_END);
        g_free(type_str);
        char *date_str = format_modified_date(info);
        GtkWidget *date_lbl = gtk_label_new(date_str);
        gtk_widget_add_css_class(date_lbl, "file-list-col-date");
        gtk_widget_set_size_request(date_lbl, 145, -1);
        gtk_widget_set_halign(date_lbl, GTK_ALIGN_START);
        g_free(date_str);
        char *size_str = is_dir ? g_strdup("—") : g_format_size((guint64)file_size);
        GtkWidget *size_lbl = gtk_label_new(size_str);
        gtk_widget_add_css_class(size_lbl, "file-list-col-size");
        gtk_widget_set_size_request(size_lbl, 75, -1);
        gtk_widget_set_halign(size_lbl, GTK_ALIGN_END);
        g_free(size_str);
        gtk_box_append(GTK_BOX(row), visual_slot);
        gtk_box_append(GTK_BOX(row), name_lbl);
        gtk_box_append(GTK_BOX(row), type_lbl);
        gtk_box_append(GTK_BOX(row), date_lbl);
        gtk_box_append(GTK_BOX(row), size_lbl);
        gtk_button_set_child(GTK_BUTTON(btn), row);
    }
    else
    {
        int card_w, base_icon, slot_w, slot_h, max_chars;
        get_nautilus_zoom_metrics(state, &card_w, &base_icon, &slot_w, &slot_h, &max_chars);
        gtk_widget_set_halign(btn, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(btn, GTK_ALIGN_START);
        gtk_widget_set_size_request(btn, card_w, -1);
        GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_add_css_class(card, "file-card");
        gtk_widget_set_halign(card, GTK_ALIGN_CENTER);
        GtkWidget *visual_slot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(visual_slot, slot_w, slot_h);
        gtk_widget_set_overflow(visual_slot, GTK_OVERFLOW_HIDDEN);
        gtk_widget_set_halign(visual_slot, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(visual_slot, GTK_ALIGN_CENTER);
        GtkWidget *icon = gicon
                              ? gtk_image_new_from_gicon(gicon)
                              : gtk_image_new_from_icon_name(is_dir ? "folder" : "text-x-generic");
        gtk_image_set_pixel_size(GTK_IMAGE(icon), base_icon);
        gtk_widget_set_size_request(icon, base_icon, base_icon);
        gtk_box_append(GTK_BOX(visual_slot), icon);
        if (can_thumb)
        {
            queue_async_thumbnail(state, visual_slot, full_path, 180, 140);
        }
        GtkWidget *label = gtk_label_new(name);
        gtk_widget_add_css_class(label, "file-label");
        if (state->is_desktop_mode)
        {
            gtk_widget_add_css_class(label, "desktop-file-label");
        }
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(label), max_chars);
        gtk_box_append(GTK_BOX(card), visual_slot);
        gtk_box_append(GTK_BOX(card), label);
        gtk_button_set_child(GTK_BUTTON(btn), card);
        g_object_set_data(G_OBJECT(btn), "visual-slot", visual_slot);
        g_object_set_data(G_OBJECT(btn), "label-widget", label);
    }
    ItemData *id = g_new0(ItemData, 1);
    id->state = state;
    id->full_path = full_path;
    id->display_name = g_strdup(name);
    id->is_dir = is_dir;
    id->card_widget = btn;
    GtkGesture *primary_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(primary_click), GDK_BUTTON_PRIMARY);
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(primary_click), GTK_PHASE_CAPTURE);
    g_signal_connect(primary_click, "pressed", G_CALLBACK(on_item_primary_pressed), id);
    gtk_widget_add_controller(btn, GTK_EVENT_CONTROLLER(primary_click));
    GtkGesture *rclick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rclick), GDK_BUTTON_SECONDARY);
    g_signal_connect(rclick, "pressed", G_CALLBACK(on_item_right_clicked), id);
    gtk_widget_add_controller(btn, GTK_EVENT_CONTROLLER(rclick));
    GtkDragSource *drag_src = gtk_drag_source_new();
    gtk_drag_source_set_actions(drag_src, GDK_ACTION_MOVE | GDK_ACTION_COPY);
    g_signal_connect(drag_src, "prepare", G_CALLBACK(on_drag_prepare), id);
    gtk_widget_add_controller(btn, GTK_EVENT_CONTROLLER(drag_src));
    if (is_dir)
    {
        GType drop_types[] = {GDK_TYPE_FILE_LIST, G_TYPE_FILE};
        GtkDropTarget *drop_tgt = gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_MOVE | GDK_ACTION_COPY);
        gtk_drop_target_set_gtypes(drop_tgt, drop_types, G_N_ELEMENTS(drop_types));
        g_signal_connect(drop_tgt, "drop", G_CALLBACK(on_folder_drop), id);
        g_signal_connect(drop_tgt, "enter", G_CALLBACK(on_folder_drop_enter), id);
        g_signal_connect(drop_tgt, "leave", G_CALLBACK(on_folder_drop_leave), id);
        gtk_widget_add_controller(btn, GTK_EVENT_CONTROLLER(drop_tgt));
    }
    g_object_set_data_full(G_OBJECT(btn), "item-data", id, destroy_item_data);
    return btn;
}