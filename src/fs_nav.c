#include "app.h"
#include <string.h>

typedef struct
{
    AppState *state;
    char *target_path;
} CrumbClickData;

static void free_crumb_click_data(gpointer data, GClosure *closure)
{
    CrumbClickData *ccd = (CrumbClickData *)data;
    g_free(ccd->target_path);
    g_free(ccd);
}

/* --- Breadcrumbs --- */
static void on_crumb_clicked(GtkButton *btn, gpointer user_data)
{
    CrumbClickData *ccd = (CrumbClickData *)user_data;
    navigate_to_path(ccd->state, ccd->target_path, TRUE);
}

static gboolean close_copied_popover_cb(gpointer user_data)
{
    GtkWidget *popover = GTK_WIDGET(user_data);
    if (GTK_IS_POPOVER(popover) && gtk_widget_get_parent(popover) != NULL)
    {
        gtk_popover_popdown(GTK_POPOVER(popover));
        gtk_widget_unparent(popover);
    }
    g_object_unref(popover);
    return G_SOURCE_REMOVE;
}

static void on_crumb_right_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    CrumbClickData *ccd = (CrumbClickData *)user_data;
    GdkClipboard *clipboard = gdk_display_get_clipboard(gdk_display_get_default());
    gdk_clipboard_set_text(clipboard, ccd->target_path);

    GtkWidget *btn = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    if (!btn)
        return;

    GtkWidget *popover = gtk_popover_new();
    gtk_widget_set_parent(popover, btn);
    gtk_popover_set_autohide(GTK_POPOVER(popover), FALSE);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), TRUE);
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);
    gtk_widget_add_css_class(popover, "copied-tooltip-popover");

    GtkWidget *lbl = gtk_label_new("Copied!");
    gtk_widget_add_css_class(lbl, "copied-tooltip-label");
    gtk_popover_set_child(GTK_POPOVER(popover), lbl);

    gtk_popover_popup(GTK_POPOVER(popover));
    g_timeout_add(1100, close_copied_popover_cb, g_object_ref(popover));
}

static void add_breadcrumb_pill(AppState *state, const char *label_text, const char *target_path, gboolean is_current)
{
    GtkWidget *btn = gtk_button_new_with_label(label_text);
    gtk_widget_add_css_class(btn, "breadcrumb-pill");
    if (is_current)
    {
        gtk_widget_add_css_class(btn, "active");
    }
    gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);

    CrumbClickData *ccd = g_new0(CrumbClickData, 1);
    ccd->state = state;
    ccd->target_path = g_strdup(target_path);

    GtkGesture *rclick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(rclick), GDK_BUTTON_SECONDARY);
    g_signal_connect(rclick, "pressed", G_CALLBACK(on_crumb_right_clicked), ccd);
    gtk_widget_add_controller(btn, GTK_EVENT_CONTROLLER(rclick));

    g_signal_connect_data(btn, "clicked", G_CALLBACK(on_crumb_clicked),
                          ccd, free_crumb_click_data, 0);

    gtk_box_append(GTK_BOX(state->breadcrumb_box), btn);
}

static gboolean scroll_breadcrumbs_to_end_cb(gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    if (state->breadcrumb_scroll && GTK_IS_SCROLLED_WINDOW(state->breadcrumb_scroll))
    {
        GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(
            GTK_SCROLLED_WINDOW(state->breadcrumb_scroll));
        double max_val = gtk_adjustment_get_upper(hadj) - gtk_adjustment_get_page_size(hadj);
        if (max_val > 0)
        {
            gtk_adjustment_set_value(hadj, max_val);
        }
    }
    return G_SOURCE_REMOVE;
}

