#ifndef APP_H
#define APP_H

#include <gtk/gtk.h>
#include <gio/gio.h>

#ifndef DATADIR
#define DATADIR "/usr/local/share/blurred_files"
#endif

typedef enum
{
    SORT_BY_NAME = 0,
    SORT_BY_TYPE,
    SORT_BY_DATE,
    SORT_BY_SIZE
} SortColumn;

typedef enum
{
    UNDO_DELETE_COPIES, /* Undo a Paste (Copy) by deleting the newly created copies */
    UNDO_MOVE_BACK,     /* Undo a Move / Cut / Rename by moving items back to original paths */
    UNDO_RESTORE_TRASH  /* Undo Move to Trash by restoring specific trashed files */
} UndoType;

typedef struct
{
    UndoType type;
    GList *src_paths; /* Original paths */
    GList *dst_paths; /* Resulting paths */
} UndoAction;

typedef struct
{
    GtkWidget *window;
    GtkWidget *flowbox;
    GtkWidget *scrolled;
    GtkWidget *grid_overlay;
    GtkWidget *selection_box;
    GtkWidget *list_header_bar;
    GtkWidget *breadcrumb_box;
    GtkWidget *breadcrumb_scroll;
    GtkWidget *sidebar_box;
    GtkWidget *back_btn;
    GtkWidget *fwd_btn;
    GtkWidget *search_entry;
    GtkWidget *grid_btn;
    GtkWidget *list_btn;
    GtkWidget *status_label;
    GtkWidget *pinned_box;
    GtkWidget *drives_box;
    GtkWidget *root_btn;

    /* Column sort header buttons */
    GtkWidget *col_btn_name;
    GtkWidget *col_btn_type;
    GtkWidget *col_btn_date;
    GtkWidget *col_btn_size;

    /* Sidebar & Volume Monitor state */
    GtkWidget *sidebar_places_box;
    GList *pinned_folders;
    GVolumeMonitor *vol_monitor;

    /* Sidebar buttons */
    GtkWidget *btn_home;
    GtkWidget *btn_docs;
    GtkWidget *btn_downloads;
    GtkWidget *btn_pictures;
    GtkWidget *btn_music;
    GtkWidget *btn_trash;

    char *current_path;
    GList *back_stack;
    GList *forward_stack;

    /* Monitors & cancellables */
    GFileMonitor *dir_monitor;
    guint refresh_timeout_id;
    GVolumeMonitor *volume_monitor;
    GCancellable *load_cancellable;

    /* Clipboard file transfer buffer */
    GList *clipboard_paths;
    gboolean clipboard_is_cut;

    /* Pinned sidebar folders */
    GList *pinned_paths;

    /* Undo / Redo stacks */
    GList *undo_stack;
    GList *redo_stack;

    /* View, Sort & Settings state */
    gboolean is_desktop_mode;
    gboolean is_list_view;
    gboolean show_hidden;
    gboolean enable_thumbnails;
    gboolean double_click_open;
    int grid_cols;
    SortColumn sort_column;
    gboolean sort_ascending;

    /* Rubberband selection state */
    gboolean is_selecting;
    double sel_start_x;
    double sel_start_y;
} AppState;

/* squircle.c */
void on_draw_squircle_bg(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data);

/* fs_nav.c */
void navigate_to_path(AppState *state, const char *path, gboolean record_history);
void populate_directory(AppState *state, const char *path);
void on_back_clicked(GtkButton *btn, gpointer user_data);
void on_fwd_clicked(GtkButton *btn, gpointer user_data);
void on_search_changed(GtkSearchEntry *entry, gpointer user_data);
char *resolve_sidebar_directory(const char *dir_type);
void set_sort_column(AppState *state, SortColumn col);
gboolean is_current_dir_trash(AppState *state);
void update_status_bar(AppState *state);

/* fs_ops.c */
void clear_item_selection(AppState *state);
void select_all_items(AppState *state);
GList *get_selected_paths(AppState *state);
void bulk_copy_selected_items(AppState *state, gboolean is_cut);
void bulk_paste_clipboard_items(AppState *state);
void bulk_trash_selected_items(AppState *state);
void bulk_restore_selected_trash_items(AppState *state);
void empty_trash_completely(AppState *state);
gboolean move_dropped_files_to_directory(AppState *state, const GValue *value, const char *dest_dir, gboolean to_trash);
void bulk_compress_selected_items(AppState *state, gboolean use_tar_gz);
void bulk_extract_selected_archives(AppState *state);
void record_undo_action(AppState *state, UndoType type, GList *src_paths, GList *dst_paths);
void perform_undo(AppState *state);
void perform_redo(AppState *state);
void open_directory_in_terminal(const char *dir_path);

/* fs_thumbs.c */
void queue_async_thumbnail(AppState *state, GtkWidget *visual_slot, const char *full_path, int w, int h);

/* fs_item_widget.c */
GtkWidget *create_real_file_item(AppState *state, GFileInfo *info, const char *parent_path);
void apply_grid_column_zoom(AppState *state);

/* ui_dialogs.c */
void show_rename_dialog(AppState *state, const char *old_path, const char *current_name);
void show_create_item_dialog(AppState *state, gboolean is_folder);
void show_settings_dialog(AppState *state);
void show_properties_dialog(AppState *state, const char *file_path);
void load_app_settings(AppState *state);
void save_app_settings(AppState *state);
void show_open_with_dialog(AppState *state, const char *file_path);

/* ui_sidebar.c */
GtkWidget *build_sidebar_widget(AppState *state);
void rebuild_sidebar_places(AppState *state);
void update_sidebar_active_highlight(AppState *state);
gboolean is_folder_pinned(AppState *state, const char *path);
void toggle_pin_folder(AppState *state, const char *path);

/* ui_main.c */
GtkWidget *build_file_grid_overlay(AppState *state);
gboolean on_window_key_pressed(GtkEventControllerKey *controller,
                               guint keyval,
                               guint keycode,
                               GdkModifierType modifiers,
                               gpointer user_data);
void activate_app(GtkApplication *app, gpointer user_data);
void activate_app_with_path(GtkApplication *app, const char *initial_path);

/* ui_desktop.c */
void activate_desktop_mode(GtkApplication *app, gpointer user_data);

#endif /* APP_H */