#ifndef BLURRED_THEME_H
#define BLURRED_THEME_H

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef struct
{
    gboolean is_light;
    gboolean blur_enabled;
    double glass_opacity;
    gboolean shadow_enabled;
    double shadow_intensity;

    char *accent_name;
    GdkRGBA accent_start;
    GdkRGBA accent_mid;
    GdkRGBA accent_end;
    char *accent_start_hex;
    char *accent_mid_hex;
    char *accent_end_hex;

    double rx_ratio;
    double ry;
    double exponent_n;
} BlurredSharedTheme;

void blurred_theme_init(void);
void blurred_theme_shutdown(void);
const BlurredSharedTheme *blurred_theme_get(void);
void blurred_theme_register_canvas(GtkWidget *widget);

#endif /* BLURRED_THEME_H */
