// app.c
#include <gtk/gtk.h>
#include "relay_lib.h"
#include "client_lib.h"

typedef struct {
    GtkEntry    *entry_port;
    GtkButton   *btn_start, *btn_stop;
    GtkTextView *log_view;
    GtkEntry    *entry_ip, *entry_cport;
    GtkButton   *btn_connect;
    GThread     *thr_relay, *thr_client;
} App;

// ログを TextView に追記
static void append_log(App *a, const char *msg) {
    GtkTextBuffer *buf = gtk_text_view_get_buffer(a->log_view);
    gtk_text_buffer_insert_at_cursor(buf, msg, -1);
    gtk_text_buffer_insert_at_cursor(buf, "\n", 1);
}

// Start Relay ボタン
static void on_start_clicked(GtkButton *b, App *a) {
    const char *p = gtk_entry_get_text(a->entry_port);
    uint16_t port = (uint16_t)atoi(p);
    append_log(a, "Starting relay...");
    uint16_t *arg = malloc(sizeof(uint16_t));
    *arg = port;
    a->thr_relay = g_thread_new("relay", relay_thread, arg);
    gtk_widget_set_sensitive(GTK_WIDGET(a->btn_start), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(a->btn_stop), TRUE);
}

// Stop Relay ボタン
static void on_stop_clicked(GtkButton *b, App *a) {
    append_log(a, "Stopping relay...");
    stop_relay();
    g_thread_join(a->thr_relay);
    append_log(a, "Relay stopped.");
    gtk_widget_set_sensitive(GTK_WIDGET(a->btn_start), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(a->btn_stop), FALSE);
}

// Connect/Disconnect トグル
static void on_toggle_connect(GtkButton *b, App *a) {
    const char *lbl = gtk_button_get_label(b);
    if (g_strcmp0(lbl, "Connect") == 0) {
        // --- Connect 処理 ---
        const char *ip_text = gtk_entry_get_text(a->entry_ip);
        const char *port_text = gtk_entry_get_text(a->entry_cport);
        char *arg = g_strdup_printf("%s:%s", ip_text, port_text);
        append_log(a, "Client: connecting...");
        a->thr_client = g_thread_new("client", client_thread, arg);
        gtk_button_set_label(b, "Disconnect");
    } else {
        // --- Disconnect 処理 ---
        append_log(a, "Client: disconnecting...");
        stop_client();
        g_thread_join(a->thr_client);
        append_log(a, "Client: disconnected.");
        gtk_button_set_label(b, "Connect");
    }
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    GtkBuilder *builder = gtk_builder_new_from_file("app.ui");
    App a = {0};

    // UI 要素取得
    a.entry_port  = GTK_ENTRY (gtk_builder_get_object(builder, "entry_port"));
    a.btn_start   = GTK_BUTTON(gtk_builder_get_object(builder, "btn_start"));
    a.btn_stop    = GTK_BUTTON(gtk_builder_get_object(builder, "btn_stop"));
    a.log_view    = GTK_TEXT_VIEW(gtk_builder_get_object(builder, "log_view"));
    a.entry_ip    = GTK_ENTRY (gtk_builder_get_object(builder, "entry_ip"));
    a.entry_cport = GTK_ENTRY (gtk_builder_get_object(builder, "entry_cport"));
    a.btn_connect = GTK_BUTTON(gtk_builder_get_object(builder, "btn_connect"));

    // シグナル接続
    g_signal_connect(a.btn_start,   "clicked", G_CALLBACK(on_start_clicked),    &a);
    g_signal_connect(a.btn_stop,    "clicked", G_CALLBACK(on_stop_clicked),     &a);
    g_signal_connect(a.btn_connect, "clicked", G_CALLBACK(on_toggle_connect),   &a);

    GtkWidget *win = GTK_WIDGET(gtk_builder_get_object(builder, "main_window"));
    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
