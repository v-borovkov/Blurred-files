#include "app.h"
#include <math.h>

static void cairo_squircle_path(cairo_t *cr, double x, double y, double w, double h, double rx, double ry, double n) {
    if (rx > w / 2.0) rx = w / 2.0;
    if (ry > h / 2.0) ry = h / 2.0;

    const int steps = 240;
    const double exp_val = 2.0 / n;

    double cx_tr = x + w - rx, cy_tr = y + ry;
    double cx_br = x + w - rx, cy_br = y + h - ry;
    double cx_bl = x + rx,     cy_bl = y + h - ry;
    double cx_tl = x + rx,     cy_tl = y + ry;

    cairo_new_sub_path(cr);

    for (int i = 0; i <= steps; i++) {
        double t = -G_PI_2 + (G_PI_2 * i) / steps;
        double cos_t = cos(t), sin_t = sin(t);
        double px = cx_tr + rx * copysign(pow(fabs(cos_t), exp_val), cos_t);
        double py = cy_tr + ry * copysign(pow(fabs(sin_t), exp_val), sin_t);
        if (i == 0) cairo_move_to(cr, px, py);
        else cairo_line_to(cr, px, py);
    }

    for (int i = 0; i <= steps; i++) {
        double t = (G_PI_2 * i) / steps;
        double cos_t = cos(t), sin_t = sin(t);
        double px = cx_br + rx * copysign(pow(fabs(cos_t), exp_val), cos_t);
        double py = cy_br + ry * copysign(pow(fabs(sin_t), exp_val), sin_t);
        cairo_line_to(cr, px, py);
    }

    for (int i = 0; i <= steps; i++) {
        double t = G_PI_2 + (G_PI_2 * i) / steps;
        double cos_t = cos(t), sin_t = sin(t);
        double px = cx_bl + rx * copysign(pow(fabs(cos_t), exp_val), cos_t);
        double py = cy_bl + ry * copysign(pow(fabs(sin_t), exp_val), sin_t);
        cairo_line_to(cr, px, py);
    }

    for (int i = 0; i <= steps; i++) {
        double t = G_PI + (G_PI_2 * i) / steps;
        double cos_t = cos(t), sin_t = sin(t);
        double px = cx_tl + rx * copysign(pow(fabs(cos_t), exp_val), cos_t);
        double py = cy_tl + ry * copysign(pow(fabs(sin_t), exp_val), sin_t);
        cairo_line_to(cr, px, py);
    }

    cairo_close_path(cr);
}

static void cairo_concave_header_path(cairo_t *cr, double x, double y, double w, double h_center, double dip) {
    double y_bottom = y + h_center;
    double y_tip    = y + h_center + dip;
    double curve_w  = 46.0;

    cairo_new_sub_path(cr);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + w, y);
    cairo_line_to(cr, x + w, y_tip);

    cairo_curve_to(cr,
                   x + w, y_bottom,
                   x + w - curve_w * 0.45, y_bottom,
                   x + w - curve_w, y_bottom);

    cairo_line_to(cr, x + curve_w, y_bottom);

    cairo_curve_to(cr,
                   x + curve_w * 0.45, y_bottom,
                   x, y_bottom,
                   x, y_tip);

    cairo_close_path(cr);
}

