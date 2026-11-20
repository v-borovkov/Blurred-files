#include "app.h"
#include <glib/gstdio.h>
#include <string.h>

/* --- Selection Helpers --- */
void clear_item_selection(AppState *state)
{
    if (!state->flowbox)
        return;

    for (GtkWidget *child = gtk_widget_get_first_child(state->flowbox);
         child != NULL;
         child = gtk_widget_get_next_sibling(child))
    {
        GtkWidget *btn = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(child));
        if (btn)
        {
            gtk_widget_remove_css_class(btn, "selected");
        }
    }
    update_status_bar(state);
}

void select_all_items(AppState *state)
{
    if (!state->flowbox)
        return;

    for (GtkWidget *child = gtk_widget_get_first_child(state->flowbox);
         child != NULL;
         child = gtk_widget_get_next_sibling(child))
    {
        GtkWidget *btn = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(child));
        if (btn)
        {
            gtk_widget_add_css_class(btn, "selected");
        }
    }
    update_status_bar(state);
}

GList *get_selected_paths(AppState *state)
{
    GList *paths = NULL;
    if (!state->flowbox)
        return NULL;

    for (GtkWidget *child = gtk_widget_get_first_child(state->flowbox);
         child != NULL;
         child = gtk_widget_get_next_sibling(child))
    {
        GtkWidget *btn = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(child));
        if (btn && gtk_widget_has_css_class(btn, "selected"))
        {
            const char *p = (const char *)g_object_get_data(G_OBJECT(btn), "full-path");
            if (p)
            {
                paths = g_list_prepend(paths, g_strdup(p));
            }
        }
    }
    return g_list_reverse(paths);
}

/* --- Recursive Copy & Delete Helpers --- */
static gboolean copy_file_or_dir_recursive(GFile *src, GFile *dst, GError **error)
{
    GFileType type = g_file_query_file_type(src, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL);

    if (type == G_FILE_TYPE_DIRECTORY)
    {
        if (!g_file_make_directory_with_parents(dst, NULL, error))
        {
            if (*error && (*error)->code == G_IO_ERROR_EXISTS)
            {
                g_clear_error(error);
            }
            else
            {
                return FALSE;
            }
        }

        GFileEnumerator *enumerator = g_file_enumerate_children(
            src, "standard::name", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, error);
        if (!enumerator)
            return FALSE;

        GFileInfo *info;
        while ((info = g_file_enumerator_next_file(enumerator, NULL, error)) != NULL)
        {
            const char *child_name = g_file_info_get_name(info);
            GFile *child_src = g_file_get_child(src, child_name);
            GFile *child_dst = g_file_get_child(dst, child_name);

            gboolean ok = copy_file_or_dir_recursive(child_src, child_dst, error);
            g_object_unref(child_src);
            g_object_unref(child_dst);
            g_object_unref(info);

            if (!ok)
            {
                g_object_unref(enumerator);
                return FALSE;
            }
        }
        g_object_unref(enumerator);
        return TRUE;
    }
    else
    {
        return g_file_copy(src, dst, G_FILE_COPY_OVERWRITE | G_FILE_COPY_NOFOLLOW_SYMLINKS, NULL, NULL, NULL, error);
    }
}

static gboolean delete_file_or_dir_recursive(GFile *target, GError **error)
{
    GFileType type = g_file_query_file_type(target, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL);

    if (type == G_FILE_TYPE_DIRECTORY)
    {
        GFileEnumerator *enumerator = g_file_enumerate_children(
            target, "standard::name", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, error);
        if (!enumerator)
            return FALSE;

        GFileInfo *info;
        while ((info = g_file_enumerator_next_file(enumerator, NULL, error)) != NULL)
        {
            const char *child_name = g_file_info_get_name(info);
            GFile *child = g_file_get_child(target, child_name);
            gboolean ok = delete_file_or_dir_recursive(child, error);
            g_object_unref(child);
            g_object_unref(info);
            if (!ok)
            {
                g_object_unref(enumerator);
                return FALSE;
            }
        }
        g_object_unref(enumerator);
    }

    return g_file_delete(target, NULL, error);
}

