#include "app.h"
#include <string.h>

typedef struct
{
    AppState *state;
    char *target_path;
    GVolume *volume;
    GMount *mount;
} SidebarBtnData;


/* --- Pinned Folders Helpers --- */
gboolean is_folder_pinned(AppState *state, const char *path)
{
    if (!state || !path)
        return FALSE;
    for (GList *l = state->pinned_folders; l != NULL; l = l->next)
    {
        if (g_strcmp0((const char *)l->data, path) == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

void toggle_pin_folder(AppState *state, const char *path)
{
    if (!state || !path)
        return;

    for (GList *l = state->pinned_folders; l != NULL; l = l->next)
    {
        if (g_strcmp0((const char *)l->data, path) == 0)
        {
            g_free(l->data);
            state->pinned_folders = g_list_delete_link(state->pinned_folders, l);
            save_app_settings(state);
            rebuild_sidebar_places(state);
            return;
        }
    }

    state->pinned_folders = g_list_append(state->pinned_folders, g_strdup(path));
    save_app_settings(state);
    rebuild_sidebar_places(state);
}

/* --- Wayland-Safe Async Volume Mount & Unmount --- */
static void on_volume_mount_finished(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GVolume *vol = G_VOLUME(source_object);
    AppState *state = (AppState *)user_data;
    GError *err = NULL;

    if (g_volume_mount_finish(vol, res, &err))
    {
        GMount *mount = g_volume_get_mount(vol);
        if (mount)
        {
            GFile *root = g_mount_get_root(mount);
            char *mount_path = g_file_get_path(root);
            if (mount_path && state && GTK_IS_WINDOW(state->window))
            {
                navigate_to_path(state, mount_path, TRUE);
            }
            g_free(mount_path);
            if (root)
                g_object_unref(root);
            g_object_unref(mount);
        }
    }
    else if (err)
    {
        if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            g_warning("Volume mount failed: %s", err->message);
        }
        g_error_free(err);
    }
}

static void on_mount_unmount_finished(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GMount *mount = G_MOUNT(source_object);
    AppState *state = (AppState *)user_data;
    GError *err = NULL;

    if (g_mount_unmount_with_operation_finish(mount, res, &err))
    {
        if (state && state->current_path && !g_file_test(state->current_path, G_FILE_TEST_EXISTS))
        {
            navigate_to_path(state, g_get_home_dir(), TRUE);
        }
    }
    else if (err)
    {
        if (!g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
            g_warning("Unmount failed: %s", err->message);
        }
        g_error_free(err);
    }
}

static void on_sidebar_item_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    SidebarBtnData *sbd = (SidebarBtnData *)user_data;
    if (!sbd || !sbd->state)
        return;

    if (sbd->target_path)
    {
        if (g_file_test(sbd->target_path, G_FILE_TEST_IS_DIR))
        {
            navigate_to_path(sbd->state, sbd->target_path, TRUE);
        }
        return;
    }

    if (sbd->volume)
    {
        GMount *mount = g_volume_get_mount(sbd->volume);
        if (mount)
        {
            GFile *root = g_mount_get_root(mount);
            char *mount_path = g_file_get_path(root);
            if (mount_path)
            {
                navigate_to_path(sbd->state, mount_path, TRUE);
                g_free(mount_path);
            }
            if (root)
                g_object_unref(root);
            g_object_unref(mount);
        }
        else
        {
            GMountOperation *mount_op = gtk_mount_operation_new(NULL);
            g_volume_mount(sbd->volume,
                           G_MOUNT_MOUNT_NONE,
                           mount_op,
                           NULL,
                           on_volume_mount_finished,
                           sbd->state);
            g_object_unref(mount_op);
        }
    }
}

/* --- Right-Click Context Menu for Pinned Folders & Mounted Drives --- */
static void on_unpin_menu_clicked(GtkButton *btn, gpointer user_data)
{
    SidebarBtnData *sbd = (SidebarBtnData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    if (sbd && sbd->target_path)
    {
        toggle_pin_folder(sbd->state, sbd->target_path);
    }
}

static void on_unmount_menu_clicked(GtkButton *btn, gpointer user_data)
{
    SidebarBtnData *sbd = (SidebarBtnData *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));

    if (sbd && sbd->mount)
    {
        GMountOperation *op = gtk_mount_operation_new(NULL);
        g_mount_unmount_with_operation(sbd->mount,
                                       G_MOUNT_UNMOUNT_NONE,
                                       op,
                                       NULL,
                                       on_mount_unmount_finished,
                                       sbd->state);
        g_object_unref(op);
    }
}

static void on_sidebar_item_right_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    (void)n_press;
    SidebarBtnData *sbd = (SidebarBtnData *)user_data;
    if (!sbd || !sbd->state)
        return;

    gboolean is_pinned = (sbd->target_path && is_folder_pinned(sbd->state, sbd->target_path));
    gboolean can_unmount = (sbd->mount != NULL && g_mount_can_unmount(sbd->mount));

    if (!is_pinned && !can_unmount)
        return;

    GtkWidget *btn = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    GtkWidget *popover = gtk_popover_new();
    gtk_widget_set_parent(popover, btn);
    g_signal_connect(popover, "closed", G_CALLBACK(gtk_widget_unparent), NULL);
    gtk_widget_add_css_class(popover, "context-popover");

    GdkRectangle rect = {(int)x, (int)y, 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);

    GtkWidget *menu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(menu_box, "context-menu-box");

    if (is_pinned)
    {
        GtkWidget *btn_unpin = gtk_button_new_with_label("Unpin from Sidebar");
        gtk_widget_add_css_class(btn_unpin, "context-menu-btn");
        g_signal_connect(btn_unpin, "clicked", G_CALLBACK(on_unpin_menu_clicked), sbd);
        gtk_box_append(GTK_BOX(menu_box), btn_unpin);
    }

    if (can_unmount)
    {
        GtkWidget *btn_eject = gtk_button_new_with_label("Unmount / Eject");
        gtk_widget_add_css_class(btn_eject, "context-menu-btn");
        g_signal_connect(btn_eject, "clicked", G_CALLBACK(on_unmount_menu_clicked), sbd);
        gtk_box_append(GTK_BOX(menu_box), btn_eject);
    }

    gtk_popover_set_child(GTK_POPOVER(popover), menu_box);
    gtk_popover_popup(GTK_POPOVER(popover));
}

