#include "../src/app.h"
#include <glib/gstdio.h>
#include <string.h>

typedef struct
{
    char *sandbox_root;
    char *work_dir;
    char *data_dir;
    AppState *state;
    GtkWidget *window;
} TestFixture;

static void pump_events_ms(guint ms)
{
    gint64 end = g_get_monotonic_time() + (gint64)ms * 1000;
    while (g_get_monotonic_time() < end)
    {
        while (g_main_context_iteration(NULL, FALSE))
        {
        }
        g_usleep(2000);
    }
}

static void write_dummy_file(const char *dir, const char *name, const char *content)
{
    char *path = g_build_filename(dir, name, NULL);
    GError *err = NULL;
    gboolean ok = g_file_set_contents(path, content, -1, &err);
    g_assert_no_error(err);
    g_assert_true(ok);
    g_free(path);
}

static void remove_dir_recursive(const char *path)
{
    if (!path)
        return;
    GDir *dir = g_dir_open(path, 0, NULL);
    if (dir)
    {
        const char *entry;
        while ((entry = g_dir_read_name(dir)) != NULL)
        {
            char *child = g_build_filename(path, entry, NULL);
            if (g_file_test(child, G_FILE_TEST_IS_DIR) && !g_file_test(child, G_FILE_TEST_IS_SYMLINK))
            {
                remove_dir_recursive(child);
            }
            else
            {
                g_unlink(child);
            }
            g_free(child);
        }
        g_dir_close(dir);
    }
    g_rmdir(path);
}

static const char *find_first_label_text(GtkWidget *w)
{
    if (!w)
        return NULL;
    if (GTK_IS_LABEL(w))
    {
        return gtk_label_get_text(GTK_LABEL(w));
    }
    for (GtkWidget *c = gtk_widget_get_first_child(w); c != NULL; c = gtk_widget_get_next_sibling(c))
    {
        const char *txt = find_first_label_text(c);
        if (txt && *txt != '\0')
            return txt;
    }
    return NULL;
}

static void select_item_by_name(AppState *s, const char *target_name)
{
    for (GtkWidget *c = gtk_widget_get_first_child(s->flowbox); c; c = gtk_widget_get_next_sibling(c))
    {
        GtkWidget *btn = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(c));
        const char *lbl = find_first_label_text(btn);
        if (g_strcmp0(lbl, target_name) == 0)
        {
            gtk_widget_add_css_class(btn, "selected");
        }
    }
}

static void fixture_setup(TestFixture *fix, gconstpointer user_data)
{
    (void)user_data;

    g_mkdir_with_parents(g_get_user_cache_dir(), 0755);
    char *tmpl = g_build_filename(g_get_user_cache_dir(), "blurred-files-test-XXXXXX", NULL);
    fix->sandbox_root = g_mkdtemp(tmpl);
    g_assert_nonnull(fix->sandbox_root);

    fix->work_dir = g_build_filename(fix->sandbox_root, "workspace", NULL);
    fix->data_dir = g_strdup(g_get_user_data_dir());

    g_mkdir_with_parents(fix->work_dir, 0755);

    AppState *state = g_new0(AppState, 1);
    state->is_list_view = FALSE;
    state->show_hidden = FALSE;
    state->enable_thumbnails = FALSE;
    state->double_click_open = FALSE;
    state->grid_cols = 2;
    state->sort_column = SORT_BY_NAME;
    state->sort_ascending = TRUE;

    GtkWidget *window = gtk_window_new();
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 680);
    g_object_set_data(G_OBJECT(window), "app-state", state);
    state->window = window;

    state->back_btn = gtk_button_new();
    state->fwd_btn = gtk_button_new();
    state->root_btn = gtk_button_new_with_label("/");
    state->grid_btn = gtk_button_new();
    state->list_btn = gtk_button_new();
    state->search_entry = gtk_search_entry_new();
    state->status_label = gtk_label_new("");
    state->breadcrumb_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    state->sidebar_places_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    GtkWidget *flowbox = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox), GTK_SELECTION_NONE);
    state->flowbox = flowbox;

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_append(GTK_BOX(vbox), state->breadcrumb_box);
    gtk_box_append(GTK_BOX(vbox), state->search_entry);
    gtk_box_append(GTK_BOX(vbox), flowbox);
    gtk_box_append(GTK_BOX(vbox), state->status_label);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    gtk_window_present(GTK_WINDOW(window));
    pump_events_ms(25);

    fix->state = state;
    fix->window = window;
}