/* --- Undo / Redo Stack Recording & Execution (Ctrl+Z / Ctrl+Y) --- */
static void free_undo_action(UndoAction *act)
{
    if (!act)
        return;
    g_list_free_full(act->src_paths, g_free);
    g_list_free_full(act->dst_paths, g_free);
    g_free(act);
}

static GList *duplicate_string_list(GList *list)
{
    GList *copy = NULL;
    for (GList *l = list; l != NULL; l = l->next)
    {
        copy = g_list_prepend(copy, g_strdup((const char *)l->data));
    }
    return g_list_reverse(copy);
}

void record_undo_action(AppState *state, UndoType type, GList *src_paths, GList *dst_paths)
{
    g_list_free_full(state->redo_stack, (GDestroyNotify)free_undo_action);
    state->redo_stack = NULL;

    UndoAction *act = g_new0(UndoAction, 1);
    act->type = type;
    act->src_paths = src_paths;
    act->dst_paths = dst_paths;
    state->undo_stack = g_list_prepend(state->undo_stack, act);
}

static char *find_latest_trashed_path_for_original(const char *orig_path)
{
    char *info_dir = g_build_filename(g_get_user_data_dir(), "Trash", "info", NULL);
    char *files_dir = g_build_filename(g_get_user_data_dir(), "Trash", "files", NULL);
    GDir *d = g_dir_open(info_dir, 0, NULL);
    if (!d)
    {
        g_free(info_dir);
        g_free(files_dir);
        return NULL;
    }

    char *matched_trashed_file = NULL;
    const char *name;
    while ((name = g_dir_read_name(d)) != NULL)
    {
        if (!g_str_has_suffix(name, ".trashinfo"))
            continue;

        char *info_path = g_build_filename(info_dir, name, NULL);
        GKeyFile *kf = g_key_file_new();
        if (g_key_file_load_from_file(kf, info_path, G_KEY_FILE_NONE, NULL))
        {
            char *raw = g_key_file_get_string(kf, "Trash Info", "Path", NULL);
            if (raw)
            {
                char *unescaped = g_uri_unescape_string(raw, NULL);
                char *resolved = (unescaped && *unescaped == '/')
                                     ? g_strdup(unescaped)
                                     : g_build_filename(g_get_home_dir(), unescaped ? unescaped : raw, NULL);

                if (g_strcmp0(resolved, orig_path) == 0)
                {
                    char *base_name = g_strndup(name, strlen(name) - strlen(".trashinfo"));
                    g_free(matched_trashed_file);
                    matched_trashed_file = g_build_filename(files_dir, base_name, NULL);
                    g_free(base_name);
                }
                g_free(resolved);
                g_free(unescaped);
                g_free(raw);
            }
        }
        g_key_file_free(kf);
        g_free(info_path);
    }

    g_dir_close(d);
    g_free(info_dir);
    g_free(files_dir);
    return matched_trashed_file;
}