/* --- Drag-and-Drop onto Sidebar Bookmarks & Trash --- */
static GdkDragAction on_sidebar_drop_enter(GtkDropTarget *target, double x, double y, gpointer user_data)
{
    (void)x;
    (void)y;
    (void)user_data;
    GtkWidget *btn = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(target));
    if (btn)
        gtk_widget_add_css_class(btn, "drop-target-hover");
    return GDK_ACTION_MOVE;
}

static void on_sidebar_drop_leave(GtkDropTarget *target, gpointer user_data)
{
    (void)user_data;
    GtkWidget *btn = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(target));
    if (btn)
        gtk_widget_remove_css_class(btn, "drop-target-hover");
}

static gboolean on_sidebar_drop(GtkDropTarget *target, const GValue *value, double x, double y, gpointer user_data)
{
    (void)x;
    (void)y;
    SidebarBtnData *sbd = (SidebarBtnData *)user_data;
    GtkWidget *btn = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(target));
    if (btn)
        gtk_widget_remove_css_class(btn, "drop-target-hover");

    if (!sbd || !sbd->state || !sbd->target_path)
        return FALSE;

    char *trash_files = g_build_filename(g_get_user_data_dir(), "Trash", "files", NULL);
    gboolean to_trash = (g_strcmp0(sbd->target_path, trash_files) == 0);
    g_free(trash_files);

    return move_dropped_files_to_directory(sbd->state, value, sbd->target_path, to_trash);
}

static void destroy_sidebar_btn_data(gpointer data) {
    SidebarBtnData *sbd = (SidebarBtnData *)data;
    if (!sbd) return;
    g_free(sbd->target_path);
    if (sbd->volume) g_object_unref(sbd->volume);
    if (sbd->mount) g_object_unref(sbd->mount);
    g_free(sbd);
}

