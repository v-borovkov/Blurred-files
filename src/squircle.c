#include "app.h"
#include <math.h>

static void cairo_squircle_path(cairo_t *cr, double x, double y, double w, double h, double rx, double ry, double n) {
    if (rx > w / 2.0) rx = w / 2.0;
    if (ry > h / 2.0) ry = h / 2.0;

    const int steps = 96;
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

    double n_outer = 3.8;
    double n_card  = 4.0;
    double sidebar_visible_w = 108.0;

    double card_x = x + sidebar_visible_w;
    double card_w = w - sidebar_visible_w;

    double rx_outer = is_max ? 0.0 : w * 0.41;
    double ry_outer = is_max ? 0.0 : 88.0;

    double rx_card  = is_max ? 0.0 : card_w * 0.41;
    double ry_card  = is_max ? 0.0 : 88.0;

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_restore(cr);

    /* 0. Custom Superellipse Window Drop Shadow */
    if (!is_max) {
        for (int i = 14; i >= 1; i--) {
            double spread = (double)i * 0.95;
            double y_off  = (double)i * 0.35;
            double alpha  = 0.008 + (14.0 - i) * 0.0022;

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
    cairo_squircle_path(cr, x, y, w, h, rx_outer, ry_outer, n_outer);
    cairo_clip(cr);

    /* 1. Translucent Slate-Charcoal Sidebar with Subtle 3D Gradient (Wayfire blur shines through) */
    cairo_pattern_t *sb_grad = cairo_pattern_create_linear(x, y, x + sidebar_visible_w, y + h);
    cairo_pattern_add_color_stop_rgba(sb_grad, 0.00, 44.0 / 255.0, 50.0 / 255.0, 64.0 / 255.0, 0.48);
    cairo_pattern_add_color_stop_rgba(sb_grad, 0.50, 28.0 / 255.0, 32.0 / 255.0, 42.0 / 255.0, 0.45);
    cairo_pattern_add_color_stop_rgba(sb_grad, 1.00, 16.0 / 255.0, 19.0 / 255.0, 26.0 / 255.0, 0.50);
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

    /* 3. Deeper 3D Ambient + Contact Shadow Cast by the White Content Card onto the Sidebar */
    for (int i = 16; i >= 1; i--) {
        cairo_save(cr);
        cairo_squircle_path(cr, card_x - i * 1.15, y, card_w, h, rx_card, ry_card, n_card);
        double alpha = 0.010 + (16.0 - i) * 0.0024;
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    /* 4. Crisp Rim Highlight along the Left Curved Edge of the White Content Card */
    cairo_save(cr);
    cairo_squircle_path(cr, card_x - 0.5, y, card_w, h, rx_card, ry_card, n_card);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.55);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
    cairo_restore(cr);

    /* Clip & Fill Solid White Content Card */
    cairo_save(cr);
    cairo_squircle_path(cr, card_x, y, card_w, h, rx_card, ry_card, n_card);
    cairo_clip_preserve(cr);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_fill(cr);

    /* Concave-Bottom 3D Bright Cold Ice-Blue Header inside Top of Content Card */
    double hdr_center_h = 74.0;
    double hdr_ear_dip  = 24.0;

    /* Soft Cool Ambient + Contact Drop Shadow cast onto the White Card */
    for (int i = 14; i >= 1; i--) {
        cairo_save(cr);
        cairo_concave_header_path(cr, card_x, y + i * 1.15, card_w, hdr_center_h, hdr_ear_dip);
        double alpha = 0.008 + (14.0 - i) * 0.0018;
        cairo_set_source_rgba(cr, 95.0 / 255.0, 120.0 / 255.0, 155.0 / 255.0, alpha);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    /* 3D Top-Lit Bright Cold Ice-Blue Gradient Fill */
    cairo_pattern_t *cold_grad = cairo_pattern_create_linear(card_x, y, card_x, y + hdr_center_h + hdr_ear_dip);
    cairo_pattern_add_color_stop_rgba(cold_grad, 0.00, 244.0 / 255.0, 248.0 / 255.0, 254.0 / 255.0, 0.98);
    cairo_pattern_add_color_stop_rgba(cold_grad, 0.45, 229.0 / 255.0, 238.0 / 255.0, 249.0 / 255.0, 0.97);
    cairo_pattern_add_color_stop_rgba(cold_grad, 0.90, 212.0 / 255.0, 225.0 / 255.0, 242.0 / 255.0, 0.98);
    cairo_pattern_add_color_stop_rgba(cold_grad, 1.00, 198.0 / 255.0, 214.0 / 255.0, 235.0 / 255.0, 0.99);

    cairo_concave_header_path(cr, card_x, y, card_w, hdr_center_h, hdr_ear_dip);
    cairo_set_source(cr, cold_grad);
    cairo_fill_preserve(cr);
    cairo_pattern_destroy(cold_grad);

    /* Cool Steel-Blue Lower Edge Bevel Stroke */
    cairo_set_source_rgba(cr, 150.0 / 255.0, 172.0 / 255.0, 202.0 / 255.0, 0.65);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    /* Inner Top Specular White Highlight Rim */
    cairo_save(cr);
    cairo_concave_header_path(cr, card_x, y, card_w, hdr_center_h, hdr_ear_dip);
    cairo_clip(cr);

    cairo_squircle_path(cr, card_x + 1.0, y + 1.2, card_w - 2.0, h - 2.0, rx_card, ry_card, n_card);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.85);
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
    cairo_restore(cr);

    cairo_restore(cr);
    cairo_restore(cr);

    /* Thin Crisp Grey Border around Outer Silhouette */
    if (!is_max) {
        cairo_squircle_path(cr, x + 0.5, y + 0.5, w - 1.0, h - 1.0, rx_outer, ry_outer, n_outer);
        cairo_set_source_rgba(cr, 180.0 / 255.0, 185.0 / 255.0, 195.0 / 255.0, 0.55);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }
}