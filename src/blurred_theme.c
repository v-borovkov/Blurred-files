#include "blurred_theme.h"
#include <math.h>
#include <string.h>

static BlurredSharedTheme g_cur;
static BlurredSharedTheme g_tgt;
static gboolean g_inited = FALSE;
static GFileMonitor *g_monitor = NULL;
static GtkCssProvider *g_dyn_css = NULL;
static GPtrArray *g_canvases = NULL;
static guint g_anim_id = 0;

static void parse_hex_rgba(const char *hex, GdkRGBA *out, const char *fallback)
{
    if (!hex || !gdk_rgba_parse(out, hex))
    {
        gdk_rgba_parse(out, fallback);
    }
}

static void theme_set_defaults(BlurredSharedTheme *t)
{
    g_free(t->accent_name);
    g_free(t->accent_start_hex);
    g_free(t->accent_mid_hex);
    g_free(t->accent_end_hex);
    memset(t, 0, sizeof(*t));

    t->is_light = FALSE;
    t->blur_enabled = TRUE;
    t->glass_opacity = 0.48;
    t->shadow_enabled = TRUE;
    t->shadow_intensity = 1.0;
    t->accent_name = g_strdup("orange");
    t->accent_start_hex = g_strdup("#FFB857");
    t->accent_mid_hex = g_strdup("#FF6B00");
    t->accent_end_hex = g_strdup("#D14000");
    parse_hex_rgba(t->accent_start_hex, &t->accent_start, "#FFB857");
    parse_hex_rgba(t->accent_mid_hex, &t->accent_mid, "#FF6B00");
    parse_hex_rgba(t->accent_end_hex, &t->accent_end, "#D14000");
    t->rx_ratio = 0.41;
    t->ry = 88.0;
    t->exponent_n = 3.8;
}

static void theme_copy_from(BlurredSharedTheme *dst, const BlurredSharedTheme *src)
{
    g_free(dst->accent_name);
    g_free(dst->accent_start_hex);
    g_free(dst->accent_mid_hex);
    g_free(dst->accent_end_hex);
    *dst = *src;
    dst->accent_name = g_strdup(src->accent_name ? src->accent_name : "orange");
    dst->accent_start_hex = g_strdup(src->accent_start_hex ? src->accent_start_hex : "#FFB857");
    dst->accent_mid_hex = g_strdup(src->accent_mid_hex ? src->accent_mid_hex : "#FF6B00");
    dst->accent_end_hex = g_strdup(src->accent_end_hex ? src->accent_end_hex : "#D14000");
}

static char *get_theme_ini_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "blurred", "theme.ini", NULL);
}

static void load_target_from_disk(void)
{
    BlurredSharedTheme next;
    memset(&next, 0, sizeof(next));
    theme_set_defaults(&next);

    char *path = get_theme_ini_path();
    GKeyFile *kf = g_key_file_new();
    if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL))
    {
        if (g_key_file_has_key(kf, "Appearance", "mode", NULL))
        {
            char *mode = g_key_file_get_string(kf, "Appearance", "mode", NULL);
            next.is_light = (g_strcmp0(mode, "light") == 0);
            g_free(mode);
        }
        if (g_key_file_has_key(kf, "Appearance", "blur_enabled", NULL))
            next.blur_enabled = g_key_file_get_boolean(kf, "Appearance", "blur_enabled", NULL);
        if (g_key_file_has_key(kf, "Appearance", "glass_opacity", NULL))
            next.glass_opacity = CLAMP(g_key_file_get_double(kf, "Appearance", "glass_opacity", NULL), 0.20, 0.98);
        if (g_key_file_has_key(kf, "Appearance", "shadow_enabled", NULL))
            next.shadow_enabled = g_key_file_get_boolean(kf, "Appearance", "shadow_enabled", NULL);
        if (g_key_file_has_key(kf, "Appearance", "shadow_intensity", NULL))
            next.shadow_intensity = CLAMP(g_key_file_get_double(kf, "Appearance", "shadow_intensity", NULL), 0.0, 2.0);

        char *name = g_key_file_get_string(kf, "Appearance", "accent", NULL);
        char *s = g_key_file_get_string(kf, "Appearance", "accent_start", NULL);
        char *m = g_key_file_get_string(kf, "Appearance", "accent_mid", NULL);
        char *e = g_key_file_get_string(kf, "Appearance", "accent_end", NULL);
        if (name && s && m && e)
        {
            g_free(next.accent_name);
            g_free(next.accent_start_hex);
            g_free(next.accent_mid_hex);
            g_free(next.accent_end_hex);
            next.accent_name = g_strdup(name);
            next.accent_start_hex = g_strdup(s);
            next.accent_mid_hex = g_strdup(m);
            next.accent_end_hex = g_strdup(e);
            parse_hex_rgba(s, &next.accent_start, "#FFB857");
            parse_hex_rgba(m, &next.accent_mid, "#FF6B00");
            parse_hex_rgba(e, &next.accent_end, "#D14000");
        }
        g_free(name);
        g_free(s);
        g_free(m);
        g_free(e);

        if (g_key_file_has_key(kf, "Geometry", "rx_ratio", NULL))
            next.rx_ratio = CLAMP(g_key_file_get_double(kf, "Geometry", "rx_ratio", NULL), 0.05, 0.50);
        if (g_key_file_has_key(kf, "Geometry", "ry", NULL))
            next.ry = CLAMP(g_key_file_get_double(kf, "Geometry", "ry", NULL), 12.0, 180.0);
        if (g_key_file_has_key(kf, "Geometry", "exponent_n", NULL))
            next.exponent_n = CLAMP(g_key_file_get_double(kf, "Geometry", "exponent_n", NULL), 2.0, 8.0);
    }
    g_key_file_free(kf);
    g_free(path);

    theme_copy_from(&g_tgt, &next);
    theme_set_defaults(&next);
    g_free(next.accent_name);
    g_free(next.accent_start_hex);
    g_free(next.accent_mid_hex);
    g_free(next.accent_end_hex);
}