void perform_undo(AppState *state)
{
    if (!state->undo_stack)
        return;

    UndoAction *act = (UndoAction *)state->undo_stack->data;
    state->undo_stack = g_list_delete_link(state->undo_stack, state->undo_stack);

    gboolean any_changed = FALSE;

    if (act->type == UNDO_DELETE_COPIES)
    {
        for (GList *l = act->dst_paths; l != NULL; l = l->next)
        {
            const char *copied_path = (const char *)l->data;
            if (g_file_test(copied_path, G_FILE_TEST_EXISTS))
            {
                GFile *f = g_file_new_for_path(copied_path);
                if (delete_file_or_dir_recursive(f, NULL))
                {
                    any_changed = TRUE;
                }
                g_object_unref(f);
            }
        }
    }
    else if (act->type == UNDO_MOVE_BACK)
    {
        GList *s = act->src_paths;
        GList *d = act->dst_paths;
        while (s != NULL && d != NULL)
        {
            const char *orig_path = (const char *)s->data;
            const char *curr_path = (const char *)d->data;
            if (g_file_test(curr_path, G_FILE_TEST_EXISTS) && !g_file_test(orig_path, G_FILE_TEST_EXISTS))
            {
                GFile *f_curr = g_file_new_for_path(curr_path);
                GFile *f_orig = g_file_new_for_path(orig_path);
                if (g_file_move(f_curr, f_orig, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL))
                {
                    any_changed = TRUE;
                }
                g_object_unref(f_curr);
                g_object_unref(f_orig);
            }
            s = s->next;
            d = d->next;
        }
    }
    else if (act->type == UNDO_RESTORE_TRASH)
    {
        for (GList *l = act->src_paths; l != NULL; l = l->next)
        {
            const char *orig_path = (const char *)l->data;
            char *trashed_path = find_latest_trashed_path_for_original(orig_path);
            if (trashed_path && g_file_test(trashed_path, G_FILE_TEST_EXISTS))
            {
                char *parent_dir = g_path_get_dirname(orig_path);
                g_mkdir_with_parents(parent_dir, 0755);
                g_free(parent_dir);

                GFile *f_trash = g_file_new_for_path(trashed_path);
                GFile *f_orig = g_file_new_for_path(orig_path);
                if (g_file_move(f_trash, f_orig, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL))
                {
                    char *base = g_path_get_basename(trashed_path);
                    char *info_name = g_strdup_printf("%s.trashinfo", base);
                    char *info_path = g_build_filename(g_get_user_data_dir(), "Trash", "info", info_name, NULL);
                    g_unlink(info_path);
                    g_free(info_path);
                    g_free(info_name);
                    g_free(base);
                    any_changed = TRUE;
                }

                g_object_unref(f_trash);
                g_object_unref(f_orig);
            }
            g_free(trashed_path);
        }
    }
    if (any_changed)
    {
        state->redo_stack = g_list_prepend(state->redo_stack, act);
        if (state->current_path)
        {
            populate_directory(state, state->current_path);
        }
    }
    else
    {
        free_undo_action(act);
    }
}
void perform_redo(AppState *state)
{
    if (!state->redo_stack)
        return;
    UndoAction *act = (UndoAction *)state->redo_stack->data;
    state->redo_stack = g_list_delete_link(state->redo_stack, state->redo_stack);
    gboolean any_changed = FALSE;
    if (act->type == UNDO_DELETE_COPIES)
    {
        GList *s = act->src_paths;
        GList *d = act->dst_paths;
        while (s != NULL && d != NULL)
        {
            const char *src_path = (const char *)s->data;
            const char *dst_path = (const char *)d->data;
            if (g_file_test(src_path, G_FILE_TEST_EXISTS))
            {
                GFile *f_src = g_file_new_for_path(src_path);
                GFile *f_dst = g_file_new_for_path(dst_path);
                if (copy_file_or_dir_recursive(f_src, f_dst, NULL))
                {
                    any_changed = TRUE;
                }
                g_object_unref(f_src);
                g_object_unref(f_dst);
            }
            s = s->next;
            d = d->next;
        }
    }
    else if (act->type == UNDO_MOVE_BACK)
    {
        GList *s = act->src_paths;
        GList *d = act->dst_paths;
        while (s != NULL && d != NULL)
        {
            const char *orig_path = (const char *)s->data;
            const char *dst_path = (const char *)d->data;
            if (g_file_test(orig_path, G_FILE_TEST_EXISTS) && !g_file_test(dst_path, G_FILE_TEST_EXISTS))
            {
                GFile *f_orig = g_file_new_for_path(orig_path);
                GFile *f_dst = g_file_new_for_path(dst_path);
                if (g_file_move(f_orig, f_dst, G_FILE_COPY_NONE, NULL, NULL, NULL, NULL))
                {
                    any_changed = TRUE;
                }
                g_object_unref(f_orig);
                g_object_unref(f_dst);
            }
            s = s->next;
            d = d->next;
        }
    }
    else if (act->type == UNDO_RESTORE_TRASH)
    {
        for (GList *l = act->src_paths; l != NULL; l = l->next)
        {
            const char *orig_path = (const char *)l->data;
            if (g_file_test(orig_path, G_FILE_TEST_EXISTS))
            {
                GFile *f_orig = g_file_new_for_path(orig_path);
                if (g_file_trash(f_orig, NULL, NULL))
                {
                    any_changed = TRUE;
                }
                g_object_unref(f_orig);
            }
        }
    }
    (void)duplicate_string_list;
    if (any_changed)
    {
        state->undo_stack = g_list_prepend(state->undo_stack, act);
        if (state->current_path)
        {
            populate_directory(state, state->current_path);
        }
    }
    else
    {
        free_undo_action(act);
    }
}
/* --- Copy / Cut / Paste --- */
void bulk_copy_selected_items(AppState *state, gboolean is_cut)
{
    GList *paths = get_selected_paths(state);
    if (!paths)
        return;
    g_list_free_full(state->clipboard_paths, g_free);
    state->clipboard_paths = paths;
    state->clipboard_is_cut = is_cut;
    GString *joined = g_string_new(NULL);
    for (GList *l = paths; l != NULL; l = l->next)
    {
        if (joined->len > 0)
            g_string_append_c(joined, '\n');
        g_string_append(joined, (const char *)l->data);
    }
    GdkClipboard *clipboard = gdk_display_get_clipboard(gdk_display_get_default());
    gdk_clipboard_set_text(clipboard, joined->str);
    g_string_free(joined, TRUE);
}
static char *build_unique_destination_path(const char *dest_dir, const char *basename)
{
    char *candidate = g_build_filename(dest_dir, basename, NULL);
    if (!g_file_test(candidate, G_FILE_TEST_EXISTS))
    {
        return candidate;
    }
    g_free(candidate);
    const char *dot = strrchr(basename, '.');
    gboolean has_ext = (dot != NULL && dot != basename);
    char *stem = has_ext ? g_strndup(basename, (gsize)(dot - basename)) : g_strdup(basename);
    const char *ext = has_ext ? dot : "";
    for (int i = 1; i < 1000; i++)
    {
        char *alt_name = g_strdup_printf("%s (copy %d)%s", stem, i, ext);
        candidate = g_build_filename(dest_dir, alt_name, NULL);
        g_free(alt_name);
        if (!g_file_test(candidate, G_FILE_TEST_EXISTS))
        {
            g_free(stem);
            return candidate;
        }
        g_free(candidate);
    }
    g_free(stem);
    return g_build_filename(dest_dir, basename, NULL);
}
void bulk_paste_clipboard_items(AppState *state)
{
    if (!state->clipboard_paths || !state->current_path)
        return;
    gboolean any_changed = FALSE;
    gboolean was_cut = state->clipboard_is_cut;
    GList *recorded_srcs = NULL;
    GList *recorded_dsts = NULL;
    for (GList *l = state->clipboard_paths; l != NULL; l = l->next)
    {
        const char *src_path = (const char *)l->data;
        if (!g_file_test(src_path, G_FILE_TEST_EXISTS))
            continue;
        char *basename = g_path_get_basename(src_path);
        char *dst_path = build_unique_destination_path(state->current_path, basename);
        GFile *src = g_file_new_for_path(src_path);
        GFile *dst = g_file_new_for_path(dst_path);
        GError *err = NULL;
        if (was_cut)
        {
            if (g_file_move(src, dst, G_FILE_COPY_NONE, NULL, NULL, NULL, &err))
            {
                any_changed = TRUE;
                recorded_srcs = g_list_prepend(recorded_srcs, g_strdup(src_path));
                recorded_dsts = g_list_prepend(recorded_dsts, g_strdup(dst_path));
            }
            else if (err)
            {
                g_warning("Move failed: %s", err->message);
                g_error_free(err);
            }
        }
        else
        {
            if (copy_file_or_dir_recursive(src, dst, &err))
            {
                any_changed = TRUE;
                recorded_srcs = g_list_prepend(recorded_srcs, g_strdup(src_path));
                recorded_dsts = g_list_prepend(recorded_dsts, g_strdup(dst_path));
            }
            else if (err)
            {
                g_warning("Copy failed: %s", err->message);
                g_error_free(err);
            }
        }
        g_object_unref(src);
        g_object_unref(dst);
        g_free(dst_path);
        g_free(basename);
    }
    if (recorded_dsts != NULL)
    {
        record_undo_action(state,
                           was_cut ? UNDO_MOVE_BACK : UNDO_DELETE_COPIES,
                           g_list_reverse(recorded_srcs),
                           g_list_reverse(recorded_dsts));
    }
    if (was_cut)
    {
        g_list_free_full(state->clipboard_paths, g_free);
        state->clipboard_paths = NULL;
        state->clipboard_is_cut = FALSE;
    }
    if (any_changed)
    {
        populate_directory(state, state->current_path);
    }
}
/* --- Drag-and-Drop Destination Handler --- */
gboolean move_dropped_files_to_directory(AppState *state,
                                         const GValue *value,
                                         const char *dest_dir,
                                         gboolean to_trash)
{
    GList *files_to_move = NULL;
    if (G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST))
    {
        GSList *list = g_value_get_boxed(value);
        for (GSList *l = list; l != NULL; l = l->next)
        {
            files_to_move = g_list_prepend(files_to_move, g_object_ref(G_FILE(l->data)));
        }
        files_to_move = g_list_reverse(files_to_move);
    }
    else if (G_VALUE_HOLDS(value, G_TYPE_FILE))
    {
        files_to_move = g_list_prepend(files_to_move, g_object_ref(G_FILE(g_value_get_object(value))));
    }
    else
    {
        return FALSE;
    }
    gboolean any_moved = FALSE;
    GList *recorded_srcs = NULL;
    GList *recorded_dsts = NULL;
    for (GList *l = files_to_move; l != NULL; l = l->next)
    {
        GFile *src_file = G_FILE(l->data);
        char *src_path = g_file_get_path(src_file);
        if (!src_path)
            continue;
        GError *err = NULL;
        if (to_trash)
        {
            if (g_file_trash(src_file, NULL, &err))
            {
                any_moved = TRUE;
                recorded_srcs = g_list_prepend(recorded_srcs, g_strdup(src_path));
            }
            else if (err)
            {
                g_warning("Drop to trash failed: %s", err->message);
                g_error_free(err);
            }
        }
        else if (dest_dir != NULL)
        {
            char *basename = g_file_get_basename(src_file);
            char *dst_path = g_build_filename(dest_dir, basename, NULL);
            if (g_strcmp0(src_path, dst_path) != 0)
            {
                GFile *dst_file = g_file_new_for_path(dst_path);
                if (g_file_move(src_file, dst_file, G_FILE_COPY_NONE, NULL, NULL, NULL, &err))
                {
                    any_moved = TRUE;
                    recorded_srcs = g_list_prepend(recorded_srcs, g_strdup(src_path));
                    recorded_dsts = g_list_prepend(recorded_dsts, g_strdup(dst_path));
                }
                else if (err)
                {
                    g_warning("Drop move failed: %s", err->message);
                    g_error_free(err);
                }
                g_object_unref(dst_file);
            }
            g_free(dst_path);
            g_free(basename);
        }
        g_free(src_path);
    }
    if (recorded_srcs != NULL)
    {
        if (to_trash)
        {
            record_undo_action(state, UNDO_RESTORE_TRASH, g_list_reverse(recorded_srcs), NULL);
        }
        else
        {
            record_undo_action(state, UNDO_MOVE_BACK, g_list_reverse(recorded_srcs), g_list_reverse(recorded_dsts));
        }
    }
    g_list_free_full(files_to_move, g_object_unref);
    if (any_moved && state->current_path)
    {
        populate_directory(state, state->current_path);
    }
    return any_moved;
}
/* --- Trash, Restore & Empty Trash --- */
void bulk_trash_selected_items(AppState *state)
{
    GList *paths = get_selected_paths(state);
    if (!paths)
        return;
    gboolean in_trash = is_current_dir_trash(state);
    gboolean any_changed = FALSE;
    GList *trashed_srcs = NULL;
    for (GList *l = paths; l != NULL; l = l->next)
    {
        const char *item_path = (const char *)l->data;
        GFile *file = g_file_new_for_path(item_path);
        GError *err = NULL;
        if (in_trash)
        {
            if (delete_file_or_dir_recursive(file, &err))
            {
                any_changed = TRUE;
                char *base = g_path_get_basename(item_path);
                char *info_name = g_strdup_printf("%s.trashinfo", base);
                char *info_path = g_build_filename(g_get_user_data_dir(), "Trash", "info", info_name, NULL);
                g_unlink(info_path);
                g_free(info_path);
                g_free(info_name);
                g_free(base);
            }
            else if (err)
            {
                g_warning("Permanent delete failed: %s", err->message);
                g_error_free(err);
            }
        }
        else
        {
            if (g_file_trash(file, NULL, &err))
            {
                any_changed = TRUE;
                trashed_srcs = g_list_prepend(trashed_srcs, g_strdup(item_path));
            }
            else if (err)
            {
                g_warning("Move to trash failed: %s", err->message);
                g_error_free(err);
            }
        }
        g_object_unref(file);
    }
    if (trashed_srcs != NULL)
    {
        record_undo_action(state, UNDO_RESTORE_TRASH, g_list_reverse(trashed_srcs), NULL);
    }
    g_list_free_full(paths, g_free);
    if (any_changed && state->current_path)
    {
        populate_directory(state, state->current_path);
    }
}
void bulk_restore_selected_trash_items(AppState *state)
{
    if (!is_current_dir_trash(state))
        return;
    GList *paths = get_selected_paths(state);
    if (!paths)
        return;
    gboolean any_restored = FALSE;
    for (GList *l = paths; l != NULL; l = l->next)
    {
        const char *trashed_path = (const char *)l->data;
        char *base = g_path_get_basename(trashed_path);
        char *info_name = g_strdup_printf("%s.trashinfo", base);
        char *info_path = g_build_filename(g_get_user_data_dir(), "Trash", "info", info_name, NULL);
        GKeyFile *kf = g_key_file_new();
        char *dest_path = NULL;
        if (g_key_file_load_from_file(kf, info_path, G_KEY_FILE_NONE, NULL))
        {
            char *raw_path = g_key_file_get_string(kf, "Trash Info", "Path", NULL);
            if (raw_path)
            {
                char *unescaped = g_uri_unescape_string(raw_path, NULL);
                if (unescaped && *unescaped == '/')
                {
                    dest_path = unescaped;
                }
                else
                {
                    g_free(unescaped);
                    dest_path = g_build_filename(g_get_home_dir(), raw_path, NULL);
                }
                g_free(raw_path);
            }
        }
        g_key_file_free(kf);
        if (!dest_path)
        {
            dest_path = g_build_filename(g_get_home_dir(), base, NULL);
        }
        char *parent_dir = g_path_get_dirname(dest_path);
        g_mkdir_with_parents(parent_dir, 0755);
        g_free(parent_dir);
        GFile *src = g_file_new_for_path(trashed_path);
        GFile *dst = g_file_new_for_path(dest_path);
        GError *err = NULL;
        if (g_file_move(src, dst, G_FILE_COPY_NONE, NULL, NULL, NULL, &err))
        {
            g_unlink(info_path);
            any_restored = TRUE;
        }
        else if (err)
        {
            g_warning("Restore failed: %s", err->message);
            g_error_free(err);
        }
        g_object_unref(src);
        g_object_unref(dst);
        g_free(dest_path);
        g_free(info_path);
        g_free(info_name);
        g_free(base);
    }
    g_list_free_full(paths, g_free);
    if (any_restored && state->current_path)
    {
        populate_directory(state, state->current_path);
    }
}
static void clear_directory_contents(const char *dir_path)
{
    GDir *d = g_dir_open(dir_path, 0, NULL);
    if (!d)
        return;
    const char *name;
    while ((name = g_dir_read_name(d)) != NULL)
    {
        char *full = g_build_filename(dir_path, name, NULL);
        GFile *f = g_file_new_for_path(full);
        delete_file_or_dir_recursive(f, NULL);
        g_object_unref(f);
        g_free(full);
    }
    g_dir_close(d);
}
static void empty_trash_thread_func(GTask *task,
                                    gpointer source_object,
                                    gpointer task_data,
                                    GCancellable *cancellable)
{
    (void)source_object;
    (void)task_data;
    (void)cancellable;
    char *trash_files = g_build_filename(g_get_user_data_dir(), "Trash", "files", NULL);
    char *trash_info = g_build_filename(g_get_user_data_dir(), "Trash", "info", NULL);
    clear_directory_contents(trash_files);
    clear_directory_contents(trash_info);
    g_free(trash_files);
    g_free(trash_info);
    g_task_return_boolean(task, TRUE);
}
static void on_empty_trash_finished(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    (void)res;
    GtkWidget *window = GTK_WIDGET(source_object);
    AppState *state = (AppState *)user_data;
    if (GTK_IS_WINDOW(window) && gtk_widget_get_root(window) != NULL &&
        state && state->current_path && GTK_IS_WIDGET(state->flowbox))
    {
        populate_directory(state, state->current_path);
    }
}
void empty_trash_completely(AppState *state)
{
    if (!state || !state->window)
        return;
    GTask *task = g_task_new(state->window, NULL, on_empty_trash_finished, state);
    g_task_run_in_thread(task, empty_trash_thread_func);
    g_object_unref(task);
}
/* --- Archive Compression & Extraction (.zip / .tar.gz) --- */
static void on_archive_subprocess_done(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    GtkWidget *window = GTK_WIDGET(user_data);
    GSubprocess *proc = G_SUBPROCESS(source_object);
    GError *err = NULL;
    if (!g_subprocess_wait_check_finish(proc, res, &err))
    {
        if (err)
        {
            g_warning("Archive operation failed: %s", err->message);
            g_error_free(err);
        }
    }
    if (GTK_IS_WINDOW(window) && gtk_widget_get_root(window) != NULL)
    {
        AppState *state = (AppState *)g_object_get_data(G_OBJECT(window), "app-state");
        if (state && state->current_path && GTK_IS_WIDGET(state->flowbox))
        {
            populate_directory(state, state->current_path);
        }
    }
    g_object_unref(window);
}
void bulk_compress_selected_items(AppState *state, gboolean use_tar_gz)
{
    if (!state || !state->current_path || !state->window)
        return;
    GList *paths = get_selected_paths(state);
    if (!paths)
        return;
    char *first_base = g_path_get_basename((const char *)paths->data);
    char *archive_name = g_strdup_printf("%s%s",
                                         g_list_length(paths) > 1 ? "Archive" : first_base,
                                         use_tar_gz ? ".tar.gz" : ".zip");
    char *unique_archive = build_unique_destination_path(state->current_path, archive_name);
    char *final_archive_base = g_path_get_basename(unique_archive);
    GPtrArray *argv = g_ptr_array_new_with_free_func(g_free);
    if (use_tar_gz)
    {
        g_ptr_array_add(argv, g_strdup("tar"));
        g_ptr_array_add(argv, g_strdup("-czf"));
        g_ptr_array_add(argv, g_strdup_printf("./%s", final_archive_base));
        for (GList *l = paths; l != NULL; l = l->next)
        {
            char *item_base = g_path_get_basename((const char *)l->data);
            g_ptr_array_add(argv, g_strdup_printf("./%s", item_base));
            g_free(item_base);
        }
    }
    else
    {
        g_ptr_array_add(argv, g_strdup("zip"));
        g_ptr_array_add(argv, g_strdup("-r"));
        g_ptr_array_add(argv, g_strdup_printf("./%s", final_archive_base));
        for (GList *l = paths; l != NULL; l = l->next)
        {
            char *item_base = g_path_get_basename((const char *)l->data);
            g_ptr_array_add(argv, g_strdup_printf("./%s", item_base));
            g_free(item_base);
        }
    }
    g_ptr_array_add(argv, NULL);
    GSubprocessLauncher *launcher = g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_NONE);
    g_subprocess_launcher_set_cwd(launcher, state->current_path);
    GError *err = NULL;
    GSubprocess *proc = g_subprocess_launcher_spawnv(launcher, (const char *const *)argv->pdata, &err);
    if (proc)
    {
        g_subprocess_wait_check_async(proc, NULL, on_archive_subprocess_done, g_object_ref(state->window));
        g_object_unref(proc);
    }
    else if (err)
    {
        g_warning("Failed to spawn archiver: %s", err->message);
        g_error_free(err);
    }
    g_object_unref(launcher);
    g_ptr_array_free(argv, TRUE);
    g_free(final_archive_base);
    g_free(unique_archive);
    g_free(archive_name);
    g_free(first_base);
    g_list_free_full(paths, g_free);
}
void bulk_extract_selected_archives(AppState *state)
{
    if (!state || !state->current_path || !state->window)
        return;
    GList *paths = get_selected_paths(state);
    if (!paths)
        return;
    for (GList *l = paths; l != NULL; l = l->next)
    {
        const char *arch_path = (const char *)l->data;
        char *base = g_path_get_basename(arch_path);
        GPtrArray *argv = g_ptr_array_new_with_free_func(g_free);
        if (g_str_has_suffix(base, ".zip"))
        {
            g_ptr_array_add(argv, g_strdup("unzip"));
            g_ptr_array_add(argv, g_strdup("-o"));
            g_ptr_array_add(argv, g_strdup_printf("./%s", base));
        }
        else if (g_str_has_suffix(base, ".tar.gz") ||
                 g_str_has_suffix(base, ".tgz") ||
                 g_str_has_suffix(base, ".tar.xz") ||
                 g_str_has_suffix(base, ".tar.bz2") ||
                 g_str_has_suffix(base, ".tar"))
        {
            g_ptr_array_add(argv, g_strdup("tar"));
            g_ptr_array_add(argv, g_strdup("-xf"));
            g_ptr_array_add(argv, g_strdup_printf("./%s", base));
        }
        else
        {
            g_ptr_array_free(argv, TRUE);
            g_free(base);
            continue;
        }
        g_ptr_array_add(argv, NULL);
        GSubprocessLauncher *launcher = g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_NONE);
        g_subprocess_launcher_set_cwd(launcher, state->current_path);
        GError *err = NULL;
        GSubprocess *proc = g_subprocess_launcher_spawnv(launcher, (const char *const *)argv->pdata, &err);
        if (proc)
        {
            g_subprocess_wait_check_async(proc, NULL, on_archive_subprocess_done, g_object_ref(state->window));
            g_object_unref(proc);
        }
        else if (err)
        {
            g_warning("Failed to extract archive: %s", err->message);
            g_error_free(err);
        }
        g_object_unref(launcher);
        g_ptr_array_free(argv, TRUE);
        g_free(base);
    }
    g_list_free_full(paths, g_free);
}
/* --- Open Directory in Terminal --- */
void open_directory_in_terminal(const char *dir_path)
{
    if (!dir_path || !g_file_test(dir_path, G_FILE_TEST_IS_DIR))
        return;
    const char *env_term = g_getenv("TERMINAL");
    const char *candidates[] = {
        env_term,
        "foot",
        "kitty",
        "alacritty",
        "wezterm",
        "ghostty",
        "gnome-terminal",
        "konsole",
        "xfce4-terminal",
        "xterm",
        NULL};
    for (int i = 0; i < (int)G_N_ELEMENTS(candidates) - 1; i++)
    {
        const char *term = candidates[i];
        if (!term || *term == '\0')
            continue;
        char *found = g_find_program_in_path(term);
        if (found != NULL)
        {
            const char *argv[] = {found, NULL};
            GError *err = NULL;
            if (g_spawn_async(dir_path, (char **)argv, NULL,
                              G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err))
            {
                g_free(found);
                return;
            }
            if (err)
            {
                g_warning("Terminal launch failed (%s): %s", found, err->message);
                g_error_free(err);
            }
            g_free(found);
        }
    }
    g_warning("No supported terminal emulator found in PATH.");
}