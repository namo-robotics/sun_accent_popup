/** Adapts IBus object lifetimes, timers, and text signals to the Sun input
 * state machine. */
#include "ibus_bridge.h"
#include <ibus.h>

/** Owns native composition scalars and one cancelable hold timer per input
 * context. */
typedef struct {
  IBusEngine parent;
  guint state[6];
  guint timer;
  gchar *surrounding;
  guint cursor;
  guint anchor;
  gchar *target;
  guint target_cursor;
  gboolean visible;
} SaEngine;
/** Installs IBus virtual methods for the Sun engine. */
typedef struct {
  IBusEngineClass parent;
} SaEngineClass;
/** Registers the native IBus subclass required by its factory. */
static GType sa_engine_get_type(void);
G_DEFINE_TYPE(SaEngine, sa_engine, IBUS_TYPE_ENGINE)
static SaDispatch dispatch_event;
static guint hold_delay;

/** Cancels a timer before resetting or destroying its input context. */
static void cancel_timer(SaEngine *engine) {
  if (engine->timer) {
    g_source_remove(engine->timer);
    engine->timer = 0;
  }
}
/** Verifies the exact text and caret expected after the application's original keypress. */
static gboolean target_matches(SaEngine *engine) {
  return engine->target && engine->surrounding &&
         engine->cursor == engine->target_cursor && engine->anchor == engine->cursor &&
         !g_strcmp0(engine->surrounding, engine->target);
}
/** Opens the picker only if the original letter is still at the unchanged caret. */
static gboolean expired(gpointer data) {
  SaEngine *engine = data;
  engine->timer = 0;
  dispatch_event(engine, target_matches(engine) ? 1 : 2, 0, 0, 0);
  return G_SOURCE_REMOVE;
}
/** Leaves unsupported clients untouched and delegates supported key decisions to Sun. */
static gboolean process_key(IBusEngine *native, guint key, guint code, guint state) {
  SaEngine *engine = (SaEngine *)native;
  if (!(native->client_capabilities & IBUS_CAP_SURROUNDING_TEXT) || !engine->surrounding) {
    dispatch_event(engine, 2, 0, 0, 0);
    return FALSE;
  }
  return dispatch_event(engine, 0, key, code, state) != 0;
}
/** Cancels a hold without committing, deleting, or composing any application text. */
static void reset(IBusEngine *engine) { dispatch_event(engine, 2, 0, 0, 0); }
/** Requests current text from the newly focused client before permitting a hold. */
static void focus_in(IBusEngine *native) {
  SaEngine *engine = (SaEngine *)native;
  reset(native);
  g_clear_pointer(&engine->surrounding, g_free);
  ibus_engine_get_surrounding_text(native, NULL, NULL, NULL);
}
/** Releases the ephemeral text snapshot when leaving a client. */
static void focus_out(IBusEngine *native) {
  reset(native);
  g_clear_pointer(&((SaEngine *)native)->surrounding, g_free);
}
/** Tracks client text and cancels the picker if an edit or selection invalidates its target. */
static void surrounding(IBusEngine *native, IBusText *text, guint cursor, guint anchor) {
  SaEngine *engine = (SaEngine *)native;
  IBUS_ENGINE_CLASS(sa_engine_parent_class)->set_surrounding_text(native, text, cursor, anchor);
  g_clear_pointer(&engine->surrounding, g_free);
  if (engine->state[5]) return;
  guint length = (guint)g_utf8_strlen(text->text, -1);
  if (cursor > length || anchor > length) { reset(native); return; }
  engine->surrounding = g_strdup(text->text);
  engine->cursor = cursor;
  engine->anchor = anchor;
  if (engine->target && !target_matches(engine)) reset(native);
}
/** Predicts only the normal insertion of the original ASCII letter, including selection replacement. */
static void expect_letter(SaEngine *engine) {
  g_clear_pointer(&engine->target, g_free);
  if (!engine->surrounding) return;
  guint start = MIN(engine->cursor, engine->anchor);
  guint end = MAX(engine->cursor, engine->anchor);
  gchar *prefix = g_strndup(engine->surrounding,
      g_utf8_offset_to_pointer(engine->surrounding, start) - engine->surrounding);
  gchar letter[2] = {(gchar)engine->state[0], 0};
  engine->target = g_strconcat(prefix, letter,
      g_utf8_offset_to_pointer(engine->surrounding, end), NULL);
  engine->target_cursor = start + 1;
  g_free(prefix);
}
/** Routes a primary-button candidate selection to the Sun state machine. */
static void candidate(IBusEngine *engine, guint index, guint button,
                      guint state) {
  (void)state;
  if (button == 1)
    dispatch_event(engine, 3, index, 0, 0);
}
/** Leaves password and PIN input untouched when clients identify those fields.
 */
