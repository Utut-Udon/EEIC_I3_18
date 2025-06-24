#include <gtk/gtk.h>
#include "server_lib.h"
#include "client_lib.h"

typedef struct {
    GtkEntry  *entry_sport;
    GtkButton *btn_start, *btn_stop;
    GtkEntry  *entry_ip, *entry_cport;
    GtkButton *btn_connect;
    GtkTextView *log_view;
    GThread   *thr_client;
} App;

// グローバルに保持
static App W;

// TextView にログ追記
static void append_log(const char *msg) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(W.log_view);
    gtk_text_buffer_insert_at_cursor(buf, msg, -1);
    gtk_text_buffer_insert_at_cursor(buf, "\n", 1);
}

// サーバー起動
static void on_start_clicked(GtkButton *b, gpointer _) {
    const char *p = gtk_entry_get_text(W.entry_sport);
    append_log("Starting server...");
    start_server((uint16_t)atoi(p));
    gtk_widget_set_sensitive(GTK_WIDGET(W.btn_start), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(W.btn_stop),  TRUE);
    char log[64];
    sprintf(log, "Server listening on port %s", p);
    append_log(log);
}

// サーバー停止
static void on_stop_clicked(GtkButton *b, gpointer _) {
    append_log("Stopping server...");
    stop_server();
    append_log("Server stopped.");
    gtk_widget_set_sensitive(GTK_WIDGET(W.btn_start), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(W.btn_stop),  FALSE);
}

// クライアント接続・切断トグル
static void on_toggle_connect(GtkButton *b, gpointer _) {
    const char *lbl = gtk_button_get_label(b);
    if (g_strcmp0(lbl, "Connect") == 0) {
        const char *ip   = gtk_entry_get_text(W.entry_ip);
        const char *port = gtk_entry_get_text(W.entry_cport);
        char *arg = g_strdup_printf("%s:%s", ip, port);
        append_log("Client: connecting...");
        W.thr_client = g_thread_new("client", client_thread, arg);
        gtk_button_set_label(b, "Disconnect");
    } else {
        append_log("Client: disconnecting...");
        stop_client();
        append_log("Client: disconnected.");
        gtk_button_set_label(b, "Connect");
    }
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    GtkBuilder *builder = gtk_builder_new_from_file("app.ui");

    // UI 要素取得
    W.entry_sport  = GTK_ENTRY (gtk_builder_get_object(builder, "entry_port"));
    W.btn_start    = GTK_BUTTON(gtk_builder_get_object(builder, "btn_start"));
    W.btn_stop     = GTK_BUTTON(gtk_builder_get_object(builder, "btn_stop"));
    W.log_view     = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "log_view"));
    W.entry_ip     = GTK_ENTRY (gtk_builder_get_object(builder, "entry_ip"));
    W.entry_cport  = GTK_ENTRY (gtk_builder_get_object(builder, "entry_cport"));
    W.btn_connect  = GTK_BUTTON(gtk_builder_get_object(builder, "btn_connect"));

    // シグナル接続
    g_signal_connect(W.btn_start,   "clicked", G_CALLBACK(on_start_clicked),   NULL);
    g_signal_connect(W.btn_stop,    "clicked", G_CALLBACK(on_stop_clicked),    NULL);
    g_signal_connect(W.btn_connect, "clicked", G_CALLBACK(on_toggle_connect),   NULL);

    // 初期状態
    gtk_widget_set_sensitive(GTK_WIDGET(W.btn_stop), FALSE);

    GtkWidget *win = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
