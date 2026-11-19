#include "app.h"
#include <math.h>

/* --- 8-Way Borderless Window Edge Resize --- */
static gboolean detect_resize_edge(GtkWidget *window, double x, double y, GdkSurfaceEdge *out_edge)
{
    int w = gtk_widget_get_width(window);
    int h = gtk_widget_get_height(window);
    const double margin = 24.0;

    if (gtk_window_is_maximized(GTK_WINDOW(window)))
    {
        return FALSE;
    }

    gboolean left = (x >= 0 && x <= margin);
    gboolean right = (x >= w - margin && x <= w);
    gboolean top = (y >= 0 && y <= margin);
    gboolean bottom = (y >= h - margin && y <= h);

    if (top && left)
    {
        *out_edge = GDK_SURFACE_EDGE_NORTH_WEST;
        return TRUE;
    }
    if (top && right)
    {
        *out_edge = GDK_SURFACE_EDGE_NORTH_EAST;
        return TRUE;
    }
    if (bottom && left)
    {
        *out_edge = GDK_SURFACE_EDGE_SOUTH_WEST;
        return TRUE;
    }
    if (bottom && right)
    {
        *out_edge = GDK_SURFACE_EDGE_SOUTH_EAST;
        return TRUE;
    }
    if (left)
    {
        *out_edge = GDK_SURFACE_EDGE_WEST;
        return TRUE;
    }
    if (right)
    {
        *out_edge = GDK_SURFACE_EDGE_EAST;
        return TRUE;
    }
    if (top)
    {
        *out_edge = GDK_SURFACE_EDGE_NORTH;
        return TRUE;
    }
    if (bottom)
    {
        *out_edge = GDK_SURFACE_EDGE_SOUTH;
        return TRUE;
    }

    return FALSE;
}


static const char *cursor_name_for_edge(GdkSurfaceEdge edge) {
    switch (edge) {
        case GDK_SURFACE_EDGE_NORTH_WEST:
        case GDK_SURFACE_EDGE_SOUTH_EAST:
            return "nwse-resize";
        case GDK_SURFACE_EDGE_NORTH_EAST:
        case GDK_SURFACE_EDGE_SOUTH_WEST:
            return "nesw-resize";
        case GDK_SURFACE_EDGE_WEST:
        case GDK_SURFACE_EDGE_EAST:
            return "ew-resize";
        case GDK_SURFACE_EDGE_NORTH:
        case GDK_SURFACE_EDGE_SOUTH:
            return "ns-resize";
        default:
            return "default";
    }
}

static GtkWidget *last_resize_hovered_widget = NULL;

static void on_window_pointer_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
    (void)controller;
    GtkWidget *overlay = GTK_WIDGET(user_data);
    GtkWidget *window = gtk_widget_get_ancestor(overlay, GTK_TYPE_WINDOW);
    if (!window) return;

    graphene_point_t pt_in = GRAPHENE_POINT_INIT((float)x, (float)y);
    graphene_point_t pt_win;
    if (!gtk_widget_compute_point(overlay, window, &pt_in, &pt_win)) {
        pt_win = pt_in;
    }

    GtkWidget *picked = gtk_widget_pick(overlay, x, y, GTK_PICK_DEFAULT);
    GdkSurfaceEdge edge;

    if (detect_resize_edge(window, pt_win.x, pt_win.y, &edge)) {
        const char *cname = cursor_name_for_edge(edge);

        if (last_resize_hovered_widget && last_resize_hovered_widget != picked) {
            gtk_widget_set_cursor_from_name(last_resize_hovered_widget, NULL);
            g_clear_weak_pointer(&last_resize_hovered_widget);
        }

        gtk_widget_set_cursor_from_name(window, cname);
        gtk_widget_set_cursor_from_name(overlay, cname);

        if (picked && GTK_IS_WIDGET(picked)) {
            gtk_widget_set_cursor_from_name(picked, cname);
            g_set_weak_pointer(&last_resize_hovered_widget, picked);
        }
    } else {
        if (last_resize_hovered_widget) {
            gtk_widget_set_cursor_from_name(last_resize_hovered_widget, NULL);
            g_clear_weak_pointer(&last_resize_hovered_widget);
        }
        gtk_widget_set_cursor_from_name(window, NULL);
        gtk_widget_set_cursor_from_name(overlay, NULL);
    }
}

