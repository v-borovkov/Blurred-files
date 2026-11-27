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

static const char *cursor_name_for_edge(GdkSurfaceEdge edge)
{
    switch (edge)
    {
    case GDK_SURFACE_EDGE_NORTH_WEST:
        return "nw-resize";
    case GDK_SURFACE_EDGE_SOUTH_EAST:
        return "se-resize";
    case GDK_SURFACE_EDGE_NORTH_EAST:
        return "ne-resize";
    case GDK_SURFACE_EDGE_SOUTH_WEST:
        return "sw-resize";
    case GDK_SURFACE_EDGE_WEST:
        return "w-resize";
    case GDK_SURFACE_EDGE_EAST:
        return "e-resize";
    case GDK_SURFACE_EDGE_NORTH:
        return "n-resize";
    case GDK_SURFACE_EDGE_SOUTH:
        return "s-resize";
    default:
        return "default";
    }
}

static GtkWidget *last_resize_hovered_widget = NULL;
static const char *active_resize_cursor_name = NULL;

static void apply_resize_cursor_at(GtkWidget *overlay, double x, double y)
{
    GtkWidget *window = gtk_widget_get_ancestor(overlay, GTK_TYPE_WINDOW);
    if (!window)
        return;

    GtkNative *native = gtk_widget_get_native(window);
    GdkSurface *surface = native ? gtk_native_get_surface(native) : NULL;

    graphene_point_t pt_in = GRAPHENE_POINT_INIT((float)x, (float)y);
    graphene_point_t pt_win;
    if (!gtk_widget_compute_point(overlay, window, &pt_in, &pt_win))
    {
        pt_win = pt_in;
    }

    GtkWidget *picked = gtk_widget_pick(overlay, x, y, GTK_PICK_DEFAULT);
    GdkSurfaceEdge edge;

    if (detect_resize_edge(window, pt_win.x, pt_win.y, &edge))
    {
        const char *cname = cursor_name_for_edge(edge);

        if (last_resize_hovered_widget && last_resize_hovered_widget != picked)
        {
            gtk_widget_set_cursor_from_name(last_resize_hovered_widget, NULL);
            g_clear_weak_pointer(&last_resize_hovered_widget);
        }

        gtk_widget_set_cursor_from_name(window, cname);
        gtk_widget_set_cursor_from_name(overlay, cname);

        if (picked && GTK_IS_WIDGET(picked))
        {
            gtk_widget_set_cursor_from_name(picked, cname);
            g_set_weak_pointer(&last_resize_hovered_widget, picked);
        }

        if (surface && g_strcmp0(active_resize_cursor_name, cname) != 0)
        {
            GdkCursor *cursor = gdk_cursor_new_from_name(cname, NULL);
            if (cursor)
            {
                gdk_surface_set_cursor(surface, cursor);
                g_object_unref(cursor);
            }
            active_resize_cursor_name = cname;
        }
    }
    else if (active_resize_cursor_name != NULL)
    {
        if (last_resize_hovered_widget)
        {
            gtk_widget_set_cursor_from_name(last_resize_hovered_widget, NULL);
            g_clear_weak_pointer(&last_resize_hovered_widget);
        }
        gtk_widget_set_cursor_from_name(window, NULL);
        gtk_widget_set_cursor_from_name(overlay, NULL);

        if (surface)
        {
            gdk_surface_set_cursor(surface, NULL);
        }
        active_resize_cursor_name = NULL;
    }
}

static void on_window_pointer_enter(GtkEventControllerMotion *controller, double x, double y, gpointer user_data)
{
    (void)controller;
    apply_resize_cursor_at(GTK_WIDGET(user_data), x, y);
}

static void on_window_pointer_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data)
{
    (void)controller;
    apply_resize_cursor_at(GTK_WIDGET(user_data), x, y);
}

