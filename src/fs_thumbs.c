#include "app.h"

typedef struct {
    char *file_path;
    int target_w;
    int target_h;
} ThumbJobData;

static void free_thumb_job_data(gpointer data) {
    ThumbJobData *job = (ThumbJobData *)data;
    g_free(job->file_path);
    g_free(job);
}

static void load_thumbnail_worker(GTask *task,
                                  gpointer source_object,
                                  gpointer task_data,
                                  GCancellable *cancellable) {
    if (g_cancellable_is_cancelled(cancellable)) {
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
        return;
    }

    ThumbJobData *job = (ThumbJobData *)task_data;
    GError *err = NULL;

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(
        job->file_path, job->target_w, job->target_h, TRUE, &err
    );

    if (!pixbuf) {
        g_task_return_error(task, err);
        return;
    }

    if (g_cancellable_is_cancelled(cancellable)) {
        g_object_unref(pixbuf);
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
        return;
    }

    GdkPixbuf *rgba = gdk_pixbuf_get_has_alpha(pixbuf)
        ? g_object_ref(pixbuf)
        : gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
    g_object_unref(pixbuf);

    int w = gdk_pixbuf_get_width(rgba);
    int h = gdk_pixbuf_get_height(rgba);
    gsize stride = (gsize)gdk_pixbuf_get_rowstride(rgba);
    const guchar *pixels = gdk_pixbuf_get_pixels(rgba);

    GBytes *bytes = g_bytes_new(pixels, stride * (gsize)h);
    GdkTexture *texture = gdk_memory_texture_new(w, h, GDK_MEMORY_R8G8B8A8, bytes, stride);

    g_bytes_unref(bytes);
    g_object_unref(rgba);

    g_task_return_pointer(task, texture, g_object_unref);
}

static void on_thumbnail_ready(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GtkWidget *visual_slot = GTK_WIDGET(source_object);
    GError *err = NULL;
    GdkTexture *texture = g_task_propagate_pointer(G_TASK(res), &err);

    if (texture != NULL) {
        if (gtk_widget_get_root(visual_slot) != NULL) {
            int req_w = gtk_widget_get_width(visual_slot);
            int req_h = gtk_widget_get_height(visual_slot);
            if (req_w <= 0) req_w = 76;
            if (req_h <= 0) req_h = 56;

            GtkWidget *picture = gtk_picture_new_for_paintable(GDK_PAINTABLE(texture));
            gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_COVER);
            gtk_widget_set_size_request(picture, req_w, req_h);
            gtk_widget_add_css_class(picture, "image-preview-thumb");

            GtkWidget *first = gtk_widget_get_first_child(visual_slot);
            if (first) {
                gtk_box_remove(GTK_BOX(visual_slot), first);
            }
            gtk_box_append(GTK_BOX(visual_slot), picture);
        }
        g_object_unref(texture);
    } else if (err) {
        g_error_free(err);
    }

    g_object_unref(visual_slot);
}

void queue_async_thumbnail(AppState *state,
                           GtkWidget *visual_slot,
                           const char *full_path,
                           int w,
                           int h) {
    ThumbJobData *job = g_new0(ThumbJobData, 1);
    job->file_path = g_strdup(full_path);
    job->target_w = w;
    job->target_h = h;

    g_object_ref(visual_slot);

    GTask *task = g_task_new(visual_slot, state->load_cancellable, on_thumbnail_ready, NULL);
    g_task_set_task_data(task, job, free_thumb_job_data);
    g_task_run_in_thread(task, load_thumbnail_worker);
    g_object_unref(task);
}