static void on_window_border_pressed(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    (void)n_press;
    GtkWidget *window = GTK_WIDGET(user_data);
    GdkSurfaceEdge edge;

    if (detect_resize_edge(window, x, y, &edge))
    {
        GtkNative *native = gtk_widget_get_native(window);
        GdkSurface *surface = gtk_native_get_surface(native);
        GdkEvent *event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), NULL);

        if (GDK_IS_TOPLEVEL(surface) && event != NULL)
        {
            GdkDevice *device = gdk_event_get_device(event);
            guint32 timestamp = gdk_event_get_time(event);
            gdk_toplevel_begin_resize(GDK_TOPLEVEL(surface), edge, device, GDK_BUTTON_PRIMARY, x, y, timestamp);
            gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
        }
    }
}

/* --- True Alpha-Mask Container for Smooth Breadcrumb Edge Fading --- */
typedef struct
{
    GtkWidget parent_instance;
    gboolean fade_left;
    gboolean fade_right;
} BreadcrumbFadeBox;

typedef struct
{
    GtkWidgetClass parent_class;
} BreadcrumbFadeBoxClass;

G_DEFINE_TYPE(BreadcrumbFadeBox, breadcrumb_fade_box, GTK_TYPE_WIDGET)

static void breadcrumb_fade_box_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
    BreadcrumbFadeBox *self = (BreadcrumbFadeBox *)widget;
    GtkWidget *child = gtk_widget_get_first_child(widget);
    if (!child)
        return;

    int w = gtk_widget_get_width(widget);
    int h = gtk_widget_get_height(widget);

    if ((!self->fade_left && !self->fade_right) || w <= 60 || h <= 0)
    {
        gtk_widget_snapshot_child(widget, child, snapshot);
        return;
    }

    const float fade_px = 28.0f;
    graphene_rect_t bounds = GRAPHENE_RECT_INIT(0.0f, 0.0f, (float)w, (float)h);

    gtk_snapshot_push_mask(snapshot, GSK_MASK_MODE_ALPHA);

    GskColorStop stops[4];
    int n_stops = 0;

    if (self->fade_left)
    {
        stops[n_stops++] = (GskColorStop){0.0f, {1.0f, 1.0f, 1.0f, 0.0f}};
        stops[n_stops++] = (GskColorStop){MIN(0.45f, fade_px / (float)w), {1.0f, 1.0f, 1.0f, 1.0f}};
    }
    else
    {
        stops[n_stops++] = (GskColorStop){0.0f, {1.0f, 1.0f, 1.0f, 1.0f}};
    }

    if (self->fade_right)
    {
        stops[n_stops++] = (GskColorStop){MAX(0.55f, 1.0f - (fade_px / (float)w)), {1.0f, 1.0f, 1.0f, 1.0f}};
        stops[n_stops++] = (GskColorStop){1.0f, {1.0f, 1.0f, 1.0f, 0.0f}};
    }
    else
    {
        stops[n_stops++] = (GskColorStop){1.0f, {1.0f, 1.0f, 1.0f, 1.0f}};
    }

    gtk_snapshot_append_linear_gradient(snapshot,
                                        &bounds,
                                        &GRAPHENE_POINT_INIT(0.0f, 0.0f),
                                        &GRAPHENE_POINT_INIT((float)w, 0.0f),
                                        stops,
                                        n_stops);
    gtk_snapshot_pop(snapshot); /* End mask definition */

    gtk_widget_snapshot_child(widget, child, snapshot);
    gtk_snapshot_pop(snapshot); /* Apply mask to child */
}