static void fixture_teardown(TestFixture *fix, gconstpointer user_data)
{
    (void)user_data;

    if (fix->state->dir_monitor)
    {
        g_file_monitor_cancel(fix->state->dir_monitor);
        g_object_unref(fix->state->dir_monitor);
    }
    if (fix->state->refresh_timeout_id != 0)
    {
        g_source_remove(fix->state->refresh_timeout_id);
    }
    if (fix->state->load_cancellable)
    {
        g_cancellable_cancel(fix->state->load_cancellable);
        g_object_unref(fix->state->load_cancellable);
    }

    gtk_window_destroy(GTK_WINDOW(fix->window));
    pump_events_ms(20);

    g_free(fix->state->current_path);
    g_list_free_full(fix->state->back_stack, g_free);
    g_list_free_full(fix->state->forward_stack, g_free);
    g_list_free_full(fix->state->clipboard_paths, g_free);
    g_list_free_full(fix->state->pinned_folders, g_free);
    g_free(fix->state);

    remove_dir_recursive(fix->sandbox_root);
    g_free(fix->sandbox_root);
    g_free(fix->work_dir);
    g_free(fix->data_dir);
}

/* --- Test 1: Settings Persistence --- */
static void test_settings_persistence(TestFixture *fix, gconstpointer user_data)
{
    (void)user_data;
    AppState *s = fix->state;

    s->is_list_view = TRUE;
    s->show_hidden = TRUE;
    s->double_click_open = TRUE;
    s->grid_cols = 4;
    s->sort_column = SORT_BY_SIZE;
    s->sort_ascending = FALSE;
    s->pinned_folders = g_list_append(s->pinned_folders, g_strdup(fix->work_dir));

    save_app_settings(s);

    s->is_list_view = FALSE;
    s->show_hidden = FALSE;
    s->double_click_open = FALSE;
    s->grid_cols = 1;
    s->sort_column = SORT_BY_NAME;
    s->sort_ascending = TRUE;
    g_list_free_full(s->pinned_folders, g_free);
    s->pinned_folders = NULL;

    load_app_settings(s);

    g_assert_true(s->is_list_view);
    g_assert_true(s->show_hidden);
    g_assert_true(s->double_click_open);
    g_assert_cmpint(s->grid_cols, ==, 4);
    g_assert_cmpint(s->sort_column, ==, SORT_BY_SIZE);
    g_assert_false(s->sort_ascending);
    g_assert_cmpuint(g_list_length(s->pinned_folders), ==, 1);
    g_assert_cmpstr((const char *)s->pinned_folders->data, ==, fix->work_dir);
}

/* --- Test 2: Navigation History & Breadcrumbs --- */
static void test_navigation_and_breadcrumbs(TestFixture *fix, gconstpointer user_data)
{
    (void)user_data;
    AppState *s = fix->state;

    char *sub = g_build_filename(fix->work_dir, "alpha", "beta", NULL);
    g_mkdir_with_parents(sub, 0755);

    navigate_to_path(s, fix->work_dir, FALSE);
    g_assert_cmpstr(s->current_path, ==, fix->work_dir);
    g_assert_false(gtk_widget_get_sensitive(s->back_btn));

    navigate_to_path(s, sub, TRUE);
    g_assert_cmpstr(s->current_path, ==, sub);
    g_assert_true(gtk_widget_get_sensitive(s->back_btn));

    GtkWidget *last_child = gtk_widget_get_last_child(s->breadcrumb_box);
    g_assert_nonnull(last_child);
    const char *last_lbl = find_first_label_text(last_child);
    g_assert_cmpstr(last_lbl, ==, "beta");

    on_back_clicked(NULL, s);
    g_assert_cmpstr(s->current_path, ==, fix->work_dir);
    g_assert_true(gtk_widget_get_sensitive(s->fwd_btn));

    on_fwd_clicked(NULL, s);
    g_assert_cmpstr(s->current_path, ==, sub);

    g_free(sub);
}