static void update_breadcrumbs(AppState *state, const char *path)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(state->breadcrumb_box)) != NULL)
    {
        gtk_box_remove(GTK_BOX(state->breadcrumb_box), child);
    }

    if (state->root_btn)
    {
        if (g_strcmp0(path, "/") == 0)
        {
            gtk_widget_add_css_class(state->root_btn, "active");
        }
        else
        {
            gtk_widget_remove_css_class(state->root_btn, "active");
        }
    }

    const char *home = g_get_home_dir();

    if (g_str_has_prefix(path, home))
    {
        const char *rel = path + strlen(home);
        if (*rel == '/')
            rel++;

        gboolean is_home_current = (*rel == '\0');
        add_breadcrumb_pill(state, "Home", home, is_home_current);

        if (!is_home_current)
        {
            char **parts = g_strsplit(rel, "/", -1);
            char *accum = g_strdup(home);

            for (int i = 0; parts[i] != NULL && *parts[i] != '\0'; i++)
            {
                GtkWidget *sep = gtk_label_new("›");
                gtk_widget_add_css_class(sep, "breadcrumb-sep");
                gtk_box_append(GTK_BOX(state->breadcrumb_box), sep);

                char *next_accum = g_build_filename(accum, parts[i], NULL);
                g_free(accum);
                accum = next_accum;

                gboolean last = (parts[i + 1] == NULL || *parts[i + 1] == '\0');
                add_breadcrumb_pill(state, parts[i], accum, last);
            }

            g_free(accum);
            g_strfreev(parts);
        }
    }
    else if (g_strcmp0(path, "/") != 0)
    {
        char **parts = g_strsplit(path, "/", -1);
        char *accum = g_strdup("/");
        gboolean first_added = FALSE;

        for (int i = 0; parts[i] != NULL; i++)
        {
            if (*parts[i] == '\0')
                continue;

            if (first_added)
            {
                GtkWidget *sep = gtk_label_new("›");
                gtk_widget_add_css_class(sep, "breadcrumb-sep");
                gtk_box_append(GTK_BOX(state->breadcrumb_box), sep);
            }

            char *next_accum = g_build_filename(accum, parts[i], NULL);
            g_free(accum);
            accum = next_accum;

            gboolean last = (parts[i + 1] == NULL || *parts[i + 1] == '\0');
            add_breadcrumb_pill(state, parts[i], accum, last);
            first_added = TRUE;
        }

        g_free(accum);
        g_strfreev(parts);
    }

    gtk_widget_set_sensitive(state->back_btn, state->back_stack != NULL);
    gtk_widget_set_sensitive(state->fwd_btn, state->forward_stack != NULL);
    g_idle_add(scroll_breadcrumbs_to_end_cb, state);
}

/* --- Status Bar Updates --- */
void update_status_bar(AppState *state)
{
    if (!state->status_label || !state->flowbox)
        return;

    int total_items = 0;
    guint64 total_folder_bytes = 0;
    int selected_items = 0;
    guint64 selected_bytes = 0;

    for (GtkWidget *child = gtk_widget_get_first_child(state->flowbox);
         child != NULL;
         child = gtk_widget_get_next_sibling(child))
    {
        GtkWidget *btn = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(child));
        if (!btn)
            continue;

        total_items++;
        guint64 item_bytes = 0;
        const char *path = (const char *)g_object_get_data(G_OBJECT(btn), "full-path");

        if (path && !g_file_test(path, G_FILE_TEST_IS_DIR))
        {
            GFile *f = g_file_new_for_path(path);
            GFileInfo *fi = g_file_query_info(f, "standard::size", G_FILE_QUERY_INFO_NONE, NULL, NULL);
            if (fi)
            {
                item_bytes = (guint64)g_file_info_get_size(fi);
                g_object_unref(fi);
            }
            g_object_unref(f);
        }

        total_folder_bytes += item_bytes;

        if (gtk_widget_has_css_class(btn, "selected"))
        {
            selected_items++;
            selected_bytes += item_bytes;
        }
    }

    char *total_size_str = g_format_size(total_folder_bytes);

    if (selected_items > 0)
    {
        char *sel_size_str = g_format_size(selected_bytes);
        char *msg = g_strdup_printf("%d item%s (%s)  •  %d selected (%s)",
                                    total_items, total_items == 1 ? "" : "s", total_size_str,
                                    selected_items, sel_size_str);
        gtk_label_set_text(GTK_LABEL(state->status_label), msg);
        g_free(msg);
        g_free(sel_size_str);
    }
    else
    {
        char *msg = g_strdup_printf("%d item%s (%s)",
                                    total_items, total_items == 1 ? "" : "s", total_size_str);
        gtk_label_set_text(GTK_LABEL(state->status_label), msg);
        g_free(msg);
    }

    g_free(total_size_str);
}