static void on_window_pointer_leave(GtkEventControllerMotion *controller, gpointer user_data)
{
    (void)controller;
    GtkWidget *overlay = GTK_WIDGET(user_data);
    GtkWidget *window = gtk_widget_get_ancestor(overlay, GTK_TYPE_WINDOW);

    if (last_resize_hovered_widget)
    {
        gtk_widget_set_cursor_from_name(last_resize_hovered_widget, NULL);
        g_clear_weak_pointer(&last_resize_hovered_widget);
    }
    if (window)
    {
        gtk_widget_set_cursor_from_name(window, NULL);
        GtkNative *native = gtk_widget_get_native(window);
        GdkSurface *surface = native ? gtk_native_get_surface(native) : NULL;
        if (surface)
        {
            gdk_surface_set_cursor(surface, NULL);
        }
    }
    gtk_widget_set_cursor_from_name(overlay, NULL);
    active_resize_cursor_name = NULL;
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

/* --- Adaptive Header Padding on Ultra-Wide Windows --- */
static void on_bg_canvas_resized(GtkDrawingArea *area, int width, int height, gpointer user_data)
{
    (void)height;
    GtkWidget *header_box = GTK_WIDGET(user_data);
    GtkWidget *window = gtk_widget_get_ancestor(GTK_WIDGET(area), GTK_TYPE_WINDOW);
    gboolean is_max = (window && GTK_IS_WINDOW(window))
                          ? gtk_window_is_maximized(GTK_WINDOW(window))
                          : FALSE;

    int extra = (!is_max && width > 1200) ? MIN(140, (width - 1200) / 6) : 0;
    gtk_widget_set_margin_start(header_box, extra);
    gtk_widget_set_margin_end(header_box, extra);
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
    gtk_snapshot_pop(snapshot);

    gtk_widget_snapshot_child(widget, child, snapshot);
    gtk_snapshot_pop(snapshot);
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
/* --- Header & Window Control Callbacks --- */
static void on_header_settings_clicked(GtkButton *btn, gpointer user_data)
{
    (void)btn;
    show_settings_dialog((AppState *)user_data);
}
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
/* --- Main Application Window Assembly --- */
void activate_app_with_path(GtkApplication *app, const char *initial_path)
{
    g_clear_weak_pointer(&last_resize_hovered_widget);
    active_resize_cursor_name = NULL;
    GtkCssProvider *provider = gtk_css_provider_new();
    if (g_file_test("src/bfiles.css", G_FILE_TEST_EXISTS))
    {
        gtk_css_provider_load_from_path(provider, "src/bfiles.css");
    }
    else if (g_file_test("bfiles.css", G_FILE_TEST_EXISTS))
    {
        gtk_css_provider_load_from_path(provider, "bfiles.css");
    }
    else
    {
        gtk_css_provider_load_from_path(provider, DATADIR "/bfiles.css");
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
    state->col_type_w = 130;
    state->col_date_w = 145;
    state->col_size_w = 75;
    load_app_settings(state);
    state->grid_cols = CLAMP(state->grid_cols, 0, 4);
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Blurred Files");
    gtk_window_set_icon_name(GTK_WINDOW(window), "blurred_files");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);
    gtk_widget_set_size_request(window, 820, 480);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    g_object_set_data(G_OBJECT(window), "app-state", state);
    state->window = window;
    GtkGesture *mouse_nav = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(mouse_nav), 0);
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(mouse_nav), GTK_PHASE_CAPTURE);
    g_signal_connect(mouse_nav, "pressed", G_CALLBACK(on_window_mouse_nav_pressed), state);
    gtk_widget_add_controller(window, GTK_EVENT_CONTROLLER(mouse_nav));
    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(key_ctrl, GTK_PHASE_CAPTURE);
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_window_key_pressed), state);
    gtk_widget_add_controller(window, key_ctrl);
    GtkWidget *overlay = gtk_overlay_new();
    gtk_window_set_child(GTK_WINDOW(window), overlay);
    GtkEventController *motion_ctrl = gtk_event_controller_motion_new();
    gtk_event_controller_set_propagation_phase(motion_ctrl, GTK_PHASE_CAPTURE);
    g_signal_connect(motion_ctrl, "enter", G_CALLBACK(on_window_pointer_enter), overlay);
    g_signal_connect(motion_ctrl, "motion", G_CALLBACK(on_window_pointer_motion), overlay);
    g_signal_connect(motion_ctrl, "leave", G_CALLBACK(on_window_pointer_leave), overlay);
    gtk_widget_add_controller(overlay, motion_ctrl);
    GtkGesture *resize_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(resize_click), GDK_BUTTON_PRIMARY);
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(resize_click), GTK_PHASE_CAPTURE);
    g_signal_connect(resize_click, "pressed", G_CALLBACK(on_window_border_pressed), window);
    gtk_widget_add_controller(overlay, GTK_EVENT_CONTROLLER(resize_click));
    GtkWidget *bg_canvas = gtk_drawing_area_new();
    gtk_widget_set_can_target(bg_canvas, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(bg_canvas), on_draw_squircle_bg, NULL, NULL);
    blurred_theme_register_canvas(bg_canvas);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), bg_canvas);
    GtkWidget *main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(main_hbox, "window-inner");
    gtk_box_append(GTK_BOX(main_hbox), build_sidebar_widget(state));
    GtkWidget *content_pane = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(content_pane, "content-controls");
    gtk_widget_set_hexpand(content_pane, TRUE);
    GtkWidget *window_handle = gtk_window_handle_new();
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(header_box, "squircle-header-content");
    gtk_window_handle_set_child(GTK_WINDOW_HANDLE(window_handle), header_box);
    g_signal_connect(bg_canvas, "resize", G_CALLBACK(on_bg_canvas_resized), header_box);
    /* 1. Back & Forward Buttons */
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
    /* 2. Root "/" + Breadcrumbs */
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
    gtk_widget_set_margin_top(breadcrumb_box, 15);
    gtk_widget_set_margin_bottom(breadcrumb_box, 15);
    gtk_widget_set_margin_start(breadcrumb_box, 12);
    gtk_widget_set_margin_end(breadcrumb_box, 12);
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
    /* 3. Grid / List View Buttons */
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
    /* 4. Search Bar & Settings */
    GtkWidget *search_entry = gtk_search_entry_new();
    gtk_widget_add_css_class(search_entry, "header-search");
    gtk_widget_set_size_request(search_entry, 150, 30);
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
    /* 5. Window Controls */
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
    gtk_box_append(GTK_BOX(header_box), nav_group);
    gtk_box_append(GTK_BOX(header_box), crumb_section);
    gtk_box_append(GTK_BOX(header_box), view_group);
    gtk_box_append(GTK_BOX(header_box), search_entry);
    gtk_box_append(GTK_BOX(header_box), settings_btn);
    gtk_box_append(GTK_BOX(header_box), win_ctrl_group);
    gtk_box_append(GTK_BOX(content_pane), window_handle);
    /* List View Column Headers (with Resizable Splitters) */
    gtk_box_append(GTK_BOX(content_pane), build_list_column_headers(state));
    /* Shared File Grid & Marquee Overlay */
    gtk_box_append(GTK_BOX(content_pane), build_file_grid_overlay(state));
    /* Bottom Status Bar */
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
    const char *start_dir = (initial_path && g_file_test(initial_path, G_FILE_TEST_IS_DIR))
                                ? initial_path
                                : g_get_home_dir();
    navigate_to_path(state, start_dir, FALSE);
    gtk_window_present(GTK_WINDOW(window));
}
void activate_app(GtkApplication *app, gpointer user_data)
{
    (void)user_data;
    activate_app_with_path(app, NULL);
}