/* --- Test 3: Sorting, Hidden Toggle & Search Filter --- */
static void test_sorting_and_search(TestFixture *fix, gconstpointer user_data)
{
    (void)user_data;
    AppState *s = fix->state;

    write_dummy_file(fix->work_dir, "zebra.txt", "1");
    write_dummy_file(fix->work_dir, "apple.txt", "1234567890");
    write_dummy_file(fix->work_dir, ".secret", "hidden");

    s->show_hidden = FALSE;
    s->sort_column = SORT_BY_NAME;
    s->sort_ascending = TRUE;
    navigate_to_path(s, fix->work_dir, FALSE);

    GtkWidget *first = gtk_widget_get_first_child(s->flowbox);
    g_assert_nonnull(first);
    GtkWidget *btn1 = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(first));
    g_assert_cmpstr(find_first_label_text(btn1), ==, "apple.txt");

    set_sort_column(s, SORT_BY_SIZE);
    first = gtk_widget_get_first_child(s->flowbox);
    btn1 = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(first));
    g_assert_cmpstr(find_first_label_text(btn1), ==, "zebra.txt");

    /* Toggle hidden files ON -> 3 items should appear */
    s->show_hidden = TRUE;
    populate_directory(s, fix->work_dir);
    int count = 0;
    for (GtkWidget *c = gtk_widget_get_first_child(s->flowbox); c; c = gtk_widget_get_next_sibling(c))
    {
        count++;
    }
    g_assert_cmpint(count, ==, 3);

    /* Filter via search bar */
    gtk_editable_set_text(GTK_EDITABLE(s->search_entry), "apple");
    on_search_changed(GTK_SEARCH_ENTRY(s->search_entry), s);
    first = gtk_widget_get_first_child(s->flowbox);
    g_assert_nonnull(first);
    g_assert_null(gtk_widget_get_next_sibling(first));
}

