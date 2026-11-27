#include "app.h"
#include <math.h>

/* --- Column Sort Header Callbacks --- */
static void on_sort_name_clicked(GtkButton *btn, gpointer data)
{
    (void)btn;
    set_sort_column((AppState *)data, SORT_BY_NAME);
}

static void on_sort_type_clicked(GtkButton *btn, gpointer data)
{
    (void)btn;
    set_sort_column((AppState *)data, SORT_BY_TYPE);
}

static void on_sort_date_clicked(GtkButton *btn, gpointer data)
{
    (void)btn;
    set_sort_column((AppState *)data, SORT_BY_DATE);
}

static void on_sort_size_clicked(GtkButton *btn, gpointer data)
{
    (void)btn;
    set_sort_column((AppState *)data, SORT_BY_SIZE);
}

/* --- Resizable List View Columns --- */
typedef enum
{
    COL_RESIZE_TYPE = 0,
    COL_RESIZE_DATE,
    COL_RESIZE_SIZE
} ColResizeTarget;

typedef struct
{
    AppState *state;
    GtkWidget *handle;
    ColResizeTarget target;
    double start_bar_x;
    int start_width;
} ColResizeData;

static void apply_list_column_widths(AppState *state)
{
    if (!state)
        return;

    int type_w = CLAMP(state->col_type_w > 0 ? state->col_type_w : 130, 60, 420);
    int date_w = CLAMP(state->col_date_w > 0 ? state->col_date_w : 145, 80, 360);
    int size_w = CLAMP(state->col_size_w > 0 ? state->col_size_w : 75, 50, 260);

    state->col_type_w = type_w;
    state->col_date_w = date_w;
    state->col_size_w = size_w;

    if (state->col_btn_type)
        gtk_widget_set_size_request(state->col_btn_type, type_w, -1);
    if (state->col_btn_date)
        gtk_widget_set_size_request(state->col_btn_date, date_w, -1);
    if (state->col_btn_size)
        gtk_widget_set_size_request(state->col_btn_size, size_w, -1);

    if (!state->flowbox || !GTK_IS_FLOW_BOX(state->flowbox) || !state->is_list_view)
        return;

    for (GtkWidget *child = gtk_widget_get_first_child(state->flowbox);
         child != NULL;
         child = gtk_widget_get_next_sibling(child))
    {
        if (!GTK_IS_FLOW_BOX_CHILD(child))
            continue;
        GtkWidget *btn = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(child));
        if (!btn)
            continue;

        GtkWidget *t_lbl = g_object_get_data(G_OBJECT(btn), "col-type-lbl");
        GtkWidget *d_lbl = g_object_get_data(G_OBJECT(btn), "col-date-lbl");
        GtkWidget *s_lbl = g_object_get_data(G_OBJECT(btn), "col-size-lbl");

        if (t_lbl)
            gtk_widget_set_size_request(t_lbl, type_w, -1);
        if (d_lbl)
            gtk_widget_set_size_request(d_lbl, date_w, -1);
        if (s_lbl)
            gtk_widget_set_size_request(s_lbl, size_w, -1);
    }
}

static double handle_x_in_header_bar(ColResizeData *crd, double local_x)
{
    graphene_point_t pt_in = GRAPHENE_POINT_INIT((float)local_x, 0.0f);
    graphene_point_t pt_out;
    if (crd->state->list_header_bar &&
        gtk_widget_compute_point(crd->handle, crd->state->list_header_bar, &pt_in, &pt_out))
    {
        return (double)pt_out.x;
    }
    return local_x;
}

static void on_col_resize_begin(GtkGestureDrag *gesture, double start_x, double start_y, gpointer user_data)
{
    (void)gesture;
    (void)start_y;
    ColResizeData *crd = (ColResizeData *)user_data;
    crd->start_bar_x = handle_x_in_header_bar(crd, start_x);

    if (crd->target == COL_RESIZE_TYPE)
        crd->start_width = crd->state->col_type_w;
    else if (crd->target == COL_RESIZE_DATE)
        crd->start_width = crd->state->col_date_w;
    else
        crd->start_width = crd->state->col_size_w;
}