/* --- Sorting --- */
static gint compare_file_infos_with_state(gconstpointer a, gconstpointer b, gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    GFileInfo *ia = G_FILE_INFO(a);
    GFileInfo *ib = G_FILE_INFO(b);

    gboolean dir_a = (g_file_info_get_file_type(ia) == G_FILE_TYPE_DIRECTORY);
    gboolean dir_b = (g_file_info_get_file_type(ib) == G_FILE_TYPE_DIRECTORY);

    if (dir_a != dir_b)
    {
        return dir_a ? -1 : 1;
    }

    int cmp = 0;
    switch (state->sort_column)
    {
    case SORT_BY_TYPE:
    {
        const char *ta = dir_a ? "Folder" : (g_file_info_get_content_type(ia) ? g_file_info_get_content_type(ia) : "");
        const char *tb = dir_b ? "Folder" : (g_file_info_get_content_type(ib) ? g_file_info_get_content_type(ib) : "");
        cmp = g_ascii_strcasecmp(ta, tb);
        break;
    }
    case SORT_BY_DATE:
    {
        GDateTime *da = g_file_info_get_modification_date_time(ia);
        GDateTime *db = g_file_info_get_modification_date_time(ib);
        if (da && db)
        {
            cmp = g_date_time_compare(da, db);
        }
        else if (da || db)
        {
            cmp = da ? 1 : -1;
        }
        if (da)
            g_date_time_unref(da);
        if (db)
            g_date_time_unref(db);
        break;
    }
    case SORT_BY_SIZE:
    {
        goffset sa = g_file_info_get_size(ia);
        goffset sb = g_file_info_get_size(ib);
        cmp = (sa < sb) ? -1 : (sa > sb ? 1 : 0);
        break;
    }
    case SORT_BY_NAME:
    default:
        cmp = 0;
        break;
    }

    if (cmp == 0)
    {
        cmp = g_utf8_collate(g_file_info_get_display_name(ia),
                             g_file_info_get_display_name(ib));
    }

    return state->sort_ascending ? cmp : -cmp;
}

