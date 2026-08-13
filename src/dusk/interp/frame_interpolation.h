#pragma once

#include "settings.h"

#include <dolphin/mtx.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
namespace dusk::interp {

void begin_record();
void end_record();
void begin_sim_tick();
uint64_t sim_tick_seq();
void begin_frame(FrameInterpMode mode, bool is_sim_frame, float step);
void interpolate();
float get_interpolation_step();

void request_presentation_sync();
bool presentation_sync_active();

bool is_enabled();

// TODO: These should be phased out as UI is progressively updated to use game_clock
void set_ui_tick_pending(bool value);
bool get_ui_tick_pending();

bool is_sim_frame();
bool is_presentation_frame();
bool is_presentation_active();

void record_final_mtx(Mtx m, const void *key);
void record_final_mtx(Mtx m);

bool lookup_replacement(const void* key, Mtx out);
bool lookup_concat_replacement(const void* lhs, const void* rhs, Mtx out);

void begin_presentation();
void end_presentation();

typedef void (*InterpolationCallBack)(void* pUserWork);
void add_interpolation_callback(InterpolationCallBack pCallBack, void* pUserWork);

}  // namespace dusk::interp
#endif
