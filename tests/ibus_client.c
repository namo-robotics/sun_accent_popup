/** Exercises native typing and accent replacement through independent IBus client contexts. */
#include <ibus.h>
#include <string.h>

/** Models text and selection owned by an application, independently of its input method. */
typedef struct {
    IBusInputContext *context;
    GString *text;
    guint cursor;
    guint anchor;
    guint commits;
    guint deletes;
    guint preedits;
    guint candidates;
    gboolean menu;
    gboolean supports_surrounding;
} Client;

/** Services actual D-Bus signals while the separate engine handles timers and keys. */
static void pump(guint milliseconds) {
    gint64 end = g_get_monotonic_time() + milliseconds * 1000;
    do {
        while (g_main_context_iteration(NULL, FALSE)) {}
        g_usleep(1000);
    } while (g_get_monotonic_time() < end);
}
/** Reports application-owned text and selection to engines that can safely replace a letter. */
static void report(Client *client) {
    if (client->supports_surrounding)
        ibus_input_context_set_surrounding_text(client->context,
            ibus_text_new_from_string(client->text->str), client->cursor, client->anchor);
}
/** Replaces a character range using UTF-8 offsets and moves the application caret. */
static void replace(Client *client, guint start, guint end, const char *text) {
    g_assert_cmpuint(end, <=, g_utf8_strlen(client->text->str, -1));
    gsize left = g_utf8_offset_to_pointer(client->text->str, start) - client->text->str;
    gsize right = g_utf8_offset_to_pointer(client->text->str, end) - client->text->str;
    g_string_erase(client->text, left, right - left);
    g_string_insert(client->text, left, text);
    client->cursor = start + g_utf8_strlen(text, -1);
    client->anchor = client->cursor;
}
/** Applies only the explicit accent commit; ordinary letters must come from native typing. */
static void commit(IBusInputContext *context, IBusText *text, gpointer data) {
    (void)context;
    Client *client = data;
    client->commits++;
    replace(client, MIN(client->cursor, client->anchor), MAX(client->cursor, client->anchor), text->text);
    report(client);
}
/** Applies the engine's precise one-character replacement request. */
static void delete_text(IBusInputContext *context, gint offset, guint count, gpointer data) {
    (void)context;
    Client *client = data;
    g_assert_cmpint(offset, ==, -1);
    g_assert_cmpuint(count, ==, 1);
    g_assert_cmpuint(client->cursor, >, 0);
    client->deletes++;
    replace(client, client->cursor - 1, client->cursor, "");
    report(client);
}
/** Rejects visible or nonempty composition; IBus may send empty initialization notifications. */
static void preedit(IBusInputContext *context, IBusText *text, guint cursor, gboolean visible, gpointer data) {
    (void)context; (void)cursor;
    if (visible || *text->text) ((Client *)data)->preedits++;
}
/** Records candidate presentation without treating display labels as committed text. */
static void lookup(IBusInputContext *context, IBusLookupTable *table, gboolean visible, gpointer data) {
    (void)context;
    Client *client = data;
    client->menu = visible;
    client->candidates = ibus_lookup_table_get_number_of_candidates(table);
    if (visible) {
        g_assert_cmpuint(client->candidates, >, 0);
        g_assert_true(g_str_has_prefix(ibus_lookup_table_get_candidate(table, 0)->text, "1 "));
        g_assert_cmpstr(ibus_lookup_table_get_label(table, 0)->text, ==, " ");
    }
}
/** Records menu dismissal in its originating application context. */
static void hide(IBusInputContext *context, gpointer data) {
    (void)context;
    ((Client *)data)->menu = FALSE;
}
/** Supplies surrounding text when the engine first enables that capability. */
static void require_text(IBusInputContext *context, gpointer data) {
    (void)context;
    report(data);
}
/** Sends a key through IBus, then performs ordinary editing only when it is not consumed. */
static gboolean key(Client *client, guint value, guint code, guint modifiers) {
    gboolean consumed = ibus_input_context_process_key_event(client->context, value, code, modifiers);
    pump(20);
    if (!consumed && !(modifiers & (IBUS_RELEASE_MASK | IBUS_CONTROL_MASK | IBUS_MOD1_MASK | IBUS_SUPER_MASK))) {
        if (value >= 32 && value < 127) {
            gchar text[2] = {(gchar)value, 0};
            replace(client, MIN(client->cursor, client->anchor), MAX(client->cursor, client->anchor), text);
        } else if (value == IBUS_KEY_BackSpace && client->cursor > 0) {
            replace(client, client->cursor - 1, client->cursor, "");
        } else if (value == IBUS_KEY_Left && client->cursor > 0) {
            client->cursor--;
            client->anchor = client->cursor;
        }
        report(client);
        pump(20);
    }
    return consumed;
}
/** Creates a focused application and verifies that the test engine is active. */
static void create_client(Client *client, IBusBus *bus, gboolean supports_surrounding) {
    client->supports_surrounding = supports_surrounding;
    client->text = g_string_new("");
    client->context = ibus_bus_create_input_context(bus, "sun-accent-native-typing-test");
    g_assert_nonnull(client->context);
    g_signal_connect(client->context, "commit-text", G_CALLBACK(commit), client);
    g_signal_connect(client->context, "delete-surrounding-text", G_CALLBACK(delete_text), client);
    g_signal_connect(client->context, "update-preedit-text", G_CALLBACK(preedit), client);
    g_signal_connect(client->context, "update-lookup-table", G_CALLBACK(lookup), client);
    g_signal_connect(client->context, "hide-lookup-table", G_CALLBACK(hide), client);
    g_signal_connect(client->context, "require-surrounding-text", G_CALLBACK(require_text), client);
    ibus_input_context_set_capabilities(client->context, IBUS_CAP_PREEDIT_TEXT | IBUS_CAP_LOOKUP_TABLE |
        IBUS_CAP_FOCUS | (supports_surrounding ? IBUS_CAP_SURROUNDING_TEXT : 0));
    ibus_input_context_focus_in(client->context);
    ibus_input_context_set_engine(client->context, "sun-accent-popup");
    for (guint i = 0; i < 100; i++) {
        pump(20);
        IBusEngineDesc *engine = ibus_input_context_get_engine(client->context);
        if (engine && !strcmp(ibus_engine_desc_get_name(engine), "sun-accent-popup")) {
            report(client);
            pump(50);
            return;
        }
    }
    g_error("Accent engine did not become active");
}
/** Clears the client document between independent input scenarios. */
static void clear(Client *client) {
    ibus_input_context_reset(client->context);
    pump(20);
    g_string_truncate(client->text, 0);
    client->cursor = client->anchor = 0;
    client->commits = client->deletes = client->preedits = 0;
    report(client);
    pump(20);
}
/** Releases the input context and its application-owned document. */
static void destroy_client(Client *client) {
    g_assert_cmpuint(client->preedits, ==, 0);
    ibus_input_context_focus_out(client->context);
    ibus_proxy_destroy(IBUS_PROXY(client->context));
    g_object_unref(client->context);
    g_string_free(client->text, TRUE);
}
/** Verifies native taps, explicit replacement, cancellation, context changes, and unsupported clients. */
int main(void) {
    ibus_init();
    IBusBus *bus = ibus_bus_new();
    g_assert_true(ibus_bus_is_connected(bus));
    g_assert_true(ibus_bus_set_global_engine(bus, "sun-accent-popup"));
    Client first = {0};
    create_client(&first, bus, TRUE);
    g_assert_false(key(&first, IBUS_KEY_a, 30, 0));
    g_assert_cmpstr(first.text->str, ==, "a");
    g_assert_false(key(&first, IBUS_KEY_a, 30, IBUS_RELEASE_MASK));
    g_assert_false(key(&first, IBUS_KEY_b, 48, 0));
    g_assert_cmpstr(first.text->str, ==, "ab");
    g_assert_cmpuint(first.commits, ==, 0);
    g_assert_cmpuint(first.deletes, ==, 0);
    g_assert_cmpuint(first.preedits, ==, 0);
    clear(&first);

    g_assert_false(key(&first, IBUS_KEY_e, 18, 0));
    g_assert_true(key(&first, IBUS_KEY_e, 18, 0));
    pump(550);
    g_assert_true(first.menu);
    g_assert_cmpuint(first.candidates, ==, 7);
    g_assert_cmpstr(first.text->str, ==, "e");
    key(&first, IBUS_KEY_e, 18, IBUS_RELEASE_MASK);
    key(&first, IBUS_KEY_2, 3, 0);
    g_assert_cmpstr(first.text->str, ==, "é");
    g_assert_cmpuint(first.deletes, ==, 1);
    g_assert_cmpuint(first.commits, ==, 1);
    g_assert_false(first.menu);
    clear(&first);

    key(&first, IBUS_KEY_N, 49, IBUS_SHIFT_MASK);
    pump(550);
    key(&first, IBUS_KEY_Return, 28, 0);
    key(&first, IBUS_KEY_N, 49, 0);
    key(&first, IBUS_KEY_n, 49, IBUS_RELEASE_MASK);
    g_assert_cmpstr(first.text->str, ==, "Ñ");
    clear(&first);
    key(&first, IBUS_KEY_a, 30, 0);
    pump(550);
    key(&first, IBUS_KEY_Escape, 1, 0);
    g_assert_cmpstr(first.text->str, ==, "a");
    g_assert_cmpuint(first.commits, ==, 0);
    key(&first, IBUS_KEY_BackSpace, 14, 0);
    g_assert_cmpstr(first.text->str, ==, "");
    clear(&first);

    // Moving the caret or editing with the mouse invalidates replacement.
    key(&first, IBUS_KEY_e, 18, 0);
    pump(550);
    first.cursor = first.anchor = 0;
    report(&first);
    pump(30);
    g_assert_false(first.menu);
    g_assert_false(key(&first, IBUS_KEY_2, 3, 0));
    g_assert_cmpstr(first.text->str, ==, "2e");
    g_assert_cmpuint(first.deletes, ==, 0);
    clear(&first);

    // Native insertion replaces a selection; accents replace only the inserted base.
    g_string_assign(first.text, "αβ tail");
    first.cursor = 0; first.anchor = 2;
    report(&first); pump(30);
    key(&first, IBUS_KEY_u, 22, 0);
    pump(550);
    key(&first, IBUS_KEY_4, 5, 0);
    key(&first, IBUS_KEY_u, 22, IBUS_RELEASE_MASK);
    g_assert_cmpstr(first.text->str, ==, "ü tail");
    clear(&first);
    g_assert_false(key(&first, IBUS_KEY_a, 30, IBUS_CONTROL_MASK));
    key(&first, IBUS_KEY_e, 18, 0);
    ibus_input_context_focus_out(first.context);
    pump(600);
    g_assert_cmpstr(first.text->str, ==, "e");
    g_assert_cmpuint(first.commits, ==, 0);

    Client second = {0};
    create_client(&second, bus, TRUE);
    ibus_input_context_set_content_type(second.context, IBUS_INPUT_PURPOSE_FREE_FORM, IBUS_INPUT_HINT_PRIVATE);
    pump(30); report(&second); pump(30);
    key(&second, IBUS_KEY_e, 18, 0);
    pump(550);
    key(&second, IBUS_KEY_2, 3, 0);
    key(&second, IBUS_KEY_e, 18, IBUS_RELEASE_MASK);
    g_assert_cmpstr(second.text->str, ==, "é");
    clear(&second);
    ibus_input_context_set_content_type(second.context, IBUS_INPUT_PURPOSE_PASSWORD, 0);
    pump(30);
    g_assert_false(key(&second, IBUS_KEY_a, 30, 0));
    g_assert_false(key(&second, IBUS_KEY_a, 30, 0));
    pump(550);
    g_assert_false(second.menu);
    g_assert_cmpuint(second.commits, ==, 0);
    destroy_client(&second);
    Client unsupported = {0};
    create_client(&unsupported, bus, FALSE);
    g_assert_false(key(&unsupported, IBUS_KEY_e, 18, 0));
    pump(550);
    g_assert_false(key(&unsupported, IBUS_KEY_e, 18, 0));
    g_assert_cmpstr(unsupported.text->str, ==, "ee");
    g_assert_false(unsupported.menu);
    g_assert_cmpuint(unsupported.deletes, ==, 0);
    destroy_client(&unsupported);
    destroy_client(&first);
    g_object_unref(bus);
    g_print("IBus native typing and accent replacement acceptance passed\n");
    return 0;
}