static void update_column_header_labels(AppState *state)
{
    if (!state->col_btn_name)
        return;

    const char *arrow = state->sort_ascending ? " ▲" : " ▼";

    char *lbl_name = g_strdup_printf("Name%s", state->sort_column == SORT_BY_NAME ? arrow : "");
    char *lbl_type = g_strdup_printf("Type%s", state->sort_column == SORT_BY_TYPE ? arrow : "");
    char *lbl_date = g_strdup_printf("Modified%s", state->sort_column == SORT_BY_DATE ? arrow : "");

    char *lbl_size = g_strdup_printf("Size%s", state->sort_column == SORT_BY_SIZE ? arrow : "");
    gtk_button_set_label(GTK_BUTTON(state->col_btn_name), lbl_name);
    gtk_button_set_label(GTK_BUTTON(state->col_btn_type), lbl_type);
    gtk_button_set_label(GTK_BUTTON(state->col_btn_date), lbl_date);
    gtk_button_set_label(GTK_BUTTON(state->col_btn_size), lbl_size);
    g_free(lbl_name);
    g_free(lbl_type);
    g_free(lbl_date);
    g_free(lbl_size);
}
void set_sort_column(AppState *state, SortColumn col)
{
    if (state->sort_column == col)
    {
        state->sort_ascending = !state->sort_ascending;
    }
    else
    {
        state->sort_column = col;
        state->sort_ascending = TRUE;
    }
    update_column_header_labels(state);
    if (state->current_path)
    {
        populate_directory(state, state->current_path);
    }
}
/* --- Directory Population --- */
void populate_directory(AppState *state, const char *path) {
    if (!state || !state->flowbox || !GTK_IS_FLOW_BOX(state->flowbox)) return;

    if (state->load_cancellable) {
        g_cancellable_cancel(state->load_cancellable);
        g_object_unref(state->load_cancellable);
    }
    state->load_cancellable = g_cancellable_new();

    if (state->list_header_bar && GTK_IS_WIDGET(state->list_header_bar)) {
        gtk_widget_set_visible(state->list_header_bar, state->is_list_view);
        update_column_header_labels(state);
    }

    if (state->is_list_view) {
        gtk_widget_set_halign(state->flowbox, GTK_ALIGN_FILL);
        gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(state->flowbox), FALSE);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(state->flowbox), 1);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(state->flowbox), 1);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(state->flowbox), 6);
    } else {
        gtk_widget_set_halign(state->flowbox, GTK_ALIGN_FILL);
        gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(state->flowbox), TRUE);
        gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(state->flowbox), 1);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(state->flowbox), 20);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(state->flowbox), 12);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(state->flowbox), 14);
    }

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(state->flowbox)) != NULL) {
        gtk_flow_box_remove(GTK_FLOW_BOX(state->flowbox), child);
    }

    GFile *dir = g_file_new_for_path(path);
    GError *error = NULL;

    GFileEnumerator *enumerator = g_file_enumerate_children(
        dir,
        "standard::name,standard::display-name,standard::type,standard::icon,standard::content-type,standard::size,standard::is-hidden,time::modified",
        G_FILE_QUERY_INFO_NONE,
        NULL,
        &error
    );

    if (error != NULL) {
        char *err_txt = g_strdup_printf("Unable to open folder: %s", error->message);
        GtkWidget *err_lbl = gtk_label_new(err_txt);
        gtk_widget_add_css_class(err_lbl, "file-label");
        gtk_flow_box_append(GTK_FLOW_BOX(state->flowbox), err_lbl);
        g_free(err_txt);
        g_error_free(error);
        g_object_unref(dir);
        update_status_bar(state);
        return;
    }

    const char *raw_query = gtk_editable_get_text(GTK_EDITABLE(state->search_entry));
    char *query_lower = (raw_query && *raw_query != '\0') ? g_utf8_strdown(raw_query, -1) : NULL;

    GList *entries = NULL;
    GFileInfo *info = NULL;

    while ((info = g_file_enumerator_next_file(enumerator, NULL, NULL)) != NULL) {
        const char *name = g_file_info_get_display_name(info);
        if (!name) {
            g_object_unref(info);
            continue;
        }

        if (!state->show_hidden && (g_file_info_get_is_hidden(info) || name[0] == '.')) {
            g_object_unref(info);
            continue;
        }

        if (query_lower != NULL) {
            char *name_lower = g_utf8_strdown(name, -1);
            gboolean match = (strstr(name_lower, query_lower) != NULL);
            g_free(name_lower);
            if (!match) {
                g_object_unref(info);
                continue;
            }
        }

        entries = g_list_prepend(entries, info);
    }

    g_free(query_lower);

    entries = g_list_sort_with_data(entries, compare_file_infos_with_state, state);

    for (GList *l = entries; l != NULL; l = l->next) {
        GFileInfo *sorted_info = G_FILE_INFO(l->data);
        GtkWidget *item = create_real_file_item(state, sorted_info, path);
        gtk_flow_box_append(GTK_FLOW_BOX(state->flowbox), item);
        g_object_unref(sorted_info);
    }
    g_list_free(entries);

    g_object_unref(enumerator);
    g_object_unref(dir);

    update_status_bar(state);
}
/* --- Real-Time Directory Auto-Refresh (GFileMonitor) --- */
static gboolean debounced_directory_refresh(gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    state->refresh_timeout_id = 0;
    if (state->current_path)
    {
        populate_directory(state, state->current_path);
    }
    return G_SOURCE_REMOVE;
}
static void on_directory_changed(GFileMonitor *monitor,
                                 GFile *file,
                                 GFile *other_file,
                                 GFileMonitorEvent event_type,
                                 gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    if (event_type == G_FILE_MONITOR_EVENT_CREATED ||
        event_type == G_FILE_MONITOR_EVENT_DELETED ||
        event_type == G_FILE_MONITOR_EVENT_MOVED_IN ||
        event_type == G_FILE_MONITOR_EVENT_MOVED_OUT ||
        event_type == G_FILE_MONITOR_EVENT_RENAMED ||
        event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    {
        if (state->refresh_timeout_id != 0)
        {
            g_source_remove(state->refresh_timeout_id);
        }
        state->refresh_timeout_id = g_timeout_add(150, debounced_directory_refresh, state);
    }
}
static void setup_directory_monitor(AppState *state, const char *path)
{
    if (state->refresh_timeout_id != 0)
    {
        g_source_remove(state->refresh_timeout_id);
        state->refresh_timeout_id = 0;
    }
    if (state->dir_monitor != NULL)
    {
        g_file_monitor_cancel(state->dir_monitor);
        g_object_unref(state->dir_monitor);
        state->dir_monitor = NULL;
    }
    GFile *dir = g_file_new_for_path(path);
    GError *err = NULL;
    state->dir_monitor = g_file_monitor_directory(dir, G_FILE_MONITOR_WATCH_MOVES, NULL, &err);
    if (state->dir_monitor != NULL)
    {
        g_signal_connect(state->dir_monitor, "changed", G_CALLBACK(on_directory_changed), state);
    }
    else if (err)
    {
        g_warning("Directory monitor unavailable: %s", err->message);
        g_error_free(err);
    }
    g_object_unref(dir);
}
/* --- Navigation & History --- */
void navigate_to_path(AppState *state, const char *path, gboolean record_history)
{
    if (!path || !g_file_test(path, G_FILE_TEST_IS_DIR))
        return;
    if (record_history && state->current_path != NULL && g_strcmp0(state->current_path, path) != 0)
    {
        state->back_stack = g_list_prepend(state->back_stack, g_strdup(state->current_path));
        g_list_free_full(state->forward_stack, g_free);
        state->forward_stack = NULL;
    }
    g_free(state->current_path);
    state->current_path = g_strdup(path);
    update_sidebar_active_highlight(state);
    update_breadcrumbs(state, state->current_path);
    setup_directory_monitor(state, state->current_path);
    populate_directory(state, state->current_path);
}
void on_back_clicked(GtkButton *btn, gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    if (!state->back_stack)
        return;
    char *prev = (char *)state->back_stack->data;
    state->back_stack = g_list_delete_link(state->back_stack, state->back_stack);
    if (state->current_path)
    {
        state->forward_stack = g_list_prepend(state->forward_stack, g_strdup(state->current_path));
    }
    navigate_to_path(state, prev, FALSE);
    g_free(prev);
}
void on_fwd_clicked(GtkButton *btn, gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    if (!state->forward_stack)
        return;
    char *next = (char *)state->forward_stack->data;
    state->forward_stack = g_list_delete_link(state->forward_stack, state->forward_stack);
    if (state->current_path)
    {
        state->back_stack = g_list_prepend(state->back_stack, g_strdup(state->current_path));
    }
    navigate_to_path(state, next, FALSE);
    g_free(next);
}
void on_search_changed(GtkSearchEntry *entry, gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    if (state->current_path)
    {
        populate_directory(state, state->current_path);
    }
}
/* --- XDG & Trash Directory Resolvers --- */
static char *resolve_xdg_or_home_subdir(GUserDirectory xdg_dir, const char *fallback_folder_name)
{
    const char *home = g_get_home_dir();
    const char *xdg = g_get_user_special_dir(xdg_dir);
    if (xdg != NULL && g_strcmp0(xdg, home) != 0 && g_file_test(xdg, G_FILE_TEST_IS_DIR))
    {
        return g_strdup(xdg);
    }
    char *fallback = g_build_filename(home, fallback_folder_name, NULL);
    if (!g_file_test(fallback, G_FILE_TEST_IS_DIR))
    {
        g_mkdir_with_parents(fallback, 0755);
    }
    return fallback;
}
char *resolve_sidebar_directory(const char *dir_type)
{
    if (g_strcmp0(dir_type, "HOME") == 0)
    {
        return g_strdup(g_get_home_dir());
    }
    else if (g_strcmp0(dir_type, "DOCUMENTS") == 0)
    {
        return resolve_xdg_or_home_subdir(G_USER_DIRECTORY_DOCUMENTS, "Documents");
    }
    else if (g_strcmp0(dir_type, "DOWNLOADS") == 0)
    {
        return resolve_xdg_or_home_subdir(G_USER_DIRECTORY_DOWNLOAD, "Downloads");
    }
    else if (g_strcmp0(dir_type, "PICTURES") == 0)
    {
        return resolve_xdg_or_home_subdir(G_USER_DIRECTORY_PICTURES, "Pictures");
    }
    else if (g_strcmp0(dir_type, "MUSIC") == 0)
    {
        return resolve_xdg_or_home_subdir(G_USER_DIRECTORY_MUSIC, "Music");
    }
    else if (g_strcmp0(dir_type, "TRASH") == 0)
    {
        char *trash_path = g_build_filename(g_get_user_data_dir(), "Trash", "files", NULL);
        if (!g_file_test(trash_path, G_FILE_TEST_IS_DIR))
        {
            g_mkdir_with_parents(trash_path, 0700);
        }
        return trash_path;
    }
    return g_strdup(g_get_home_dir());
}
gboolean is_current_dir_trash(AppState *state)
{
    if (!state->current_path)
        return FALSE;
    char *trash_path = resolve_sidebar_directory("TRASH");
    gboolean match = (g_strcmp0(state->current_path, trash_path) == 0);
    g_free(trash_path);
    return match;
}