void on_draw_squircle_bg(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    (void)data;

    if (width <= 0 || height <= 0) return;

    GtkWidget *toplevel = gtk_widget_get_ancestor(GTK_WIDGET(area), GTK_TYPE_WINDOW);
    gboolean is_max = (toplevel && GTK_IS_WINDOW(toplevel))
        ? gtk_window_is_maximized(GTK_WINDOW(toplevel))
        : FALSE;

    double pad = is_max ? 0.0 : 14.0;
    double x = pad;
    double y = pad;
    double w = (double)width - pad * 2.0;
    double h = (double)height - pad * 2.0;

    if (w <= 0 || h <= 0) return;

    const BlurredSharedTheme *bt = blurred_theme_get();
    double n_outer = bt->exponent_n;
    double n_card  = MIN(8.0, bt->exponent_n + 0.2);
    double sidebar_visible_w = 108.0;

    double card_x = x + sidebar_visible_w;
    double card_w = w - sidebar_visible_w;

    double rx_outer = is_max ? 0.0 : w * bt->rx_ratio;
    double ry_outer = is_max ? 0.0 : bt->ry;

    double rx_card  = is_max ? 0.0 : card_w * bt->rx_ratio;
    double ry_card  = is_max ? 0.0 : bt->ry;

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);

    /* 0. Custom Superellipse Window Drop Shadow (live from blurred-settings) */
    if (!is_max && bt->shadow_enabled && bt->shadow_intensity > 0.01) {
        for (int i = 14; i >= 1; i--) {
            double spread = (double)i * 0.95;
            double y_off  = (double)i * 0.35;
            double alpha  = (0.008 + (14.0 - i) * 0.0022) * bt->shadow_intensity;

            cairo_save(cr);
            cairo_squircle_path(cr,
                                x - spread,
                                y - spread + y_off,
                                w + spread * 2.0,
                                h + spread * 2.0,
                                rx_outer + spread,
                                ry_outer + spread,
                                n_outer);
            cairo_set_source_rgba(cr, 10.0 / 255.0, 14.0 / 255.0, 22.0 / 255.0, alpha);
            cairo_fill(cr);
            cairo_restore(cr);
        }
    }

    cairo_save(cr);
    cairo_squircle_path(cr, x + 1.2, y + 1.2, w - 2.4, h - 2.4, rx_outer, ry_outer, n_outer);
    cairo_clip(cr);

    /* 1. Translucent Sidebar (Dark Slate or Frosted Alabaster, scales with glass_opacity) */
    double op = bt->blur_enabled ? bt->glass_opacity : MIN(0.96, bt->glass_opacity + 0.35);
    cairo_pattern_t *sb_grad = cairo_pattern_create_linear(x, y, x + sidebar_visible_w, y + h);
    if (bt->is_light) {
        cairo_pattern_add_color_stop_rgba(sb_grad, 0.00, 240.0 / 255.0, 244.0 / 255.0, 250.0 / 255.0, op + 0.14);
        cairo_pattern_add_color_stop_rgba(sb_grad, 0.50, 228.0 / 255.0, 234.0 / 255.0, 244.0 / 255.0, op + 0.10);
        cairo_pattern_add_color_stop_rgba(sb_grad, 1.00, 216.0 / 255.0, 224.0 / 255.0, 236.0 / 255.0, op + 0.16);
    } else {
        cairo_pattern_add_color_stop_rgba(sb_grad, 0.00, 44.0 / 255.0, 50.0 / 255.0, 64.0 / 255.0, op);
        cairo_pattern_add_color_stop_rgba(sb_grad, 0.50, 28.0 / 255.0, 32.0 / 255.0, 42.0 / 255.0, MAX(0.20, op - 0.03));
        cairo_pattern_add_color_stop_rgba(sb_grad, 1.00, 16.0 / 255.0, 19.0 / 255.0, 26.0 / 255.0, MIN(0.98, op + 0.02));
    }
    cairo_set_source(cr, sb_grad);
    cairo_paint(cr);
    cairo_pattern_destroy(sb_grad);

    /* 2. Inner Left & Top Specular Glass Bevel along the Outer Superellipse Rim */
    if (!is_max) {
        cairo_save(cr);
        cairo_squircle_path(cr, x + 1.2, y + 1.2, w - 2.4, h - 2.4, rx_outer, ry_outer, n_outer);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.24);
        cairo_set_line_width(cr, 1.4);
        cairo_stroke(cr);
        cairo_restore(cr);
    }

    /* 3. Deeper 3D Ambient + Contact Shadow Cast by the Content Card onto the Sidebar */
    for (int i = 16; i >= 1; i--) {
        cairo_save(cr);
        cairo_squircle_path(cr, card_x - i * 1.15, y, card_w, h, rx_card, ry_card, n_card);
        double alpha = 0.012 + (16.0 - i) * 0.0028;
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    /* 4. Subtle Rim Highlight along the Left Curved Edge of the Content Card */
    cairo_save(cr);
    cairo_squircle_path(cr, card_x - 0.5, y, card_w, h, rx_card, ry_card, n_card);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.22);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
    cairo_restore(cr);

    /* Clip & Fill Deep Charcoal Content Card (Darker than the Header Bar) */
    cairo_save(cr);
    cairo_squircle_path(cr, card_x, y, card_w, h, rx_card, ry_card, n_card);
    cairo_clip_preserve(cr);

    cairo_pattern_t *card_grad = cairo_pattern_create_linear(card_x, y, card_x, y + h);
    if (bt->is_light) {
        cairo_pattern_add_color_stop_rgb(card_grad, 0.0, 248.0 / 255.0, 250.0 / 255.0, 253.0 / 255.0);
        cairo_pattern_add_color_stop_rgb(card_grad, 1.0, 238.0 / 255.0, 242.0 / 255.0, 248.0 / 255.0);
    } else {
        cairo_pattern_add_color_stop_rgb(card_grad, 0.0, 35.0 / 255.0, 40.0 / 255.0, 52.0 / 255.0);
        cairo_pattern_add_color_stop_rgb(card_grad, 1.0, 26.0 / 255.0, 30.0 / 255.0, 40.0 / 255.0);
    }
    cairo_set_source(cr, card_grad);
    cairo_fill(cr);
    cairo_pattern_destroy(card_grad);

    /* Concave-Bottom 3D Opaque Header inside Top of Content Card */
    double hdr_center_h = 74.0;
    double hdr_ear_dip  = 24.0;

    for (int i = 16; i >= 1; i--) {
        cairo_save(cr);
        cairo_concave_header_path(cr, card_x, y + i * 1.25, card_w, hdr_center_h, hdr_ear_dip);
        double alpha = (bt->is_light ? 0.008 : 0.016) + (16.0 - i) * (bt->is_light ? 0.0018 : 0.0034);
        cairo_set_source_rgba(cr, 8.0 / 255.0, 10.0 / 255.0, 16.0 / 255.0, alpha);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    cairo_pattern_t *dark_hdr_grad = cairo_pattern_create_linear(card_x, y, card_x, y + hdr_center_h + hdr_ear_dip);
    if (bt->is_light) {
        cairo_pattern_add_color_stop_rgb(dark_hdr_grad, 0.00, 255.0 / 255.0, 255.0 / 255.0, 255.0 / 255.0);
        cairo_pattern_add_color_stop_rgb(dark_hdr_grad, 0.45, 244.0 / 255.0, 247.0 / 255.0, 252.0 / 255.0);
        cairo_pattern_add_color_stop_rgb(dark_hdr_grad, 0.90, 232.0 / 255.0, 237.0 / 255.0, 245.0 / 255.0);
        cairo_pattern_add_color_stop_rgb(dark_hdr_grad, 1.00, 224.0 / 255.0, 230.0 / 255.0, 240.0 / 255.0);
    } else {
        cairo_pattern_add_color_stop_rgb(dark_hdr_grad, 0.00, 86.0 / 255.0, 94.0 / 255.0, 112.0 / 255.0);
        cairo_pattern_add_color_stop_rgb(dark_hdr_grad, 0.45, 70.0 / 255.0, 77.0 / 255.0, 93.0 / 255.0);
        cairo_pattern_add_color_stop_rgb(dark_hdr_grad, 0.90, 56.0 / 255.0, 62.0 / 255.0, 76.0 / 255.0);
        cairo_pattern_add_color_stop_rgb(dark_hdr_grad, 1.00, 48.0 / 255.0, 53.0 / 255.0, 66.0 / 255.0);
    }

    cairo_concave_header_path(cr, card_x, y, card_w, hdr_center_h, hdr_ear_dip);
    cairo_set_source(cr, dark_hdr_grad);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(dark_hdr_grad);

    /* Crisp Cool-Grey Lower Edge Bevel Stroke */
    cairo_set_source_rgba(cr, 165.0 / 255.0, 178.0 / 255.0, 204.0 / 255.0, 0.45);
    cairo_set_line_width(cr, 1.3);
    cairo_stroke(cr);

    /* Inner Top Specular White Highlight Rim */
    cairo_save(cr);
    cairo_concave_header_path(cr, card_x, y, card_w, hdr_center_h, hdr_ear_dip);
    cairo_clip(cr);

    cairo_squircle_path(cr, card_x + 1.0, y + 1.2, card_w - 2.0, h - 2.0, rx_card, ry_card, n_card);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.34);
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
    cairo_restore(cr);

    cairo_restore(cr);
    cairo_restore(cr);

    /* 2px Warm Orange Gradient Border around Outer Superellipse Silhouette */
    if (!is_max) {
        cairo_save(cr);
        cairo_squircle_path(cr, x + 1.0, y + 1.0, w - 2.0, h - 2.0, rx_outer, ry_outer, n_outer);

        cairo_pattern_t *rim_grad = cairo_pattern_create_linear(x, y, x + w, y + h);
        cairo_pattern_add_color_stop_rgba(rim_grad, 0.0, bt->accent_start.red, bt->accent_start.green, bt->accent_start.blue, 0.92);
        cairo_pattern_add_color_stop_rgba(rim_grad, 0.5, bt->accent_mid.red, bt->accent_mid.green, bt->accent_mid.blue, 0.95);
        cairo_pattern_add_color_stop_rgba(rim_grad, 1.0, bt->accent_end.red, bt->accent_end.green, bt->accent_end.blue, 0.90);

        cairo_set_source(cr, rim_grad);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);

        cairo_pattern_destroy(rim_grad);
        cairo_restore(cr);
    }
}