/* --- Sidebar Button Factory --- */
static GtkWidget *create_sidebar_place_button(AppState *state,
                                              const char *label_text,
                                              const char *icon_name,
                                              GIcon *gicon,
                                              const char *target_path,
                                              GVolume *volume,
                                              GMount *mount) {
    GtkWidget *btn = gtk_button_new();
    gtk_widget_add_css_class(btn, "sidebar-btn");
    gtk_widget_set_halign(btn, GTK_ALIGN_CENTER);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);

    GtkWidget *icon = gicon
        ? gtk_image_new_from_gicon(gicon)
        : gtk_image_new_from_icon_name(icon_name ? icon_name : "folder-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 20);

    GtkWidget *lbl = gtk_label_new(label_text);
    gtk_widget_add_css_class(lbl, "sidebar-label");
    gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(lbl), 11);

    gtk_box_append(GTK_BOX(vbox), icon);
    gtk_box_append(GTK_BOX(vbox), lbl);
    gtk_button_set_child(GTK_BUTTON(btn), vbox);

    SidebarBtnData *sbd = g_new0(SidebarBtnData, 1);
    sbd->state = state;
    sbd->target_path = g_strdup(target_path);
    sbd->volume = volume ? g_object_ref(volume) : NULL;
    sbd->mount = mount ? g_object_ref(mount) : NULL;

    if (target_path) {
        g_object_set_data_full(G_OBJECT(btn), "sidebar-path", g_strdup(target_path), g_free);
    }

    g_signal_connect(btn, "clicked", G_CALLBACK(on_sidebar_item_clicked), sbd);

    GtkGesture *rclick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rclick), GDK_BUTTON_SECONDARY);
    g_signal_connect(rclick, "pressed", G_CALLBACK(on_sidebar_item_right_clicked), sbd);
    gtk_widget_add_controller(btn, GTK_EVENT_CONTROLLER(rclick));

    if (target_path) {
        GType drop_types[] = { GDK_TYPE_FILE_LIST, G_TYPE_FILE };
        GtkDropTarget *drop_tgt = gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_MOVE | GDK_ACTION_COPY);
        gtk_drop_target_set_gtypes(drop_tgt, drop_types, G_N_ELEMENTS(drop_types));
        g_signal_connect(drop_tgt, "enter", G_CALLBACK(on_sidebar_drop_enter), sbd);
        g_signal_connect(drop_tgt, "leave", G_CALLBACK(on_sidebar_drop_leave), sbd);
        g_signal_connect(drop_tgt, "drop", G_CALLBACK(on_sidebar_drop), sbd);
        gtk_widget_add_controller(btn, GTK_EVENT_CONTROLLER(drop_tgt));
    }

    g_object_set_data_full(G_OBJECT(btn), "sbd-data", sbd, destroy_sidebar_btn_data);

    return btn;
}
/* --- Active Place Highlight Sync --- */
void update_sidebar_active_highlight(AppState *state)
{
    if (!state || !state->sidebar_places_box || !state->current_path)
        return;
    for (GtkWidget *child = gtk_widget_get_first_child(state->sidebar_places_box);
         child != NULL;
         child = gtk_widget_get_next_sibling(child))
    {
        const char *btn_path = g_object_get_data(G_OBJECT(child), "sidebar-path");
        if (btn_path && g_strcmp0(btn_path, state->current_path) == 0)
        {
            gtk_widget_add_css_class(child, "active");
        }
        else
        {
            gtk_widget_remove_css_class(child, "active");
        }
    }
}
/* --- Dynamic Places, Pinned Folders, Drives & Trash Rebuild --- */
void rebuild_sidebar_places(AppState *state)
{
    if (!state || !state->sidebar_places_box || !GTK_IS_BOX(state->sidebar_places_box))
        return;
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(state->sidebar_places_box)) != NULL)
    {
        gtk_box_remove(GTK_BOX(state->sidebar_places_box), child);
    }
    const char *home_dir = g_get_home_dir();
    gtk_box_append(GTK_BOX(state->sidebar_places_box),
                   create_sidebar_place_button(state, "Home", "user-home-symbolic", NULL, home_dir, NULL, NULL));
    const struct
    {
        GUserDirectory dir_enum;
        const char *fallback_name;
        const char *icon_name;
    } xdg_places[] = {
        {G_USER_DIRECTORY_DESKTOP, "Desktop", "user-desktop-symbolic"},
        {G_USER_DIRECTORY_DOCUMENTS, "Documents", "folder-documents-symbolic"},
        {G_USER_DIRECTORY_DOWNLOAD, "Downloads", "folder-download-symbolic"},
        {G_USER_DIRECTORY_PICTURES, "Pictures", "folder-pictures-symbolic"},
        {G_USER_DIRECTORY_MUSIC, "Music", "folder-music-symbolic"},
        {G_USER_DIRECTORY_VIDEOS, "Videos", "folder-videos-symbolic"}};
    for (guint i = 0; i < G_N_ELEMENTS(xdg_places); i++)
    {
        const char *p = g_get_user_special_dir(xdg_places[i].dir_enum);
        if (p && g_strcmp0(p, home_dir) != 0 && g_file_test(p, G_FILE_TEST_IS_DIR))
        {
            gtk_box_append(GTK_BOX(state->sidebar_places_box),
                           create_sidebar_place_button(state,
                                                       xdg_places[i].fallback_name,
                                                       xdg_places[i].icon_name,
                                                       NULL,
                                                       p,
                                                       NULL,
                                                       NULL));
        }
    }
    /* User-Pinned Favorite Folders (Folder Icon) */
    for (GList *l = state->pinned_folders; l != NULL; l = l->next)
    {
        const char *pin_path = (const char *)l->data;
        if (g_file_test(pin_path, G_FILE_TEST_IS_DIR))
        {
            char *base = g_path_get_basename(pin_path);
            gtk_box_append(GTK_BOX(state->sidebar_places_box),
                           create_sidebar_place_button(state, base, "folder-symbolic", NULL, pin_path, NULL, NULL));
            g_free(base);
        }
    }
    /* External Volumes & Mounted Drives */
    if (state->vol_monitor)
    {
        GList *volumes = g_volume_monitor_get_volumes(state->vol_monitor);
        for (GList *l = volumes; l != NULL; l = l->next)
        {
            GVolume *vol = G_VOLUME(l->data);
            char *vname = g_volume_get_name(vol);
            GIcon *vicon = g_volume_get_icon(vol);
            GMount *mount = g_volume_get_mount(vol);
            char *mpath = NULL;
            if (mount)
            {
                GFile *root = g_mount_get_root(mount);
                if (root)
                {
                    mpath = g_file_get_path(root);
                    g_object_unref(root);
                }
            }
            gtk_box_append(GTK_BOX(state->sidebar_places_box),
                           create_sidebar_place_button(state,
                                                       vname ? vname : "Drive",
                                                       "drive-harddisk-symbolic",
                                                       vicon,
                                                       mpath,
                                                       vol,
                                                       mount));
            g_free(mpath);
            g_free(vname);
            if (vicon)
                g_object_unref(vicon);
            if (mount)
                g_object_unref(mount);
        }
        g_list_free_full(volumes, g_object_unref);
    }
    /* Trash */
    char *trash_files = g_build_filename(g_get_user_data_dir(), "Trash", "files", NULL);
    g_mkdir_with_parents(trash_files, 0700);
    gtk_box_append(GTK_BOX(state->sidebar_places_box),
                   create_sidebar_place_button(state, "Trash", "user-trash-symbolic", NULL, trash_files, NULL, NULL));
    g_free(trash_files);
    update_sidebar_active_highlight(state);
}
/* --- Deferred GVolumeMonitor Callbacks (Prevents Wayland Event Disconnects) --- */
static gboolean deferred_rebuild_sidebar_cb(gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    if (state && state->sidebar_places_box && GTK_IS_BOX(state->sidebar_places_box))
    {
        rebuild_sidebar_places(state);
    }
    return G_SOURCE_REMOVE;
}
static void on_volume_monitor_changed(GVolumeMonitor *monitor, gpointer item, gpointer user_data)
{
    (void)monitor;
    (void)item;
    g_idle_add(deferred_rebuild_sidebar_cb, user_data);
}
/* --- Main Sidebar Widget Construction --- */
GtkWidget *build_sidebar_widget(AppState *state)
{
    GtkWidget *sidebar_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(sidebar_box, "sidebar-controls");
    gtk_widget_set_size_request(sidebar_box, 108, -1);
    GtkWidget *brand_icon = gtk_image_new_from_icon_name("system-file-manager-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(brand_icon), 26);
    gtk_widget_add_css_class(brand_icon, "sidebar-brand");
    gtk_box_append(GTK_BOX(sidebar_box), brand_icon);
    GtkWidget *sw = gtk_scrolled_window_new();
    gtk_widget_add_css_class(sw, "sidebar-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(sw, TRUE);
    GtkWidget *places_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_halign(places_box, GTK_ALIGN_CENTER);
    state->sidebar_places_box = places_box;
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sw), places_box);
    gtk_box_append(GTK_BOX(sidebar_box), sw);
    state->vol_monitor = g_volume_monitor_get();
    if (state->vol_monitor)
    {
        g_signal_connect(state->vol_monitor, "volume-added", G_CALLBACK(on_volume_monitor_changed), state);
        g_signal_connect(state->vol_monitor, "volume-removed", G_CALLBACK(on_volume_monitor_changed), state);
        g_signal_connect(state->vol_monitor, "volume-changed", G_CALLBACK(on_volume_monitor_changed), state);
        g_signal_connect(state->vol_monitor, "mount-added", G_CALLBACK(on_volume_monitor_changed), state);
        g_signal_connect(state->vol_monitor, "mount-removed", G_CALLBACK(on_volume_monitor_changed), state);
    }
    rebuild_sidebar_places(state);
    return sidebar_box;
}