static void breadcrumb_fade_box_dispose(GObject *object)
{
    GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(object));
    if (child)
        gtk_widget_unparent(child);
    G_OBJECT_CLASS(breadcrumb_fade_box_parent_class)->dispose(object);
}

static void breadcrumb_fade_box_class_init(BreadcrumbFadeBoxClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = breadcrumb_fade_box_dispose;
    GtkWidgetClass *wclass = GTK_WIDGET_CLASS(klass);
    wclass->snapshot = breadcrumb_fade_box_snapshot;
    gtk_widget_class_set_layout_manager_type(wclass, GTK_TYPE_BIN_LAYOUT);
}

static void breadcrumb_fade_box_init(BreadcrumbFadeBox *self)
{
    self->fade_left = FALSE;
    self->fade_right = FALSE;
}

static void on_crumb_hadj_updated(GtkAdjustment *hadj, gpointer user_data)
{
    BreadcrumbFadeBox *self = (BreadcrumbFadeBox *)user_data;
    double val = gtk_adjustment_get_value(hadj);
    double upper = gtk_adjustment_get_upper(hadj);
    double page = gtk_adjustment_get_page_size(hadj);

    gboolean new_left = (val > 2.0);
    gboolean new_right = (upper - page - val > 2.0);

    if (self->fade_left != new_left || self->fade_right != new_right)
    {
        self->fade_left = new_left;
        self->fade_right = new_right;
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
}

/* --- Mouse Side-Button Navigation (Button 8 = Back, Button 9 = Forward) --- */
static void on_window_mouse_nav_pressed(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    (void)n_press;
    (void)x;
    (void)y;
    AppState *state = (AppState *)user_data;
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));

    if (button == 8)
    {
        on_back_clicked(NULL, state);
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
    else if (button == 9)
    {
        on_fwd_clicked(NULL, state);
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
}

/* --- Header Preferences Button --- */
static void on_header_settings_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    show_settings_dialog((AppState *)user_data);
}

/* --- Window Control Buttons --- */
static void on_minimize_clicked(GtkButton *btn, GtkWindow *window)
{
    (void)btn;
    gtk_window_minimize(window);
}

static void on_maximize_clicked(GtkButton *btn, GtkWindow *window)
{
    (void)btn;
    if (gtk_window_is_maximized(window))
    {
        gtk_window_unmaximize(window);
    }
    else
    {
        gtk_window_maximize(window);
    }
}

static void on_close_clicked(GtkButton *btn, GtkWindow *window)
{
    (void)btn;
    gtk_window_close(window);
}

/* --- Root "/" Button & Copy Current Path on Right-Click --- */
static void on_root_slash_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AppState *state = (AppState *)user_data;
    navigate_to_path(state, "/", TRUE);
}

static gboolean close_slash_copied_popover_cb(gpointer user_data)
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