static void sync_css_and_windows(void)
{
    if (!g_dyn_css)
        return;

    char *css = g_strdup_printf(
        "@define-color accent_start %s;\n"
        "@define-color accent_mid %s;\n"
        "@define-color accent_end %s;\n"
        "@define-color accent_glow alpha(%s, 0.45);\n"
        "@define-color accent_tint_soft alpha(%s, 0.18);\n"
        "@define-color accent_tint_strong alpha(%s, 0.28);\n",
        g_cur.accent_start_hex ? g_cur.accent_start_hex : "#FFB857",
        g_cur.accent_mid_hex ? g_cur.accent_mid_hex : "#FF6B00",
        g_cur.accent_end_hex ? g_cur.accent_end_hex : "#D14000",
        g_cur.accent_mid_hex ? g_cur.accent_mid_hex : "#FF6B00",
        g_cur.accent_mid_hex ? g_cur.accent_mid_hex : "#FF6B00",
        g_cur.accent_mid_hex ? g_cur.accent_mid_hex : "#FF6B00");
    gtk_css_provider_load_from_string(g_dyn_css, css);
    g_free(css);

    GListModel *toplevels = gtk_window_get_toplevels();
    guint n = g_list_model_get_n_items(toplevels);
    for (guint i = 0; i < n; i++)
    {
        GtkWidget *w = g_list_model_get_item(toplevels, i);
        if (GTK_IS_WIDGET(w))
        {
            if (g_cur.is_light)
                gtk_widget_add_css_class(w, "theme-light");
            else
                gtk_widget_remove_css_class(w, "theme-light");
            g_object_unref(w);
        }
    }

    if (g_canvases)
    {
        for (guint i = 0; i < g_canvases->len; i++)
        {
            GtkWidget *canvas = g_ptr_array_index(g_canvases, i);
            if (GTK_IS_WIDGET(canvas))
                gtk_widget_queue_draw(canvas);
        }
    }
}

