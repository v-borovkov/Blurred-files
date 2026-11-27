#include "app.h"
#include <gtk4-layer-shell.h>

static guint settings_reload_timer_id = 0;

static gboolean reload_desktop_settings_cb(gpointer user_data)
{
    AppState *state = (AppState *)user_data;
    settings_reload_timer_id = 0;

    if (state && state->current_path && GTK_IS_WIDGET(state->flowbox))
    {
        load_app_settings(state);
        state->is_list_view = FALSE; /* Desktop always stays in icon grid mode */
        populate_directory(state, state->current_path);
        for (GtkWidget *c = gtk_widget_get_first_child(state->flowbox); c != NULL; c = gtk_widget_get_next_sibling(c)) { gtk_widget_set_visible(c, !state->hide_desktop_icons); }
    gtk_widget_set_can_target(state->flowbox, !state->hide_desktop_icons);
    }
    return G_SOURCE_REMOVE;
}

static void on_settings_file_changed(GFileMonitor *monitor,
                                     GFile *file,
                                     GFile *other_file,
                                     GFileMonitorEvent event_type,
                                     gpointer user_data)
{
    (void)monitor;
    (void)file;
    (void)other_file;

    if (event_type == G_FILE_MONITOR_EVENT_CHANGED ||
        event_type == G_FILE_MONITOR_EVENT_CREATED ||
        event_type == G_FILE_MONITOR_EVENT_MOVED_IN ||
        event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    {
        if (settings_reload_timer_id != 0)
        {
            g_source_remove(settings_reload_timer_id);
        }
        settings_reload_timer_id = g_timeout_add(120, reload_desktop_settings_cb, user_data);
    }
}

static void on_draw_desktop_clear_anchor(GtkDrawingArea *area, cairo_t *cr, int w, int h, gpointer data)
{
    (void)area; (void)w; (void)h; (void)data;
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
}

void activate_desktop_mode(GtkApplication *app, gpointer user_data)
{
    blurred_theme_init();
    (void)user_data;

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
    state->is_desktop_mode = TRUE;
    state->is_list_view = FALSE;
    state->show_hidden = FALSE;
    state->enable_thumbnails = TRUE;
    state->double_click_open = FALSE;
    state->grid_cols = 2;
    state->sort_column = SORT_BY_NAME;
    state->sort_ascending = TRUE;
    load_app_settings(state);
    state->is_list_view = FALSE;
    state->grid_cols = CLAMP(state->grid_cols, 0, 4);

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Blurred Desktop");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_widget_add_css_class(window, "desktop-mode-window");
    g_object_set_data(G_OBJECT(window), "app-state", state);
    state->window = window;

    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_BOTTOM);
    gtk_layer_set_namespace(GTK_WINDOW(window), "blurred-desktop");
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(window), 0);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

    /* Keep desktop icons clear of top panel and bottom/left dock */
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, 44);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, 76);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, 12);
    gtk_layer_set_margin(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, 12);

    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(key_ctrl, GTK_PHASE_CAPTURE);
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_window_key_pressed), state);
    gtk_widget_add_controller(window, key_ctrl);

    GtkWidget *grid_overlay = build_file_grid_overlay(state);
    gtk_widget_add_css_class(grid_overlay, "desktop-grid-overlay");
    gtk_widget_add_css_class(state->flowbox, "desktop-files-grid");
    GtkWidget *clear_anchor = gtk_drawing_area_new();
    gtk_widget_set_size_request(clear_anchor, 1, 1);
    gtk_widget_set_halign(clear_anchor, GTK_ALIGN_START);
    gtk_widget_set_valign(clear_anchor, GTK_ALIGN_START);
    gtk_widget_set_can_target(clear_anchor, FALSE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(clear_anchor), on_draw_desktop_clear_anchor, NULL, NULL);
    gtk_overlay_add_overlay(GTK_OVERLAY(grid_overlay), clear_anchor);
    gtk_window_set_child(GTK_WINDOW(window), grid_overlay);

    /* Live-sync with ~/.config/blurred_files/settings.ini whenever Preferences change */
    char *cfg_dir = g_build_filename(g_get_user_config_dir(), "blurred_files", NULL);
    g_mkdir_with_parents(cfg_dir, 0755);
    char *cfg_file = g_build_filename(cfg_dir, "settings.ini", NULL);
    GFile *settings_gfile = g_file_new_for_path(cfg_file);
    GFileMonitor *settings_mon = g_file_monitor_file(settings_gfile, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);
    if (settings_mon)
    {
        g_signal_connect(settings_mon, "changed", G_CALLBACK(on_settings_file_changed), state);
        g_object_set_data_full(G_OBJECT(window), "settings-monitor", settings_mon, g_object_unref);
    }
    g_object_unref(settings_gfile);
    g_free(cfg_file);
    g_free(cfg_dir);

    const char *xdg_desktop = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
    char *desktop_dir = (xdg_desktop && g_strcmp0(xdg_desktop, g_get_home_dir()) != 0)
                            ? g_strdup(xdg_desktop)
                            : g_build_filename(g_get_home_dir(), "Desktop", NULL);
    if (!g_file_test(desktop_dir, G_FILE_TEST_IS_DIR))
    {
        g_mkdir_with_parents(desktop_dir, 0755);
    }

    navigate_to_path(state, desktop_dir, FALSE);
    for (GtkWidget *c = gtk_widget_get_first_child(state->flowbox); c != NULL; c = gtk_widget_get_next_sibling(c)) { gtk_widget_set_visible(c, !state->hide_desktop_icons); }
    gtk_widget_set_can_target(state->flowbox, !state->hide_desktop_icons);
    g_free(desktop_dir);

    gtk_window_present(GTK_WINDOW(window));
}