static void content_type(IBusEngine *engine, guint purpose, guint hints) {
  // Private input forbids learning; this engine never stores typing history.
  (void)hints;
  gboolean sensitive = purpose == IBUS_INPUT_PURPOSE_PASSWORD ||
                       purpose == IBUS_INPUT_PURPOSE_PIN;
  dispatch_event(engine, 4, sensitive, 0, 0);
  if (sensitive) g_clear_pointer(&((SaEngine *)engine)->surrounding, g_free);
  else ibus_engine_get_surrounding_text(engine, NULL, NULL, NULL);
}
/** Cancels callbacks before the inherited IBus service is destroyed. */
static void destroy(IBusObject *object) {
  cancel_timer((SaEngine *)object);
  g_clear_pointer(&((SaEngine *)object)->target, g_free);
  g_clear_pointer(&((SaEngine *)object)->surrounding, g_free);
  IBUS_OBJECT_CLASS(sa_engine_parent_class)->destroy(object);
}
/** Installs native lifecycle and event adapters without application policy. */
static void sa_engine_class_init(SaEngineClass *klass) {
  IBusEngineClass *engine = IBUS_ENGINE_CLASS(klass);
  engine->process_key_event = process_key;
  engine->reset = reset;
  engine->focus_in = focus_in;
  engine->enable = focus_in;
  engine->focus_out = focus_out;
  engine->set_surrounding_text = surrounding;
  engine->disable = reset;
  engine->candidate_clicked = candidate;
  engine->set_content_type = content_type;
  IBUS_OBJECT_CLASS(klass)->destroy = destroy;
}
/** Initializes one input context with no pending timer or composition. */
static void sa_engine_init(SaEngine *engine) { (void)engine; }
uint32_t sa_get(void *engine, unsigned field) {
  return field < 6 ? ((SaEngine *)engine)->state[field] : 0;
}
void sa_set(void *engine, unsigned field, uint32_t value) {
  if (field < 6)
    ((SaEngine *)engine)->state[field] = value;
}
void sa_apply(void *pointer, const char *choices,
              unsigned selected, const char *commit, int arm) {
  SaEngine *engine = pointer;
  IBusEngine *native = IBUS_ENGINE(engine);
  if (!engine->state[0] || engine->state[3] || !engine->state[1])
    cancel_timer(engine);
  if (*commit && target_matches(engine)) {
    // The client already inserted the base letter; replace only that verified character.
    ibus_engine_delete_surrounding_text(native, -1, 1);
    ibus_engine_commit_text(native, ibus_text_new_from_string(commit));
  }
  if (!engine->state[0]) g_clear_pointer(&engine->target, g_free);
  if (*choices) {
    IBusLookupTable *table = ibus_lookup_table_new(9, 0, TRUE, TRUE);
    g_object_ref_sink(table);
    gchar **items = g_strsplit(choices, " ", -1);
    for (guint i = 0; items[i]; i++) {
      // Keep the shortcut and accent in the same theme-colored text label.
      // A nonempty blank index prevents GNOME from supplying a dim default number.
      gchar *display = g_strdup_printf("%u %s", i + 1, items[i]);
      ibus_lookup_table_append_candidate(table, ibus_text_new_from_string(display));
      ibus_lookup_table_append_label(table, ibus_text_new_from_string(" "));
      g_free(display);
    }
    ibus_lookup_table_set_cursor_pos(table, selected);
    ibus_lookup_table_set_orientation(table, IBUS_ORIENTATION_HORIZONTAL);
    ibus_engine_update_lookup_table(native, table, TRUE);
    engine->visible = TRUE;
    g_object_unref(table);
    g_strfreev(items);
  } else if (engine->visible) {
    ibus_engine_hide_lookup_table(native);
    engine->visible = FALSE;
  }
  if (arm) {
    expect_letter(engine);
    cancel_timer(engine);
    engine->timer = g_timeout_add(hold_delay, expired, engine);
  }
}
/** Stops this process when its owning desktop bus disappears. */
static void disconnected(IBusBus *bus, gpointer data) {
  (void)bus;
  (void)data;
  ibus_quit();
}
int sa_run(SaDispatch dispatch, int register_component, unsigned delay_ms) {
  dispatch_event = dispatch;
  hold_delay = delay_ms;
  ibus_init();
  IBusBus *bus = ibus_bus_new();
  if (!ibus_bus_is_connected(bus)) {
    g_object_unref(bus);
    return 1;
  }
  g_signal_connect(bus, "disconnected", G_CALLBACK(disconnected), NULL);
  IBusFactory *factory = ibus_factory_new(ibus_bus_get_connection(bus));
  g_object_ref_sink(factory);
  ibus_factory_add_engine(factory, "sun-accent-popup", sa_engine_get_type());
  guint reply =
      ibus_bus_request_name(bus, "org.freedesktop.IBus.SunAccentPopup", 4);
  if (reply != 1) {
    g_object_unref(factory);
    g_object_unref(bus);
    return 2;
  }
  if (register_component) {
    IBusComponent *component = ibus_component_new(
        "org.freedesktop.IBus.SunAccentPopup", "Hold keys for accents", "0.1.0",
        "MIT", "namo-robotics",
        "https://github.com/namo-robotics/sun_accent_popup", "", "");
    g_object_ref_sink(component);
    ibus_component_add_engine(
        component,
        ibus_engine_desc_new("sun-accent-popup", "Sun Accent Popup",
                             "Hold a letter to select an accent", "en", "MIT",
                             "namo-robotics", "input-keyboard", "us"));
    gboolean registered = ibus_bus_register_component(bus, component);
    g_object_unref(component);
    if (!registered) {
      g_object_unref(factory);
      g_object_unref(bus);
      return 3;
    }
  }
  ibus_main();
  ibus_object_destroy(IBUS_OBJECT(factory));
  g_object_unref(factory);
  g_object_unref(bus);
  return 0;
}
