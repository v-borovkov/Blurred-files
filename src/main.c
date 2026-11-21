#include "app.h"
#include <string.h>

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

    gboolean desktop_mode = FALSE;
    for (int i = 1; i < argc; i++)
    {
        if (g_strcmp0(argv[i], "--desktop") == 0)
        {
            desktop_mode = TRUE;
            break;
        }
    }

    if (desktop_mode)
    {
        GtkApplication *app = gtk_application_new("com.github.blurredfiles.desktop", G_APPLICATION_DEFAULT_FLAGS);
        g_signal_connect(app, "activate", G_CALLBACK(activate_desktop_mode), NULL);
        int status = g_application_run(G_APPLICATION(app), 1, argv);
        g_object_unref(app);
        return status;
    }

    GtkApplication *app = gtk_application_new("com.github.blurredfiles", G_APPLICATION_HANDLES_OPEN);
    g_signal_connect(app, "activate", G_CALLBACK(activate_app), NULL);
    g_signal_connect(app, "open", G_CALLBACK(on_open), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}