#pragma once

#include <dolphin/mtx.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

class J3DModel;

#ifdef __cplusplus
namespace dusk::interp {

void begin_record();
void end_record();
void begin_sim_tick();
uint64_t sim_tick_seq();
void begin_frame(float step);
float get_interpolation_step();

void request_presentation_sync();
bool presentation_sync_active();

bool is_enabled();

// TODO: These should be phased out as UI is progressively updated to use game_clock
void set_ui_tick_pending(bool value);
bool get_ui_tick_pending();

void record_final_mtx(Mtx m, const void* key);
void record_final_mtx(Mtx m);

bool lookup_replacement(const void* key, Mtx out);
bool lookup_concat_replacement(const void* lhs, const void* rhs, Mtx out);

void begin_presentation(float step);
void end_presentation();
bool is_presentation_active();

typedef void (*InterpolationCallBack)(void* pUserWork);
void add_interpolation_callback(InterpolationCallBack pCallBack, void* pUserWork);
void add_presentation_callbacks(InterpolationCallBack begin, InterpolationCallBack end,
                                void* pUserWork);
void add_model_interpolation_callbacks(::J3DModel* model, InterpolationCallBack before,
                                       InterpolationCallBack after, void* pUserWork);
bool has_model_interpolation_callbacks(const ::J3DModel* model);
void begin_model_interpolation(::J3DModel* model);
void end_model_interpolation(::J3DModel* model);

}  // namespace dusk::interp
#endif
