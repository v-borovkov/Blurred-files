#include "app.h"

int main(int argc, char **argv) {
    g_setenv("GSK_RENDERER", "gl", FALSE);

    GtkApplication *app = gtk_application_new("com.github.blurredfiles", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate_app), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}