static void on_col_resize_update(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data)
{
    (void)offset_y;
    ColResizeData *crd = (ColResizeData *)user_data;
    double start_local_x = 0.0, start_local_y = 0.0;
    gtk_gesture_drag_get_start_point(gesture, &start_local_x, &start_local_y);

    double current_bar_x = handle_x_in_header_bar(crd, start_local_x + offset_x);
    double delta_x = current_bar_x - crd->start_bar_x;
    int new_w = crd->start_width - (int)round(delta_x);

    if (crd->target == COL_RESIZE_TYPE)
        crd->state->col_type_w = CLAMP(new_w, 60, 420);
    else if (crd->target == COL_RESIZE_DATE)
        crd->state->col_date_w = CLAMP(new_w, 80, 360);
    else
        crd->state->col_size_w = CLAMP(new_w, 50, 260);

    apply_list_column_widths(crd->state);
}

static void on_col_resize_end(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data)
{
    (void)gesture;
    (void)offset_x;
    (void)offset_y;
    ColResizeData *crd = (ColResizeData *)user_data;
    save_app_settings(crd->state);
}

static GtkWidget *create_col_resize_handle(AppState *state, ColResizeTarget target)
{
    GtkWidget *handle = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(handle, "list-col-resizer");
    gtk_widget_set_size_request(handle, 8, -1);
    gtk_widget_set_valign(handle, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(handle, FALSE);
    gtk_widget_set_cursor_from_name(handle, "col-resize");

    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(bar, "list-col-resizer-bar");
    gtk_widget_set_size_request(bar, 2, 14);
    gtk_widget_set_halign(bar, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(bar, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(bar, FALSE);
    gtk_box_append(GTK_BOX(handle), bar);

    ColResizeData *crd = g_new0(ColResizeData, 1);
    crd->state = state;
    crd->handle = handle;
    crd->target = target;
    g_object_set_data_full(G_OBJECT(handle), "col-resize-data", crd, g_free);

    GtkGesture *drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
    g_signal_connect(drag, "drag-begin", G_CALLBACK(on_col_resize_begin), crd);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_col_resize_update), crd);
    g_signal_connect(drag, "drag-end", G_CALLBACK(on_col_resize_end), crd);
    gtk_widget_add_controller(handle, GTK_EVENT_CONTROLLER(drag));

    return handle;
}

GtkWidget *build_list_column_headers(AppState *state)
{
    GtkWidget *list_header_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(list_header_bar, "list-column-headers");
    gtk_widget_set_valign(list_header_bar, GTK_ALIGN_START);
    gtk_widget_set_vexpand(list_header_bar, FALSE);
    gtk_widget_set_visible(list_header_bar, state->is_list_view);
    state->list_header_bar = list_header_bar;

    GtkWidget *icon_col_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(icon_col_spacer, 32, -1);

    GtkWidget *col_name = gtk_button_new_with_label("Name ▲");
    gtk_widget_add_css_class(col_name, "list-col-btn");
    gtk_widget_set_hexpand(col_name, TRUE);
    gtk_widget_set_halign(col_name, GTK_ALIGN_FILL);
    gtk_widget_set_valign(col_name, GTK_ALIGN_CENTER);
    gtk_button_set_has_frame(GTK_BUTTON(col_name), FALSE);

    GtkWidget *col_type = gtk_button_new_with_label("Type");
    gtk_widget_add_css_class(col_type, "list-col-btn");
    gtk_widget_set_size_request(col_type, state->col_type_w, -1);
    gtk_widget_set_valign(col_type, GTK_ALIGN_CENTER);
    gtk_button_set_has_frame(GTK_BUTTON(col_type), FALSE);

    GtkWidget *col_date = gtk_button_new_with_label("Modified");
    gtk_widget_add_css_class(col_date, "list-col-btn");
    gtk_widget_set_size_request(col_date, state->col_date_w, -1);
    gtk_widget_set_valign(col_date, GTK_ALIGN_CENTER);
    gtk_button_set_has_frame(GTK_BUTTON(col_date), FALSE);

    GtkWidget *col_size = gtk_button_new_with_label("Size");
    gtk_widget_add_css_class(col_size, "list-col-btn");
    gtk_widget_set_size_request(col_size, state->col_size_w, -1);
    gtk_widget_set_valign(col_size, GTK_ALIGN_CENTER);
    gtk_button_set_has_frame(GTK_BUTTON(col_size), FALSE);

    state->col_btn_name = col_name;
    state->col_btn_type = col_type;
    state->col_btn_date = col_date;
    state->col_btn_size = col_size;

    g_signal_connect(col_name, "clicked", G_CALLBACK(on_sort_name_clicked), state);
    g_signal_connect(col_type, "clicked", G_CALLBACK(on_sort_type_clicked), state);
    g_signal_connect(col_date, "clicked", G_CALLBACK(on_sort_date_clicked), state);
    g_signal_connect(col_size, "clicked", G_CALLBACK(on_sort_size_clicked), state);

    gtk_box_append(GTK_BOX(list_header_bar), icon_col_spacer);
    gtk_box_append(GTK_BOX(list_header_bar), col_name);
    gtk_box_append(GTK_BOX(list_header_bar), create_col_resize_handle(state, COL_RESIZE_TYPE));
    gtk_box_append(GTK_BOX(list_header_bar), col_type);
    gtk_box_append(GTK_BOX(list_header_bar), create_col_resize_handle(state, COL_RESIZE_DATE));
    gtk_box_append(GTK_BOX(list_header_bar), col_date);
    gtk_box_append(GTK_BOX(list_header_bar), create_col_resize_handle(state, COL_RESIZE_SIZE));
    gtk_box_append(GTK_BOX(list_header_bar), col_size);

    return list_header_bar;
}

/* --- Empty-Space Background Context Menu --- */
static void on_bg_new_folder_clicked(GtkButton *btn, gpointer user_data)
{
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    show_create_item_dialog((AppState *)user_data, TRUE);
}

static void on_bg_new_file_clicked(GtkButton *btn, gpointer user_data)
{
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    show_create_item_dialog((AppState *)user_data, FALSE);
}

static void on_bg_open_terminal_clicked(GtkButton *btn, gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    if (state->current_path)
    {
        open_directory_in_terminal(state->current_path);
    }
}

static void on_bg_paste_clicked(GtkButton *btn, gpointer user_data)
{
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    bulk_paste_clipboard_items((AppState *)user_data);
}

static void on_bg_empty_trash_clicked(GtkButton *btn, gpointer user_data)
{
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    empty_trash_completely((AppState *)user_data);
}

static gboolean deferred_apply_desktop_icons_cb(gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    if (state && state->current_path)
    {
        populate_directory(state, state->current_path);
        if (state->window && GTK_IS_WINDOW(state->window))
        {
            gtk_widget_queue_draw(state->window);
            GtkNative *nat = gtk_widget_get_native(state->window);
            GdkSurface *surf = nat ? gtk_native_get_surface(nat) : NULL;
            if (surf)
                gdk_surface_queue_render(surf);
        }
    }
    return G_SOURCE_REMOVE;
}

static void on_bg_toggle_desktop_icons_clicked(GtkButton *btn, gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    if (!state)
        return;
    state->hide_desktop_icons = !state->hide_desktop_icons;
    save_app_settings(state);
    g_idle_add(deferred_apply_desktop_icons_cb, state);
}

static void on_bg_change_wallpaper_clicked(GtkButton *btn, gpointer user_data)
{
    (void)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    const char *argv[] = {"blurred_settings", "--page", "wallpaper", NULL};
    g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

static void on_bg_desktop_settings_clicked(GtkButton *btn, gpointer user_data)
{
    (void)user_data;
    GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(btn), GTK_TYPE_POPOVER);
    if (popover)
        gtk_popover_popdown(GTK_POPOVER(popover));
    const char *argv[] = {"blurred_settings", "--page", "appearance", NULL};
    g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

static void on_grid_bg_right_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    (void)gesture;
    (void)n_press;
    AppState *state = (AppState *)user_data;
    GtkWidget *picked = gtk_widget_pick(state->grid_overlay, x, y, GTK_PICK_DEFAULT);
    if (picked && gtk_widget_get_ancestor(picked, GTK_TYPE_BUTTON) != NULL)
    {
        return;
    }

    GtkWidget *popover = gtk_popover_new();
    gtk_widget_set_parent(popover, state->grid_overlay);
    g_signal_connect(popover, "closed", G_CALLBACK(gtk_widget_unparent), NULL);
    gtk_widget_add_css_class(popover, "context-popover");

    GdkRectangle rect = {(int)x, (int)y, 1, 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);

    GtkWidget *menu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    gtk_widget_add_css_class(menu_box, "context-menu-box");
    if (is_current_dir_trash(state))
    {
        GtkWidget *btn_empty = gtk_button_new_with_label("Empty Trash");
        gtk_widget_add_css_class(btn_empty, "context-menu-btn");
        gtk_widget_add_css_class(btn_empty, "danger");
        g_signal_connect(btn_empty, "clicked", G_CALLBACK(on_bg_empty_trash_clicked), state);
        gtk_box_append(GTK_BOX(menu_box), btn_empty);
    }
    else
    {
        if (state->clipboard_paths != NULL)
        {
            guint count = g_list_length(state->clipboard_paths);
            char *paste_lbl = g_strdup_printf("Paste %u Item%s", count, count > 1 ? "s" : "");
            GtkWidget *btn_paste = gtk_button_new_with_label(paste_lbl);
            gtk_widget_add_css_class(btn_paste, "context-menu-btn");
            g_signal_connect(btn_paste, "clicked", G_CALLBACK(on_bg_paste_clicked), state);
            gtk_box_append(GTK_BOX(menu_box), btn_paste);
            g_free(paste_lbl);
        }
        GtkWidget *btn_folder = gtk_button_new_with_label("New Folder");
        GtkWidget *btn_file = gtk_button_new_with_label("New File");
        GtkWidget *btn_term = gtk_button_new_with_label("Open in Terminal");
        gtk_widget_add_css_class(btn_folder, "context-menu-btn");
        gtk_widget_add_css_class(btn_file, "context-menu-btn");
        gtk_widget_add_css_class(btn_term, "context-menu-btn");
        g_signal_connect(btn_folder, "clicked", G_CALLBACK(on_bg_new_folder_clicked), state);
        g_signal_connect(btn_file, "clicked", G_CALLBACK(on_bg_new_file_clicked), state);
        g_signal_connect(btn_term, "clicked", G_CALLBACK(on_bg_open_terminal_clicked), state);
        gtk_box_append(GTK_BOX(menu_box), btn_folder);
        gtk_box_append(GTK_BOX(menu_box), btn_file);
        gtk_box_append(GTK_BOX(menu_box), btn_term);
        if (state->is_desktop_mode)
        {
            gtk_box_append(GTK_BOX(menu_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
            GtkWidget *btn_icons = gtk_button_new_with_label(
                state->hide_desktop_icons ? "Show Desktop Icons" : "Hide Desktop Icons");
            gtk_widget_add_css_class(btn_icons, "context-menu-btn");
            g_signal_connect(btn_icons, "clicked", G_CALLBACK(on_bg_toggle_desktop_icons_clicked), state);
            gtk_box_append(GTK_BOX(menu_box), btn_icons);
            GtkWidget *btn_wp = gtk_button_new_with_label("Change Wallpaper...");
            GtkWidget *btn_cfg = gtk_button_new_with_label("Desktop Settings");
            gtk_widget_add_css_class(btn_wp, "context-menu-btn");
            gtk_widget_add_css_class(btn_cfg, "context-menu-btn");
            g_signal_connect(btn_wp, "clicked", G_CALLBACK(on_bg_change_wallpaper_clicked), state);
            g_signal_connect(btn_cfg, "clicked", G_CALLBACK(on_bg_desktop_settings_clicked), state);
            gtk_box_append(GTK_BOX(menu_box), btn_wp);
            gtk_box_append(GTK_BOX(menu_box), btn_cfg);
        }
    }
    gtk_popover_set_child(GTK_POPOVER(popover), menu_box);
    gtk_popover_popup(GTK_POPOVER(popover));
}
/* --- Cross-Window Drop Target onto Grid Background --- */
static gboolean on_grid_bg_drop(GtkDropTarget *target, const GValue *value, double x, double y, gpointer user_data)
{
    (void)target;
    (void)x;
    (void)y;
    AppState *state = (AppState *)user_data;
    if (!state->current_path)
        return FALSE;
    return move_dropped_files_to_directory(state, value, state->current_path, is_current_dir_trash(state));
}
/* --- Whitespace-Only Rubberband Rectangle Selection --- */
static void on_drag_begin(GtkGestureDrag *gesture, double start_x, double start_y, gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    GtkWidget *picked = gtk_widget_pick(state->grid_overlay, start_x, start_y, GTK_PICK_DEFAULT);
    if (picked && gtk_widget_get_ancestor(picked, GTK_TYPE_BUTTON) != NULL)
    {
        state->is_selecting = FALSE;
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_DENIED);
        return;
    }
    state->is_selecting = TRUE;
    state->sel_start_x = start_x;
    state->sel_start_y = start_y;
    clear_item_selection(state);
    gtk_widget_set_margin_start(state->selection_box, (int)start_x);
    gtk_widget_set_margin_top(state->selection_box, (int)start_y);
    gtk_widget_set_size_request(state->selection_box, 1, 1);
    gtk_widget_set_visible(state->selection_box, TRUE);
}
static void on_drag_update(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data)
{
    (void)gesture;
    AppState *state = (AppState *)user_data;
    if (!state->is_selecting)
        return;
    double rx = (offset_x >= 0) ? state->sel_start_x : (state->sel_start_x + offset_x);
    double ry = (offset_y >= 0) ? state->sel_start_y : (state->sel_start_y + offset_y);
    double rw = fabs(offset_x);
    double rh = fabs(offset_y);
    if (rx < 0)
    {
        rw += rx;
        rx = 0;
    }
    if (ry < 0)
    {
        rh += ry;
        ry = 0;
    }
    gtk_widget_set_margin_start(state->selection_box, (int)rx);
    gtk_widget_set_margin_top(state->selection_box, (int)ry);
    gtk_widget_set_size_request(state->selection_box, (int)rw, (int)rh);
    graphene_rect_t sel_rect = GRAPHENE_RECT_INIT((float)rx, (float)ry, (float)rw, (float)rh);
    for (GtkWidget *child = gtk_widget_get_first_child(state->flowbox);
         child != NULL;
         child = gtk_widget_get_next_sibling(child))
    {
        GtkWidget *btn = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(child));
        if (!btn)
            continue;
        graphene_rect_t bounds;
        if (gtk_widget_compute_bounds(btn, state->grid_overlay, &bounds))
        {
            if (graphene_rect_intersection(&sel_rect, &bounds, NULL))
            {
                gtk_widget_add_css_class(btn, "selected");
            }
            else
            {
                gtk_widget_remove_css_class(btn, "selected");
            }
        }
    }
    update_status_bar(state);
}
static void on_drag_end(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data)
{
    (void)gesture;
    (void)offset_x;
    (void)offset_y;
    AppState *state = (AppState *)user_data;
    if (!state->is_selecting)
        return;
    state->is_selecting = FALSE;
    gtk_widget_set_visible(state->selection_box, FALSE);
    update_status_bar(state);
}
/* --- Nautilus-Style 5-Tier Zoom (Ctrl + Mouse Wheel & Ctrl + +/-/0) --- */
static guint zoom_save_timer_id = 0;
static AppState *zoom_save_state = NULL;
static double wheel_zoom_accum = 0.0;

static void on_zoom_window_destroyed(GtkWidget *window, gpointer user_data)
{
    (void)window;
    (void)user_data;
    if (zoom_save_timer_id != 0)
    {
        g_source_remove(zoom_save_timer_id);
        zoom_save_timer_id = 0;
    }
    zoom_save_state = NULL;
}

static gboolean save_col_zoom_cb(gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    zoom_save_timer_id = 0;
    zoom_save_state = NULL;
    if (state && state->window && GTK_IS_WINDOW(state->window))
    {
        save_app_settings(state);
    }
    return G_SOURCE_REMOVE;
}

static void step_grid_columns(AppState *state, int tier_delta)
{
    if (!state || state->is_list_view)
        return;
    int current = CLAMP(state->grid_cols, 0, 4);
    int next = CLAMP(current + tier_delta, 0, 4);
    if (next == current)
        return;
    state->grid_cols = next;
    apply_grid_column_zoom(state);

    if (zoom_save_timer_id != 0)
    {
        g_source_remove(zoom_save_timer_id);
        zoom_save_timer_id = 0;
    }
    if (state->window && GTK_IS_WINDOW(state->window))
    {
        if (!g_object_get_data(G_OBJECT(state->window), "zoom-destroy-hooked"))
        {
            g_object_set_data(G_OBJECT(state->window), "zoom-destroy-hooked", GINT_TO_POINTER(1));
            g_signal_connect(state->window, "destroy", G_CALLBACK(on_zoom_window_destroyed), NULL);
        }
        zoom_save_state = state;
        zoom_save_timer_id = g_timeout_add(300, save_col_zoom_cb, state);
    }
}
static gboolean on_grid_scroll_zoom(GtkEventControllerScroll *ctrl, double dx, double dy, gpointer user_data)
{
    (void)dx;
    AppState *state = (AppState *)user_data;
    GdkModifierType mods = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(ctrl));
    if ((mods & GDK_CONTROL_MASK) != 0)
    {
        wheel_zoom_accum += dy;
        if (wheel_zoom_accum <= -0.8)
        {
            step_grid_columns(state, 1);
            wheel_zoom_accum = 0.0;
        }
        else if (wheel_zoom_accum >= 0.8)
        {
            step_grid_columns(state, -1);
            wheel_zoom_accum = 0.0;
        }
        return TRUE;
    }
    wheel_zoom_accum = 0.0;
    return FALSE;
}
/* --- Shared Keyboard Shortcuts --- */
gboolean on_window_key_pressed(GtkEventControllerKey *controller,
                               guint keyval,
                               guint keycode,
                               GdkModifierType modifiers,
                               gpointer user_data)
{
    (void)controller;
    (void)keycode;
    AppState *state = (AppState *)user_data;
    gboolean ctrl = (modifiers & GDK_CONTROL_MASK) != 0;
    gboolean shift = (modifiers & GDK_SHIFT_MASK) != 0;
    gboolean alt = (modifiers & GDK_ALT_MASK) != 0;
    GtkWidget *focused = gtk_window_get_focus(GTK_WINDOW(state->window));
    gboolean typing_in_entry = (focused != NULL && GTK_IS_EDITABLE(focused));
    if (ctrl && (keyval == GDK_KEY_plus || keyval == GDK_KEY_equal || keyval == GDK_KEY_KP_Add))
    {
        step_grid_columns(state, 1);
        return TRUE;
    }
    if (ctrl && (keyval == GDK_KEY_minus || keyval == GDK_KEY_KP_Subtract))
    {
        step_grid_columns(state, -1);
        return TRUE;
    }
    if (ctrl && (keyval == GDK_KEY_0 || keyval == GDK_KEY_KP_0))
    {
        state->grid_cols = 2;
        apply_grid_column_zoom(state);
        save_app_settings(state);
        return TRUE;
    }
    if (ctrl && !shift && (keyval == GDK_KEY_z || keyval == GDK_KEY_Z))
    {
        if (!typing_in_entry)
        {
            perform_undo(state);
            return TRUE;
        }
    }
    if ((ctrl && !shift && (keyval == GDK_KEY_y || keyval == GDK_KEY_Y)) ||
        (ctrl && shift && (keyval == GDK_KEY_z || keyval == GDK_KEY_Z)))
    {
        if (!typing_in_entry)
        {
            perform_redo(state);
            return TRUE;
        }
    }
    if (ctrl && alt && (keyval == GDK_KEY_t || keyval == GDK_KEY_T))
    {
        if (state->current_path)
        {
            open_directory_in_terminal(state->current_path);
        }
        return TRUE;
    }
    if (ctrl && shift && (keyval == GDK_KEY_n || keyval == GDK_KEY_N))
    {
        show_create_item_dialog(state, TRUE);
        return TRUE;
    }
    if (ctrl && (keyval == GDK_KEY_a || keyval == GDK_KEY_A))
    {
        if (!typing_in_entry)
        {
            select_all_items(state);
            return TRUE;
        }
    }
    if (ctrl && (keyval == GDK_KEY_c || keyval == GDK_KEY_C))
    {
        if (!typing_in_entry)
        {
            bulk_copy_selected_items(state, FALSE);
            return TRUE;
        }
    }
    if (ctrl && (keyval == GDK_KEY_x || keyval == GDK_KEY_X))
    {
        if (!typing_in_entry)
        {
            bulk_copy_selected_items(state, TRUE);
            return TRUE;
        }
    }
    if (ctrl && (keyval == GDK_KEY_v || keyval == GDK_KEY_V))
    {
        if (!typing_in_entry)
        {
            bulk_paste_clipboard_items(state);
            return TRUE;
        }
    }
    if (keyval == GDK_KEY_Delete && !typing_in_entry)
    {
        bulk_trash_selected_items(state);
        return TRUE;
    }
    if (keyval == GDK_KEY_space && !typing_in_entry)
    {
        GList *sel = get_selected_paths(state);
        if (sel != NULL)
        {
            show_properties_dialog(state, (const char *)sel->data);
            g_list_free_full(sel, g_free);
        }
        else if (state->current_path)
        {
            show_properties_dialog(state, state->current_path);
        }
        return TRUE;
    }
    if (ctrl && (keyval == GDK_KEY_h || keyval == GDK_KEY_H))
    {
        state->show_hidden = !state->show_hidden;
        save_app_settings(state);
        if (state->current_path)
            populate_directory(state, state->current_path);
        return TRUE;
    }
    if (ctrl && (keyval == GDK_KEY_f || keyval == GDK_KEY_F))
    {
        if (state->search_entry)
        {
            gtk_widget_grab_focus(state->search_entry);
            return TRUE;
        }
    }
    if (keyval == GDK_KEY_Escape)
    {
        clear_item_selection(state);
        if (state->search_entry)
        {
            const char *txt = gtk_editable_get_text(GTK_EDITABLE(state->search_entry));
            if (txt && *txt != '\0')
            {
                gtk_editable_set_text(GTK_EDITABLE(state->search_entry), "");
            }
        }
        return TRUE;
    }
    if (!state->is_desktop_mode)
    {
        if (alt && keyval == GDK_KEY_Left)
        {
            on_back_clicked(NULL, state);
            return TRUE;
        }
        if (alt && keyval == GDK_KEY_Right)
        {
            on_fwd_clicked(NULL, state);
            return TRUE;
        }
        if (keyval == GDK_KEY_BackSpace && !typing_in_entry)
        {
            if (state->current_path && g_strcmp0(state->current_path, "/") != 0)
            {
                char *parent = g_path_get_dirname(state->current_path);
                navigate_to_path(state, parent, TRUE);
                g_free(parent);
                return TRUE;
            }
        }
    }
    return FALSE;
}
/* --- Alpha-Mask Container for Smooth Top & Bottom Scroll Edge Fading --- */
typedef struct
{
    GtkWidget parent_instance;
    gboolean fade_top;
    gboolean fade_bottom;
} FilesFadeBox;