static void on_slash_right_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    (void)n_press;
    (void)x;
    (void)y;
    AppState *state = (AppState *)user_data;
    if (!state->current_path)
        return;

    GdkClipboard *clipboard = gdk_display_get_clipboard(gdk_display_get_default());
    gdk_clipboard_set_text(clipboard, state->current_path);

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
    g_timeout_add(1100, close_slash_copied_popover_cb, g_object_ref(popover));
}

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
/* --- View Mode Switching (Grid vs List) --- */
static void on_grid_view_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AppState *state = (AppState *)user_data;
    if (!state->is_list_view)
        return;
    state->is_list_view = FALSE;
    gtk_widget_add_css_class(state->grid_btn, "active");
    gtk_widget_remove_css_class(state->list_btn, "active");
    save_app_settings(state);
    if (state->current_path)
        populate_directory(state, state->current_path);
}
static void on_list_view_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    AppState *state = (AppState *)user_data;
    if (state->is_list_view)
        return;
    state->is_list_view = TRUE;
    gtk_widget_add_css_class(state->list_btn, "active");
    gtk_widget_remove_css_class(state->grid_btn, "active");
    save_app_settings(state);
    if (state->current_path)
        populate_directory(state, state->current_path);
}
static gboolean on_breadcrumb_scroll(GtkEventControllerScroll *ctrl, double dx, double dy, gpointer user_data)
{
    (void)ctrl;
    GtkScrolledWindow *sw = GTK_SCROLLED_WINDOW(user_data);
    GtkAdjustment *hadj = gtk_scrolled_window_get_hadjustment(sw);
    double delta = (fabs(dx) > fabs(dy)) ? dx : dy;
    double new_val = gtk_adjustment_get_value(hadj) + delta * 45.0;
    gtk_adjustment_set_value(hadj, new_val);
    return TRUE;
}
/* --- Nautilus-Style 5-Tier Zoom (Ctrl + Mouse Wheel & Ctrl + +/-/0) --- */
static guint zoom_save_timer_id = 0;
static double wheel_zoom_accum = 0.0;
static gboolean save_col_zoom_cb(gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    zoom_save_timer_id = 0;
    save_app_settings(state);
    return G_SOURCE_REMOVE;
}
static void step_grid_columns(AppState *state, int tier_delta)
{
    if (state->is_list_view)
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
    }
    zoom_save_timer_id = g_timeout_add(300, save_col_zoom_cb, state);
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
/* --- Keyboard Shortcuts --- */
static gboolean on_window_key_pressed(GtkEventControllerKey *controller,
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
        gtk_widget_grab_focus(state->search_entry);
        return TRUE;
    }
    if (keyval == GDK_KEY_Escape)
    {
        clear_item_selection(state);
        const char *txt = gtk_editable_get_text(GTK_EDITABLE(state->search_entry));
        if (txt && *txt != '\0')
        {
            gtk_editable_set_text(GTK_EDITABLE(state->search_entry), "");
        }
        return TRUE;
    }
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
    return FALSE;
}
/* --- Main Application Window Assembly --- */
void activate_app(GtkApplication *app, gpointer user_data)
{
    (void)user_data;
    GtkCssProvider *provider = gtk_css_provider_new();
    if (g_file_test("src/style.css", G_FILE_TEST_EXISTS))
    {
        gtk_css_provider_load_from_path(provider, "src/style.css");
    }
    else if (g_file_test("style.css", G_FILE_TEST_EXISTS))
    {
        gtk_css_provider_load_from_path(provider, "style.css");
    }
    else
    {
        gtk_css_provider_load_from_path(provider, DATADIR "/style.css");
    }
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    AppState *state = g_new0(AppState, 1);
    state->is_list_view = FALSE;
    state->show_hidden = FALSE;
    state->enable_thumbnails = TRUE;
    state->double_click_open = FALSE;
    state->grid_cols = 2;
    state->sort_column = SORT_BY_NAME;
    state->sort_ascending = TRUE;
    load_app_settings(state);
    state->grid_cols = CLAMP(state->grid_cols, 0, 4);
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Custom File Manager");
    gtk_window_set_icon_name(GTK_WINDOW(window), "blurred_files");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 720);
    gtk_widget_set_size_request(window, 640, 480);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    g_object_set_data(G_OBJECT(window), "app-state", state);
    state->window = window;
    /* Mouse side-button navigation (Back / Forward) */
    GtkGesture *mouse_nav = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(mouse_nav), 0);
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(mouse_nav), GTK_PHASE_CAPTURE);
    g_signal_connect(mouse_nav, "pressed", G_CALLBACK(on_window_mouse_nav_pressed), state);
    gtk_widget_add_controller(window, GTK_EVENT_CONTROLLER(mouse_nav));
    /* Capture-phase keyboard shortcuts */
    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(key_ctrl, GTK_PHASE_CAPTURE);
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_window_key_pressed), state);
    gtk_widget_add_controller(window, key_ctrl);
    GtkWidget *overlay = gtk_overlay_new();
    gtk_window_set_child(GTK_WINDOW(window), overlay);
    /* 8-way window edge resize controllers attached directly to overlay */
    GtkEventController *motion_ctrl = gtk_event_controller_motion_new();
    gtk_event_controller_set_propagation_phase(motion_ctrl, GTK_PHASE_CAPTURE);
    g_signal_connect(motion_ctrl, "motion", G_CALLBACK(on_window_pointer_motion), overlay);
    gtk_widget_add_controller(overlay, motion_ctrl);
    GtkGesture *resize_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(resize_click), GDK_BUTTON_PRIMARY);
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(resize_click), GTK_PHASE_CAPTURE);
    g_signal_connect(resize_click, "pressed", G_CALLBACK(on_window_border_pressed), window);
    gtk_widget_add_controller(overlay, GTK_EVENT_CONTROLLER(resize_click));
    GtkWidget *bg_canvas = gtk_drawing_area_new();
    gtk_widget_set_can_target(bg_canvas, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(bg_canvas), on_draw_squircle_bg, NULL, NULL);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), bg_canvas);
    GtkWidget *main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(main_hbox, "window-inner");
    /* Left Sidebar */
    gtk_box_append(GTK_BOX(main_hbox), build_sidebar_widget(state));
    /* Right Content Pane */
    GtkWidget *content_pane = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(content_pane, "content-controls");
    gtk_widget_set_hexpand(content_pane, TRUE);
    GtkWidget *window_handle = gtk_window_handle_new();
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(header_box, "squircle-header-content");
    gtk_window_handle_set_child(GTK_WINDOW_HANDLE(window_handle), header_box);
    /* 1. Left & Right Arrow Buttons Together (No Gap) */
    GtkWidget *back_btn = gtk_button_new_from_icon_name("go-previous-symbolic");
    GtkWidget *fwd_btn = gtk_button_new_from_icon_name("go-next-symbolic");
    gtk_widget_remove_css_class(back_btn, "circular");
    gtk_widget_remove_css_class(fwd_btn, "circular");
    gtk_widget_add_css_class(back_btn, "nav-btn");
    gtk_widget_add_css_class(fwd_btn, "nav-btn");
    gtk_widget_set_size_request(back_btn, 30, 30);
    gtk_widget_set_size_request(fwd_btn, 30, 30);
    state->back_btn = back_btn;
    state->fwd_btn = fwd_btn;
    g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), state);
    g_signal_connect(fwd_btn, "clicked", G_CALLBACK(on_fwd_clicked), state);
    GtkWidget *nav_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(nav_group, "joined-btn-group");
    gtk_widget_set_valign(nav_group, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(nav_group), back_btn);
    gtk_box_append(GTK_BOX(nav_group), fwd_btn);
    /* 2. Root "/" + "|" + Tight Breadcrumbs with True Alpha Edge Fade */
    GtkWidget *root_btn = gtk_button_new_with_label("/");
    gtk_widget_add_css_class(root_btn, "breadcrumb-pill");
    gtk_widget_set_size_request(root_btn, -1, 30);
    gtk_widget_set_valign(root_btn, GTK_ALIGN_CENTER);
    state->root_btn = root_btn;
    g_signal_connect(root_btn, "clicked", G_CALLBACK(on_root_slash_clicked), state);
    GtkGesture *slash_rclick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(slash_rclick), GDK_BUTTON_SECONDARY);
    g_signal_connect(slash_rclick, "pressed", G_CALLBACK(on_slash_right_clicked), state);
    gtk_widget_add_controller(root_btn, GTK_EVENT_CONTROLLER(slash_rclick));
    GtkWidget *pipe_lbl = gtk_label_new("|");
    gtk_widget_add_css_class(pipe_lbl, "slash-label");
    gtk_widget_set_valign(pipe_lbl, GTK_ALIGN_CENTER);
    GtkWidget *breadcrumb_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(breadcrumb_scroll), GTK_POLICY_EXTERNAL, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(breadcrumb_scroll), FALSE);
    gtk_widget_set_hexpand(breadcrumb_scroll, TRUE);
    gtk_widget_set_valign(breadcrumb_scroll, GTK_ALIGN_CENTER);
    state->breadcrumb_scroll = breadcrumb_scroll;
    GtkEventController *crumb_scroll_ctrl = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
    g_signal_connect(crumb_scroll_ctrl, "scroll", G_CALLBACK(on_breadcrumb_scroll), breadcrumb_scroll);
    gtk_widget_add_controller(breadcrumb_scroll, crumb_scroll_ctrl);
    GtkWidget *breadcrumb_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(breadcrumb_box, "breadcrumb-container");
    gtk_widget_set_valign(breadcrumb_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(breadcrumb_box, 8);
    gtk_widget_set_margin_bottom(breadcrumb_box, 8);
    gtk_widget_set_margin_start(breadcrumb_box, 8);
    gtk_widget_set_margin_end(breadcrumb_box, 8);
    state->breadcrumb_box = breadcrumb_box;
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(breadcrumb_scroll), breadcrumb_box);
    GtkWidget *crumb_fade_wrap = g_object_new(breadcrumb_fade_box_get_type(), NULL);
    gtk_widget_set_hexpand(crumb_fade_wrap, TRUE);
    gtk_widget_set_valign(crumb_fade_wrap, GTK_ALIGN_CENTER);
    gtk_widget_set_parent(breadcrumb_scroll, crumb_fade_wrap);
    GtkAdjustment *crumb_hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(breadcrumb_scroll));
    g_signal_connect(crumb_hadj, "value-changed", G_CALLBACK(on_crumb_hadj_updated), crumb_fade_wrap);
    g_signal_connect(crumb_hadj, "changed", G_CALLBACK(on_crumb_hadj_updated), crumb_fade_wrap);
    GtkWidget *crumb_section = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_hexpand(crumb_section, TRUE);
    gtk_widget_set_valign(crumb_section, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(crumb_section, 14);
    gtk_widget_set_margin_end(crumb_section, 14);
    gtk_box_append(GTK_BOX(crumb_section), root_btn);
    gtk_box_append(GTK_BOX(crumb_section), pipe_lbl);
    gtk_box_append(GTK_BOX(crumb_section), crumb_fade_wrap);
    /* 3. Icon & List View Buttons Together (No Gap) */
    GtkWidget *grid_btn = gtk_button_new_from_icon_name("view-grid-symbolic");
    GtkWidget *list_btn = gtk_button_new_from_icon_name("view-list-symbolic");
    gtk_widget_remove_css_class(grid_btn, "circular");
    gtk_widget_remove_css_class(list_btn, "circular");
    gtk_widget_add_css_class(grid_btn, "view-btn");
    gtk_widget_add_css_class(list_btn, "view-btn");
    gtk_widget_add_css_class(state->is_list_view ? list_btn : grid_btn, "active");
    gtk_widget_set_size_request(grid_btn, 30, 30);
    gtk_widget_set_size_request(list_btn, 30, 30);
    state->grid_btn = grid_btn;
    state->list_btn = list_btn;
    g_signal_connect(grid_btn, "clicked", G_CALLBACK(on_grid_view_clicked), state);
    g_signal_connect(list_btn, "clicked", G_CALLBACK(on_list_view_clicked), state);
    GtkWidget *view_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(view_group, "joined-btn-group");
    gtk_widget_set_valign(view_group, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(view_group), grid_btn);
    gtk_box_append(GTK_BOX(view_group), list_btn);
    /* 4. Search Bar & Settings Gear Button (Little Spacing) */
    GtkWidget *search_entry = gtk_search_entry_new();
    gtk_widget_add_css_class(search_entry, "header-search");
    gtk_widget_set_size_request(search_entry, 165, 30);
    gtk_widget_set_valign(search_entry, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(search_entry, 8);
    state->search_entry = search_entry;
    g_signal_connect(search_entry, "search-changed", G_CALLBACK(on_search_changed), state);
    GtkWidget *settings_btn = gtk_button_new_from_icon_name("preferences-system-symbolic");
    gtk_widget_remove_css_class(settings_btn, "circular");
    gtk_widget_add_css_class(settings_btn, "nav-btn");
    gtk_widget_set_size_request(settings_btn, 30, 30);
    gtk_widget_set_valign(settings_btn, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(settings_btn, 8);
    g_signal_connect(settings_btn, "clicked", G_CALLBACK(on_header_settings_clicked), state);
    /* 5. System Window Buttons Close Together (4px gap) */
    GtkWidget *min_btn = gtk_button_new_from_icon_name("window-minimize-symbolic");
    GtkWidget *max_btn = gtk_button_new_from_icon_name("window-maximize-symbolic");
    GtkWidget *close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(min_btn, "win-ctrl-btn");
    gtk_widget_add_css_class(max_btn, "win-ctrl-btn");
    gtk_widget_add_css_class(close_btn, "win-ctrl-btn");
    gtk_widget_set_size_request(min_btn, 30, 30);
    gtk_widget_set_size_request(max_btn, 30, 30);
    gtk_widget_set_size_request(close_btn, 30, 30);
    g_signal_connect(min_btn, "clicked", G_CALLBACK(on_minimize_clicked), window);
    g_signal_connect(max_btn, "clicked", G_CALLBACK(on_maximize_clicked), window);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_close_clicked), window);
    GtkWidget *win_ctrl_group = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_valign(win_ctrl_group, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(win_ctrl_group, 16);
    gtk_box_append(GTK_BOX(win_ctrl_group), min_btn);
    gtk_box_append(GTK_BOX(win_ctrl_group), max_btn);
    gtk_box_append(GTK_BOX(win_ctrl_group), close_btn);
    /* Assemble Header Bar */
    gtk_box_append(GTK_BOX(header_box), nav_group);
    gtk_box_append(GTK_BOX(header_box), crumb_section);
    gtk_box_append(GTK_BOX(header_box), view_group);
    gtk_box_append(GTK_BOX(header_box), search_entry);
    gtk_box_append(GTK_BOX(header_box), settings_btn);
    gtk_box_append(GTK_BOX(header_box), win_ctrl_group);
    gtk_box_append(GTK_BOX(content_pane), window_handle);
    /* Clickable List View Column Header Bar */
    GtkWidget *list_header_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_widget_add_css_class(list_header_bar, "list-column-headers");
    gtk_widget_set_visible(list_header_bar, state->is_list_view);
    state->list_header_bar = list_header_bar;
    GtkWidget *icon_col_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(icon_col_spacer, 32, -1);
    GtkWidget *col_name = gtk_button_new_with_label("Name ▲");
    gtk_widget_add_css_class(col_name, "list-col-btn");
    gtk_widget_set_hexpand(col_name, TRUE);
    gtk_widget_set_halign(col_name, GTK_ALIGN_FILL);
    gtk_button_set_has_frame(GTK_BUTTON(col_name), FALSE);
    GtkWidget *col_type = gtk_button_new_with_label("Type");
    gtk_widget_add_css_class(col_type, "list-col-btn");
    gtk_widget_set_size_request(col_type, 130, -1);
    gtk_button_set_has_frame(GTK_BUTTON(col_type), FALSE);
    GtkWidget *col_date = gtk_button_new_with_label("Modified");
    gtk_widget_add_css_class(col_date, "list-col-btn");
    gtk_widget_set_size_request(col_date, 145, -1);
    gtk_button_set_has_frame(GTK_BUTTON(col_date), FALSE);
    GtkWidget *col_size = gtk_button_new_with_label("Size");
    gtk_widget_add_css_class(col_size, "list-col-btn");
    gtk_widget_set_size_request(col_size, 75, -1);
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
    gtk_box_append(GTK_BOX(list_header_bar), col_type);
    gtk_box_append(GTK_BOX(list_header_bar), col_date);
    gtk_box_append(GTK_BOX(list_header_bar), col_size);
    gtk_box_append(GTK_BOX(content_pane), list_header_bar);
    /* Overlay around ScrolledWindow to draw Rubberband Rectangle Selection Marquee */
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
    gtk_overlay_set_child(GTK_OVERLAY(grid_overlay), scrolled);
    /* Selection Marquee Box */
    GtkWidget *selection_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(selection_box, "selection-marquee");
    gtk_widget_set_halign(selection_box, GTK_ALIGN_START);
    gtk_widget_set_valign(selection_box, GTK_ALIGN_START);
    gtk_widget_set_can_target(selection_box, FALSE);
    gtk_widget_set_visible(selection_box, FALSE);
    state->selection_box = selection_box;
    gtk_overlay_add_overlay(GTK_OVERLAY(grid_overlay), selection_box);
    /* Ctrl + Mouse Wheel 5-tier icon zoom */
    GtkEventController *zoom_scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    gtk_event_controller_set_propagation_phase(zoom_scroll, GTK_PHASE_CAPTURE);
    g_signal_connect(zoom_scroll, "scroll", G_CALLBACK(on_grid_scroll_zoom), state);
    gtk_widget_add_controller(grid_overlay, zoom_scroll);
    /* Left-drag gesture for whitespace-only rubberband selection */
    GtkGesture *drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), GDK_BUTTON_PRIMARY);
    g_signal_connect(drag, "drag-begin", G_CALLBACK(on_drag_begin), state);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update), state);
    g_signal_connect(drag, "drag-end", G_CALLBACK(on_drag_end), state);
    gtk_widget_add_controller(grid_overlay, GTK_EVENT_CONTROLLER(drag));
    /* Right-click gesture on empty space */
    GtkGesture *bg_rclick = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(bg_rclick), GDK_BUTTON_SECONDARY);
    g_signal_connect(bg_rclick, "pressed", G_CALLBACK(on_grid_bg_right_clicked), state);
    gtk_widget_add_controller(grid_overlay, GTK_EVENT_CONTROLLER(bg_rclick));
    /* Cross-window drop target onto the directory background */
    GType bg_drop_types[] = {GDK_TYPE_FILE_LIST, G_TYPE_FILE};
    GtkDropTarget *bg_drop_tgt = gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_MOVE | GDK_ACTION_COPY);
    gtk_drop_target_set_gtypes(bg_drop_tgt, bg_drop_types, G_N_ELEMENTS(bg_drop_types));
    g_signal_connect(bg_drop_tgt, "drop", G_CALLBACK(on_grid_bg_drop), state);
    gtk_widget_add_controller(grid_overlay, GTK_EVENT_CONTROLLER(bg_drop_tgt));
    gtk_box_append(GTK_BOX(content_pane), grid_overlay);
    /* Bottom Status Bar (Centered & Shifted 5px Down) */
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(status_bar, "bottom-status-bar");
    gtk_widget_set_halign(status_bar, GTK_ALIGN_CENTER);
    GtkWidget *status_label = gtk_label_new("0 items (0 bytes)");
    gtk_widget_add_css_class(status_label, "status-bar-label");
    gtk_widget_set_halign(status_label, GTK_ALIGN_CENTER);
    state->status_label = status_label;
    gtk_box_append(GTK_BOX(status_bar), status_label);
    gtk_box_append(GTK_BOX(content_pane), status_bar);
    gtk_box_append(GTK_BOX(main_hbox), content_pane);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), main_hbox);
    navigate_to_path(state, g_get_home_dir(), FALSE);
    gtk_window_present(GTK_WINDOW(window));
}