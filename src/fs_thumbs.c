#include "app.h"

/* --- Zero-Intrinsic-Size GPU Cover-Fit Thumbnail Widget --- */
typedef struct
{
    GtkWidget parent_instance;
    GdkTexture *texture;
} ThumbCoverWidget;

typedef struct
{
    GtkWidgetClass parent_class;
} ThumbCoverWidgetClass;

G_DEFINE_TYPE(ThumbCoverWidget, thumb_cover_widget, GTK_TYPE_WIDGET)

static void thumb_cover_widget_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
    ThumbCoverWidget *self = (ThumbCoverWidget *)widget;
    if (!self->texture)
        return;

    int w = gtk_widget_get_width(widget);
    int h = gtk_widget_get_height(widget);
    int tex_w = gdk_texture_get_width(self->texture);
    int tex_h = gdk_texture_get_height(self->texture);
    if (w <= 0 || h <= 0 || tex_w <= 0 || tex_h <= 0)
        return;

    float scale = MAX((float)w / (float)tex_w, (float)h / (float)tex_h);
    float draw_w = (float)tex_w * scale;
    float draw_h = (float)tex_h * scale;
    float off_x = ((float)w - draw_w) * 0.5f;
    float off_y = ((float)h - draw_h) * 0.5f;

    GskRoundedRect clip;
    gsk_rounded_rect_init_from_rect(&clip,
                                    &GRAPHENE_RECT_INIT(0.0f, 0.0f, (float)w, (float)h),
                                    8.0f);
    gtk_snapshot_push_rounded_clip(snapshot, &clip);
    gtk_snapshot_append_texture(snapshot,
                                self->texture,
                                &GRAPHENE_RECT_INIT(off_x, off_y, draw_w, draw_h));
    gtk_snapshot_pop(snapshot);
}

static void thumb_cover_widget_dispose(GObject *object)
{
    ThumbCoverWidget *self = (ThumbCoverWidget *)object;
    g_clear_object(&self->texture);
    G_OBJECT_CLASS(thumb_cover_widget_parent_class)->dispose(object);
}

static void thumb_cover_widget_class_init(ThumbCoverWidgetClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = thumb_cover_widget_dispose;
    GTK_WIDGET_CLASS(klass)->snapshot = thumb_cover_widget_snapshot;
}

static void thumb_cover_widget_init(ThumbCoverWidget *self)
{
    self->texture = NULL;
}

static GtkWidget *thumb_cover_widget_new(GdkTexture *texture)
{
    ThumbCoverWidget *self = g_object_new(thumb_cover_widget_get_type(), NULL);
    self->texture = g_object_ref(texture);
    gtk_widget_set_hexpand(GTK_WIDGET(self), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(self), TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(self), "image-preview-thumb");
    return GTK_WIDGET(self);
}

/* --- Async Pixbuf -> GdkTexture Loader --- */
typedef struct
{
    char *file_path;
    int target_w;
    int target_h;
} ThumbJobData;

static void free_thumb_job_data(gpointer data)
{
    ThumbJobData *job = (ThumbJobData *)data;
    g_free(job->file_path);
    g_free(job);
}

static void load_thumbnail_worker(GTask *task,
                                  gpointer source_object,
                                  gpointer task_data,
                                  GCancellable *cancellable)
{
    (void)source_object;
    if (g_cancellable_is_cancelled(cancellable))
    {
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
        return;
    }

    ThumbJobData *job = (ThumbJobData *)task_data;
    GError *err = NULL;

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(
        job->file_path, job->target_w, job->target_h, TRUE, &err);

    if (!pixbuf)
    {
        g_task_return_error(task, err);
        return;
    }

    if (g_cancellable_is_cancelled(cancellable))
    {
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

static void on_thumbnail_ready(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    (void)user_data;
    GtkWidget *visual_slot = GTK_WIDGET(source_object);
    GError *err = NULL;
    GdkTexture *texture = g_task_propagate_pointer(G_TASK(res), &err);

    if (texture != NULL)
    {
        if (GTK_IS_BOX(visual_slot) && gtk_widget_get_root(visual_slot) != NULL)
        {
            GtkWidget *thumb = thumb_cover_widget_new(texture);
            GtkWidget *first = gtk_widget_get_first_child(visual_slot);
            if (first)
            {
                gtk_box_remove(GTK_BOX(visual_slot), first);
            }
            gtk_box_append(GTK_BOX(visual_slot), thumb);
        }
        g_object_unref(texture);
    }
    else if (err)
    {
        g_error_free(err);
    }

    g_object_unref(visual_slot);
}

void queue_async_thumbnail(AppState *state,
                           GtkWidget *visual_slot,
                           const char *full_path,
                           int w,
                           int h)
{
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