typedef struct
{
    GtkWidgetClass parent_class;
} FilesFadeBoxClass;

G_DEFINE_TYPE(FilesFadeBox, files_fade_box, GTK_TYPE_WIDGET)

static void files_fade_box_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
    FilesFadeBox *self = (FilesFadeBox *)widget;
    GtkWidget *child = gtk_widget_get_first_child(widget);
    if (!child)
        return;

    int w = gtk_widget_get_width(widget);
    int h = gtk_widget_get_height(widget);

    if ((!self->fade_top && !self->fade_bottom) || w <= 0 || h <= 60)
    {
        gtk_widget_snapshot_child(widget, child, snapshot);
        return;
    }

    const float fade_px = 28.0f;
    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0.0f, 0.0f, (float)w, (float)h);

    gtk_snapshot_push_mask(snapshot, GSK_MASK_MODE_ALPHA);

    GskColorStop stops[4];
    int n_stops = 0;

    if (self->fade_top)
    {
        stops[n_stops++] = (GskColorStop){0.0f, {1.0f, 1.0f, 1.0f, 0.0f}};
        stops[n_stops++] = (GskColorStop){MIN(0.45f, fade_px / (float)h), {1.0f, 1.0f, 1.0f, 1.0f}};
    }
    else
    {
        stops[n_stops++] = (GskColorStop){0.0f, {1.0f, 1.0f, 1.0f, 1.0f}};
    }

    if (self->fade_bottom)
    {
        stops[n_stops++] = (GskColorStop){MAX(0.55f, 1.0f - (fade_px / (float)h)), {1.0f, 1.0f, 1.0f, 1.0f}};
        stops[n_stops++] = (GskColorStop){1.0f, {1.0f, 1.0f, 1.0f, 0.0f}};
    }
    else
    {
        stops[n_stops++] = (GskColorStop){1.0f, {1.0f, 1.0f, 1.0f, 1.0f}};
    }

    gtk_snapshot_append_linear_gradient(snapshot,
                                        &bounds,
                                        &GRAPHENE_POINT_INIT(0.0f, 0.0f),
                                        &GRAPHENE_POINT_INIT(0.0f, (float)h),
                                        stops,
                                        n_stops);
    gtk_snapshot_pop(snapshot);

    gtk_widget_snapshot_child(widget, child, snapshot);
    gtk_snapshot_pop(snapshot);
}

