#pragma once
#include <stdint.h>

/** Receives synchronous IBus events on the main thread without retaining Sun
 * borrows. */
typedef int (*SaDispatch)(void *, uint32_t, uint32_t, uint32_t, uint32_t);
/** Runs the IBus factory until disconnection; registration is for uninstalled
 * development sessions. */
int sa_run(SaDispatch dispatch, int register_component, unsigned delay_ms);
/** Reads one scalar owned by the current native input context. */
uint32_t sa_get(void *engine, unsigned field);
/** Writes one scalar owned by the current native input context. */
void sa_set(void *engine, unsigned field, uint32_t value);
/** Applies candidate snapshots and replaces a verified letter only after selection. */
void sa_apply(void *engine, const char *choices,
              unsigned selected, const char *commit, int arm);
