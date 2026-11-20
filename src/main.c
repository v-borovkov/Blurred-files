#include "app.h"

static void on_open(GtkApplication *app,
                    GFile **files,
                    gint n_files,
                    const gchar *hint,
                    gpointer user_data)
{
    (void)hint;
    (void)user_data;

    char *target_dir = NULL;
    if (n_files > 0 && files != NULL)
    {
        char *path = g_file_get_path(files[0]);
        if (path != NULL)
        {
            if (g_file_test(path, G_FILE_TEST_IS_DIR))
            {
                target_dir = path;
            }
            else
            {
                target_dir = g_path_get_dirname(path);
                g_free(path);
            }
        }
    }

    activate_app_with_path(app, target_dir);
    g_free(target_dir);
}

int main(int argc, char **argv)
{
    g_setenv("GSK_RENDERER", "gl", FALSE);

    GtkApplication *app = gtk_application_new("com.github.blurredfiles", G_APPLICATION_HANDLES_OPEN);
    g_signal_connect(app, "activate", G_CALLBACK(activate_app), NULL);
    g_signal_connect(app, "open", G_CALLBACK(on_open), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}