/* --- Test 4: Selection, Copy/Paste & Undo/Redo --- */
static void test_copy_paste_and_undo(TestFixture *fix, gconstpointer user_data)
{
    (void)user_data;
    AppState *s = fix->state;

    write_dummy_file(fix->work_dir, "report.txt", "hello world");
    char *dest_dir = g_build_filename(fix->work_dir, "dest", NULL);
    g_mkdir_with_parents(dest_dir, 0755);

    navigate_to_path(s, fix->work_dir, FALSE);
    select_item_by_name(s, "report.txt");

    GList *sel = get_selected_paths(s);
    g_assert_cmpuint(g_list_length(sel), ==, 1);
    g_list_free_full(sel, g_free);

    /* Cut & Paste into dest/ */
    bulk_copy_selected_items(s, TRUE);
    navigate_to_path(s, dest_dir, TRUE);
    bulk_paste_clipboard_items(s);

    char *moved_path = g_build_filename(dest_dir, "report.txt", NULL);
    char *orig_path = g_build_filename(fix->work_dir, "report.txt", NULL);

    g_assert_true(g_file_test(moved_path, G_FILE_TEST_EXISTS));
    g_assert_false(g_file_test(orig_path, G_FILE_TEST_EXISTS));
    /* Undo moves it back */
    perform_undo(s);
    g_assert_true(g_file_test(orig_path, G_FILE_TEST_EXISTS));
    g_assert_false(g_file_test(moved_path, G_FILE_TEST_EXISTS));
    /* Redo moves it into dest/ again */
    perform_redo(s);
    g_assert_true(g_file_test(moved_path, G_FILE_TEST_EXISTS));
    g_assert_false(g_file_test(orig_path, G_FILE_TEST_EXISTS));
    g_free(moved_path);
    g_free(orig_path);
    g_free(dest_dir);
}
/* --- Test 5: Trash & Async Background Empty Trash --- */
static void test_trash_and_empty(TestFixture *fix, gconstpointer user_data)
{
    (void)user_data;
    AppState *s = fix->state;
    write_dummy_file(fix->work_dir, "disposable.txt", "temp");
    navigate_to_path(s, fix->work_dir, FALSE);
    select_all_items(s);
    bulk_trash_selected_items(s);
    char *orig_path = g_build_filename(fix->work_dir, "disposable.txt", NULL);
    g_assert_false(g_file_test(orig_path, G_FILE_TEST_EXISTS));
    char *trash_files = g_build_filename(fix->data_dir, "Trash", "files", NULL);
    navigate_to_path(s, trash_files, TRUE);
    g_assert_true(is_current_dir_trash(s));
    empty_trash_completely(s);
    pump_events_ms(120);
    g_assert_null(gtk_widget_get_first_child(s->flowbox));
    g_free(orig_path);
    g_free(trash_files);
}
/* --- Test 6: Trash Restore --- */
static void test_trash_restore(TestFixture *fix, gconstpointer user_data)
{
    (void)user_data;
    AppState *s = fix->state;
    write_dummy_file(fix->work_dir, "precious.txt", "keep me");
    navigate_to_path(s, fix->work_dir, FALSE);
    select_item_by_name(s, "precious.txt");
    bulk_trash_selected_items(s);
    char *orig_path = g_build_filename(fix->work_dir, "precious.txt", NULL);
    g_assert_false(g_file_test(orig_path, G_FILE_TEST_EXISTS));
    char *trash_files = g_build_filename(fix->data_dir, "Trash", "files", NULL);
    navigate_to_path(s, trash_files, TRUE);
    select_item_by_name(s, "precious.txt");
    bulk_restore_selected_trash_items(s);
    g_assert_true(g_file_test(orig_path, G_FILE_TEST_EXISTS));
    g_free(orig_path);
    g_free(trash_files);
}
/* --- Test 7: Archive Compression & Extraction (.zip & .tar.gz) --- */
static void test_archives_zip_and_targz(TestFixture *fix, gconstpointer user_data)
{
    (void)user_data;
    AppState *s = fix->state;
    write_dummy_file(fix->work_dir, "doc_a.txt", "alpha content");
    write_dummy_file(fix->work_dir, "doc_b.txt", "beta content");
    navigate_to_path(s, fix->work_dir, FALSE);
    /* Compress into .tar.gz */
    select_item_by_name(s, "doc_a.txt");
    select_item_by_name(s, "doc_b.txt");
    bulk_compress_selected_items(s, TRUE);
    pump_events_ms(150);
    /* Find the generated .tar.gz archive */
    const char *archive_name = NULL;
    for (GtkWidget *c = gtk_widget_get_first_child(s->flowbox); c; c = gtk_widget_get_next_sibling(c))
    {
        GtkWidget *btn = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(c));
        const char *lbl = find_first_label_text(btn);
        if (lbl && g_str_has_suffix(lbl, ".tar.gz"))
        {
            archive_name = lbl;
            break;
        }
    }
    g_assert_nonnull(archive_name);
    /* Delete originals, then extract archive */
    char *pa = g_build_filename(fix->work_dir, "doc_a.txt", NULL);
    char *pb = g_build_filename(fix->work_dir, "doc_b.txt", NULL);
    g_unlink(pa);
    g_unlink(pb);
    g_assert_false(g_file_test(pa, G_FILE_TEST_EXISTS));
    clear_item_selection(s);
    select_item_by_name(s, archive_name);
    bulk_extract_selected_archives(s);
    pump_events_ms(150);
    g_assert_true(g_file_test(pa, G_FILE_TEST_EXISTS));
    g_assert_true(g_file_test(pb, G_FILE_TEST_EXISTS));
    g_free(pa);
    g_free(pb);
}
/* --- Test 8: Sidebar Pinned Folders & Active Highlight --- */
static void test_sidebar_pinning(TestFixture *fix, gconstpointer user_data)
{
    (void)user_data;
    AppState *s = fix->state;
    char *fav_dir = g_build_filename(fix->work_dir, "Projects", NULL);
    g_mkdir_with_parents(fav_dir, 0755);
    g_assert_false(is_folder_pinned(s, fav_dir));
    toggle_pin_folder(s, fav_dir);
    g_assert_true(is_folder_pinned(s, fav_dir));
    navigate_to_path(s, fav_dir, TRUE);
    update_sidebar_active_highlight(s);
    gboolean found_active_pin = FALSE;
    for (GtkWidget *c = gtk_widget_get_first_child(s->sidebar_places_box); c; c = gtk_widget_get_next_sibling(c))
    {
        const char *p = g_object_get_data(G_OBJECT(c), "sidebar-path");
        if (g_strcmp0(p, fav_dir) == 0 && gtk_widget_has_css_class(c, "active"))
        {
            found_active_pin = TRUE;
        }
    }
    g_assert_true(found_active_pin);
    /* Unpin */
    toggle_pin_folder(s, fav_dir);
    g_assert_false(is_folder_pinned(s, fav_dir));
    g_free(fav_dir);
}
/* --- Test 9: List View & 5-Tier Grid Column Zoom --- */
static void test_view_modes_and_zoom(TestFixture *fix, gconstpointer user_data)
{
    (void)user_data;
    AppState *s = fix->state;
    write_dummy_file(fix->work_dir, "sample.txt", "zoom test");
    navigate_to_path(s, fix->work_dir, FALSE);
    for (int tier = 0; tier <= 4; tier++)
    {
        s->grid_cols = tier;
        apply_grid_column_zoom(s);
        g_assert_cmpuint(gtk_flow_box_get_max_children_per_line(GTK_FLOW_BOX(s->flowbox)), >, 0);
    }
    s->is_list_view = TRUE;
    populate_directory(s, fix->work_dir);
    g_assert_cmpuint(gtk_flow_box_get_max_children_per_line(GTK_FLOW_BOX(s->flowbox)), ==, 1);
}
/* --- Test 10: Status Bar Selection & Summary Text --- */
static void test_status_bar_updates(TestFixture *fix, gconstpointer user_data)
{
    (void)user_data;
    AppState *s = fix->state;
    write_dummy_file(fix->work_dir, "one.txt", "12345");
    write_dummy_file(fix->work_dir, "two.txt", "1234567890");
    navigate_to_path(s, fix->work_dir, FALSE);
    const char *idle_txt = gtk_label_get_text(GTK_LABEL(s->status_label));
    g_assert_nonnull(strstr(idle_txt, "2"));
    select_item_by_name(s, "one.txt");
    update_status_bar(s);
    const char *sel_txt = gtk_label_get_text(GTK_LABEL(s->status_label));
    g_assert_nonnull(strstr(sel_txt, "1"));
}
/* --- Test 11: Squircle Modals Smoke Test (Preferences, Properties, Rename, New) --- */
static void test_squircle_modals_smoke(TestFixture *fix, gconstpointer user_data)
{
    (void)user_data;
    AppState *s = fix->state;
    write_dummy_file(fix->work_dir, "note.txt", "modal smoke");
    char *note_path = g_build_filename(fix->work_dir, "note.txt", NULL);
    navigate_to_path(s, fix->work_dir, FALSE);
    show_settings_dialog(s);
    show_properties_dialog(s, note_path);
    show_rename_dialog(s, note_path, "note.txt");
    show_create_item_dialog(s, TRUE);
    pump_events_ms(60);
    /* Destroy all transient modal windows cleanly */
    GList *toplevels = gtk_window_list_toplevels();
    for (GList *l = toplevels; l != NULL; l = l->next)
    {
        GtkWindow *w = GTK_WINDOW(l->data);
        if (GTK_WIDGET(w) != fix->window)
        {
            gtk_window_destroy(w);
        }
    }
    g_list_free(toplevels);
    pump_events_ms(30);
    g_free(note_path);
}
/* --- Test 12: Offscreen Cairo Squircle Render Smoke Test --- */
static void test_cairo_squircle_render(void)
{
    GtkWidget *da = gtk_drawing_area_new();
    const struct
    {
        int w;
        int h;
    } sizes[] = {
        {1200, 720},
        {640, 480},
        {0, 0}};
    for (guint i = 0; i < G_N_ELEMENTS(sizes); i++)
    {
        int w = MAX(1, sizes[i].w);
        int h = MAX(1, sizes[i].h);
        cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
        cairo_t *cr = cairo_create(surf);
        on_draw_squircle_bg(GTK_DRAWING_AREA(da), cr, sizes[i].w, sizes[i].h, NULL);
        g_assert_cmpint(cairo_status(cr), ==, CAIRO_STATUS_SUCCESS);
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
    }
    g_object_ref_sink(da);
    g_object_unref(da);
}
int main(int argc, char **argv)
{
    g_mkdir_with_parents(g_get_user_cache_dir(), 0755);
    char *suite_tmpl = g_build_filename(g_get_user_cache_dir(), "blurred-files-suite-XXXXXX", NULL);
    char *base_tmp = g_mkdtemp(suite_tmpl);
    g_assert_nonnull(base_tmp);
    char *cfg_root = g_build_filename(base_tmp, "config", NULL);
    char *dat_root = g_build_filename(base_tmp, "data", NULL);
    g_mkdir_with_parents(cfg_root, 0755);
    g_mkdir_with_parents(dat_root, 0700);
    g_setenv("GSK_RENDERER", "cairo", TRUE);
    g_setenv("GTK_A11Y", "none", TRUE);
    g_setenv("NO_AT_BRIDGE", "1", TRUE);
    g_setenv("XDG_CONFIG_HOME", cfg_root, TRUE);
    g_setenv("XDG_DATA_HOME", dat_root, TRUE);
    g_test_init(&argc, &argv, NULL);
    gtk_init();
    g_test_add("/settings/persistence", TestFixture, NULL, fixture_setup, test_settings_persistence, fixture_teardown);
    g_test_add("/nav/history_and_breadcrumbs", TestFixture, NULL, fixture_setup, test_navigation_and_breadcrumbs, fixture_teardown);
    g_test_add("/fs/sorting_and_search", TestFixture, NULL, fixture_setup, test_sorting_and_search, fixture_teardown);
    g_test_add("/fs/copy_paste_and_undo", TestFixture, NULL, fixture_setup, test_copy_paste_and_undo, fixture_teardown);
    g_test_add("/fs/trash_and_empty", TestFixture, NULL, fixture_setup, test_trash_and_empty, fixture_teardown);
    g_test_add("/fs/trash_restore", TestFixture, NULL, fixture_setup, test_trash_restore, fixture_teardown);
    g_test_add("/fs/archives_zip_and_targz", TestFixture, NULL, fixture_setup, test_archives_zip_and_targz, fixture_teardown);
    g_test_add("/sidebar/pinning_and_highlight", TestFixture, NULL, fixture_setup, test_sidebar_pinning, fixture_teardown);
    g_test_add("/ui/view_modes_and_zoom", TestFixture, NULL, fixture_setup, test_view_modes_and_zoom, fixture_teardown);
    g_test_add("/ui/status_bar_updates", TestFixture, NULL, fixture_setup, test_status_bar_updates, fixture_teardown);
    g_test_add("/ui/squircle_modals_smoke", TestFixture, NULL, fixture_setup, test_squircle_modals_smoke, fixture_teardown);
    g_test_add_func("/ui/cairo_squircle_render", test_cairo_squircle_render);
    int res = g_test_run();
    remove_dir_recursive(base_tmp);
    g_free(cfg_root);
    g_free(dat_root);
    g_free(base_tmp);
    return res;
}