static void files_fade_box_dispose(GObject *object)
{
    GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(object));
    if (child)
        gtk_widget_unparent(child);
    G_OBJECT_CLASS(files_fade_box_parent_class)->dispose(object);
}

static void files_fade_box_class_init(FilesFadeBoxClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = files_fade_box_dispose;
    GtkWidgetClass *wclass = GTK_WIDGET_CLASS(klass);
    wclass->snapshot = files_fade_box_snapshot;
    gtk_widget_class_set_layout_manager_type(wclass, GTK_TYPE_BIN_LAYOUT);
}

static void files_fade_box_init(FilesFadeBox *self)
{
    self->fade_top = FALSE;
    self->fade_bottom = FALSE;
}

static void on_files_vadj_updated(GtkAdjustment *vadj, gpointer user_data)
{
    FilesFadeBox *self = (FilesFadeBox *)user_data;
    if (!GTK_IS_WIDGET(self))
        return;
    double val = gtk_adjustment_get_value(vadj);
    double page = gtk_adjustment_get_page_size(vadj);
    double upper = gtk_adjustment_get_upper(vadj);
    gboolean want_top = (val > 1.0);
    gboolean want_bot = ((val + page) < (upper - 1.0));
    if (self->fade_top != want_top || self->fade_bottom != want_bot)
    {
        self->fade_top = want_top;
        self->fade_bottom = want_bot;
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
}

/* --- Shared File Grid & Marquee Overlay Builder --- */
GtkWidget *build_file_grid_overlay(AppState *state)
{
    GtkWidget *grid_overlay = gtk_overlay_new();
    gtk_widget_set_vexpand(grid_overlay, TRUE);
    state->grid_overlay = grid_overlay;
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_EXTERNAL, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(scrolled), FALSE);
    gtk_widget_set_vexpand(scrolled, TRUE);
    state->scrolled = scrolled;
    GtkWidget *flowbox = gtk_flow_box_new();
    gtk_widget_set_halign(flowbox, GTK_ALIGN_FILL);
    gtk_widget_set_valign(flowbox, GTK_ALIGN_START);
    gtk_widget_add_css_class(flowbox, "files-grid");
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flowbox), TRUE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flowbox), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox), 20);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flowbox), 12);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flowbox), 12);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox), GTK_SELECTION_NONE);
    state->flowbox = flowbox;
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), flowbox);
    if (!state->is_desktop_mode)
    {
        GtkWidget *fade_box = g_object_new(files_fade_box_get_type(), NULL);
        gtk_widget_set_vexpand(fade_box, TRUE);
        gtk_widget_set_parent(scrolled, fade_box);
        GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled));
        if (vadj)
        {
            g_signal_connect(vadj, "value-changed", G_CALLBACK(on_files_vadj_updated), fade_box);
            g_signal_connect(vadj, "changed", G_CALLBACK(on_files_vadj_updated), fade_box);
            on_files_vadj_updated(vadj, fade_box);
        }
        gtk_overlay_set_child(GTK_OVERLAY(grid_overlay), fade_box);
    }
    else
    {
        gtk_overlay_set_child(GTK_OVERLAY(grid_overlay), scrolled);
    }
    GtkWidget *selection_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(selection_box, "selection-marquee");
    gtk_widget_set_halign(selection_box, GTK_ALIGN_START);
    gtk_widget_set_valign(selection_box, GTK_ALIGN_START);
    gtk_widget_set_can_target(selection_box, FALSE);
    gtk_widget_set_visible(selection_box, FALSE);
    state->selection_box = selection_box;
    gtk_overlay_add_overlay(GTK_OVERLAY(grid_overlay), selection_box);
    GtkEventController *zoom_scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    gtk_event_controller_set_propagation_phase(zoom_scroll, GTK_PHASE_CAPTURE);
    g_signal_connect(zoom_scroll, "scroll", G_CALLBACK(on_grid_scroll_zoom), state);
    gtk_widget_add_controller(grid_overlay, zoom_scroll);
    GtkGesture *drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
    g_signal_connect(drag, "drag-begin", G_CALLBACK(on_drag_begin), state);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update), state);
    g_signal_connect(drag, "drag-end", G_CALLBACK(on_drag_end), state);
    gtk_widget_add_controller(grid_overlay, GTK_EVENT_CONTROLLER(drag));
    GtkGesture *bg_rclick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(bg_rclick), GDK_BUTTON_SECONDARY);
    g_signal_connect(bg_rclick, "pressed", G_CALLBACK(on_grid_bg_right_clicked), state);
    gtk_widget_add_controller(grid_overlay, GTK_EVENT_CONTROLLER(bg_rclick));
    GType bg_drop_types[] = {GDK_TYPE_FILE_LIST, G_TYPE_FILE};
    GtkDropTarget *bg_drop_tgt = gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_MOVE | GDK_ACTION_COPY);
    gtk_drop_target_set_gtypes(bg_drop_tgt, bg_drop_types, G_N_ELEMENTS(bg_drop_types));
    g_signal_connect(bg_drop_tgt, "drop", G_CALLBACK(on_grid_bg_drop), state);
    gtk_widget_add_controller(grid_overlay, GTK_EVENT_CONTROLLER(bg_drop_tgt));
    return grid_overlay;
}