static gboolean on_morph_tick(gpointer user_data)
{
    (void)user_data;
    const double k = 0.24;
    gboolean more = FALSE;

    g_cur.is_light = g_tgt.is_light;
    g_cur.blur_enabled = g_tgt.blur_enabled;
    g_cur.shadow_enabled = g_tgt.shadow_enabled;
    g_cur.accent_start = g_tgt.accent_start;
    g_cur.accent_mid = g_tgt.accent_mid;
    g_cur.accent_end = g_tgt.accent_end;

    g_free(g_cur.accent_name);
    g_free(g_cur.accent_start_hex);
    g_free(g_cur.accent_mid_hex);
    g_free(g_cur.accent_end_hex);
    g_cur.accent_name = g_strdup(g_tgt.accent_name);
    g_cur.accent_start_hex = g_strdup(g_tgt.accent_start_hex);
    g_cur.accent_mid_hex = g_strdup(g_tgt.accent_mid_hex);
    g_cur.accent_end_hex = g_strdup(g_tgt.accent_end_hex);

    double d_rx = g_tgt.rx_ratio - g_cur.rx_ratio;
    double d_ry = g_tgt.ry - g_cur.ry;
    double d_n  = g_tgt.exponent_n - g_cur.exponent_n;
    double d_op = g_tgt.glass_opacity - g_cur.glass_opacity;
    double d_sh = g_tgt.shadow_intensity - g_cur.shadow_intensity;

    if (fabs(d_rx) > 0.0005 || fabs(d_ry) > 0.15 || fabs(d_n) > 0.005 || fabs(d_op) > 0.002 || fabs(d_sh) > 0.005)
    {
        g_cur.rx_ratio += d_rx * k;
        g_cur.ry += d_ry * k;
        g_cur.exponent_n += d_n * k;
        g_cur.glass_opacity += d_op * k;
        g_cur.shadow_intensity += d_sh * k;
        more = TRUE;
    }
    else
    {
        g_cur.rx_ratio = g_tgt.rx_ratio;
        g_cur.ry = g_tgt.ry;
        g_cur.exponent_n = g_tgt.exponent_n;
        g_cur.glass_opacity = g_tgt.glass_opacity;
        g_cur.shadow_intensity = g_tgt.shadow_intensity;
    }

    sync_css_and_windows();
    if (!more)
    {
        g_anim_id = 0;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static void on_theme_file_changed(GFileMonitor *monitor,
                                  GFile *file,
                                  GFile *other_file,
                                  GFileMonitorEvent event_type,
                                  gpointer user_data)
{
    (void)monitor;
    (void)file;
    (void)other_file;
    (void)user_data;
    if (event_type == G_FILE_MONITOR_EVENT_CHANGED ||
        event_type == G_FILE_MONITOR_EVENT_CREATED ||
        event_type == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED ||
        event_type == G_FILE_MONITOR_EVENT_MOVED_IN)
    {
        load_target_from_disk();
        if (g_anim_id == 0)
            g_anim_id = g_timeout_add(16, on_morph_tick, NULL);
    }
}

static void on_canvas_destroyed(GtkWidget *widget, gpointer user_data)
{
    (void)user_data;
    if (g_canvases)
        g_ptr_array_remove_fast(g_canvases, widget);
}

void blurred_theme_register_canvas(GtkWidget *widget)
{
    if (!widget)
        return;
    if (!g_inited)
        blurred_theme_init();
    if (!g_canvases)
        g_canvases = g_ptr_array_new();
    g_ptr_array_add(g_canvases, widget);
    g_signal_connect(widget, "destroy", G_CALLBACK(on_canvas_destroyed), NULL);
    if (g_cur.is_light)
    {
        GtkWidget *top = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);
        if (top)
            gtk_widget_add_css_class(top, "theme-light");
    }
}

void blurred_theme_init(void)
{
    if (g_inited)
        return;
    g_inited = TRUE;

    theme_set_defaults(&g_cur);
    theme_set_defaults(&g_tgt);
    load_target_from_disk();
    theme_copy_from(&g_cur, &g_tgt);

    GdkDisplay *disp = gdk_display_get_default();
    if (disp)
    {
        g_dyn_css = gtk_css_provider_new();
        gtk_style_context_add_provider_for_display(
            disp,
            GTK_STYLE_PROVIDER(g_dyn_css),
            GTK_STYLE_PROVIDER_PRIORITY_USER + 10);
        sync_css_and_windows();
    }

    char *path = get_theme_ini_path();
    char *dir = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);

    char *d2 = g_path_get_dirname(path); GFile *f = g_file_new_for_path(d2); g_free(d2);
    g_free(path);
    g_monitor = g_file_monitor_directory(f, G_FILE_MONITOR_NONE, NULL, NULL);
g_object_unref(f);
if (g_monitor)
g_signal_connect(g_monitor, "changed", G_CALLBACK(on_theme_file_changed), NULL);
}
const BlurredSharedTheme *blurred_theme_get(void)
{
if (!g_inited)
blurred_theme_init();
return &g_cur;
}
void blurred_theme_shutdown(void)
{
if (!g_inited)
return;
if (g_anim_id)
{
g_source_remove(g_anim_id);
g_anim_id = 0;
}
g_clear_object(&g_monitor);
g_clear_object(&g_dyn_css);
g_clear_pointer(&g_canvases, g_ptr_array_unref);
g_inited = FALSE;
}
