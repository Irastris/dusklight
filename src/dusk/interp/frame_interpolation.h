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

bool should_capture();

void record_final_mtx(Mtx m, const void* key);
void record_final_mtx(Mtx m);
bool override_presentation_mtx(const void* key, const Mtx value);

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
