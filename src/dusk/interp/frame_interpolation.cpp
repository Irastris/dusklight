#include "dusk/interp/frame_interpolation.h"

#include "dusk/game_clock.h"
#include "dusk/interp/dual_buffer.h"
#include "dusk/interp/lerp.h"
#include "dusk/interp/matrix.h"
#include "dusk/interp/skeleton.h"

#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "mtx.h"

#include <absl/container/flat_hash_map.h>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace dusk::interp {
void camera_on_sim_tick();
void camera_on_begin_record();
void camera_apply_presentation();
void camera_restore_presentation();
void clear_camera();
void actor_pose_on_sim_tick();
void clear_actor_pose();
}  // namespace dusk::interp

namespace {

struct Recording {
    absl::flat_hash_map<uintptr_t, dusk::interp::matrix::MatrixSample> matrix_values;
};

bool s_recording = false;
bool s_replacementsActive = false;
bool s_syncPresentation = false;

float s_step = 0.0f;
bool s_uiTickPending = false;
uint64_t s_simTickSeq = 0;
uint64_t s_observedPresentationEpoch = 0;

Recording s_currentRecording;
Recording s_previousRecording;

absl::flat_hash_map<uintptr_t, Mtx> g_replacements;

int s_presentationDepth = 0;

const Mtx* resolve_replacement(const Mtx* source, Mtx* scratch) {
    if (!s_replacementsActive || source == nullptr ||
        dusk::interp::presentation_sync_active())
    {
        return source;
    }

    auto it = g_replacements.find(reinterpret_cast<uintptr_t>(source));
    if (it == g_replacements.end()) {
        return source;
    }

    MTXCopy(it->second, *scratch);
    return scratch;
}

bool has_recording_data(const Recording& recording) {
    return !recording.matrix_values.empty();
}

void clear_replacements() {
    g_replacements.clear();
}

void interpolate_replacements() {
    clear_replacements();
    s_replacementsActive = dusk::interp::is_enabled() && !s_recording && !s_syncPresentation
                           && has_recording_data(s_currentRecording);
    if (!s_replacementsActive) {
        return;
    }
    for (auto const& old : s_previousRecording.matrix_values) {
        if (auto it = s_currentRecording.matrix_values.find(old.first);
            it != s_currentRecording.matrix_values.end())
        {
            dusk::interp::matrix::interpolate(
                g_replacements[old.first], old.second, it->second, s_step);
        }
    }
}

struct InterpolationCallBackWork {
    dusk::interp::InterpolationCallBack begin;
    dusk::interp::InterpolationCallBack end;
    void* pUserWork;
};

std::vector<InterpolationCallBackWork> s_interpolationCallBackWork;

struct ModelInterpolationCallBackWork {
    dusk::interp::InterpolationCallBack before;
    dusk::interp::InterpolationCallBack after;
    void* pUserWork;
};

absl::flat_hash_map<const J3DModel*, ModelInterpolationCallBackWork>
    s_modelInterpolationCallBackWork;

void clear_callbacks() {
    s_interpolationCallBackWork.clear();
    s_modelInterpolationCallBackWork.clear();
}

void callbacks_run_begin() {
    for (const auto& work : s_interpolationCallBackWork) {
        if (work.begin != nullptr) {
            work.begin(work.pUserWork);
        }
    }
}

void callbacks_run_end() {
    for (size_t i = s_interpolationCallBackWork.size(); i > 0; --i) {
        const auto& work = s_interpolationCallBackWork[i - 1];
        if (work.end != nullptr) {
            work.end(work.pUserWork);
        }
    }
}

void clear_interpolation_history() {
    s_recording = false;
    s_replacementsActive = false;
    s_syncPresentation = false;
    s_previousRecording = {};
    s_currentRecording = {};
    clear_replacements();
    dusk::interp::clear_actor_pose();
    dusk::interp::clear_owned_buffers();
    dusk::interp::clear_weather_buffers();
    clear_callbacks();
    dusk::interp::clear_camera();
    dusk::interp::skeleton::clear();
    s_presentationDepth = 0;
}

}  // namespace

namespace dusk::interp {

void begin_sim_tick() {
    if (!is_enabled()) {
        return;
    }

    clear_callbacks();
    camera_on_sim_tick();
    actor_pose_on_sim_tick();
    ++s_simTickSeq;
    skeleton::begin_sim_tick();
}

uint64_t sim_tick_seq() {
    return s_simTickSeq;
}

void begin_frame(float step) {
    const game_clock::FrameTiming& timing = game_clock::g_frameTiming;
    if (s_observedPresentationEpoch != timing.presentationEpoch) {
        s_observedPresentationEpoch = timing.presentationEpoch;
        clear_interpolation_history();
    }

    s_step = std::clamp(step, 0.0f, 1.0f);
    if (!is_enabled()) {
        clear_interpolation_history();
    }
}

bool is_enabled() {
    return game_clock::g_frameTiming.interpolating;
}

void begin_record() {
    if (!is_enabled()) {
        clear_interpolation_history();
        return;
    }

    s_syncPresentation = false;
    s_previousRecording = std::move(s_currentRecording);
    s_currentRecording = {};
    s_recording = true;
    s_replacementsActive = false;
    clear_replacements();
    camera_on_begin_record();
}

void end_record() {
    s_recording = false;
    for (auto& entry : s_currentRecording.matrix_values) {
        dusk::interp::matrix::finalize(&entry.second);
    }
}

void request_presentation_sync() {
    if (!is_enabled()) {
        return;
    }
    s_syncPresentation = true;
}

bool presentation_sync_active() {
    if (!is_enabled()) {
        return false;
    }
    return s_syncPresentation;
}

float get_interpolation_step() {
    return presentation_sync_active() ? 1.0f : s_step;
}

void set_ui_tick_pending(bool value) {
    if (s_uiTickPending == value) { return; }
    s_uiTickPending = value;
}

bool get_ui_tick_pending() {
    return is_enabled() ? s_uiTickPending : true;
}

void record_final_mtx(Mtx m, const void* key) {
    if (!s_recording || m == nullptr) {
        return;
    }

    auto& sample = s_currentRecording.matrix_values[reinterpret_cast<uintptr_t>(key)];
    dusk::interp::matrix::record(&sample, m);
}

void record_final_mtx(Mtx m) {
    record_final_mtx(m, m);
}

bool override_presentation_mtx(const void* key, const Mtx value) {
    if (!s_replacementsActive || presentation_sync_active() || key == nullptr || value == nullptr) {
        return false;
    }

    MTXCopy(value, g_replacements[reinterpret_cast<uintptr_t>(key)]);
    return true;
}

bool lookup_replacement(const void* key, Mtx out) {
    if (presentation_sync_active() || !s_replacementsActive || key == nullptr) {
        return false;
    }

    auto it = g_replacements.find(reinterpret_cast<uintptr_t>(key));
    if (it == g_replacements.end()) {
        return false;
    }

    MTXCopy(it->second, out);
    return true;
}

bool lookup_concat_replacement(const void* lhs, const void* rhs, Mtx out) {
    if (presentation_sync_active() || !s_replacementsActive || lhs == nullptr || rhs == nullptr) {
        return false;
    }

    Mtx lhs_scratch;
    Mtx rhs_scratch;
    const Mtx* resolved_lhs = resolve_replacement(reinterpret_cast<const Mtx*>(lhs), &lhs_scratch);
    const Mtx* resolved_rhs = resolve_replacement(reinterpret_cast<const Mtx*>(rhs), &rhs_scratch);
    if (resolved_lhs == reinterpret_cast<const Mtx*>(lhs) &&
        resolved_rhs == reinterpret_cast<const Mtx*>(rhs))
    {
        return false;
    }

    MTXConcat(*resolved_lhs, *resolved_rhs, out);
    return true;
}

void begin_presentation(float step) {
    begin_frame(step);
    if (!is_enabled()) {
        return;
    }

    interpolate_replacements();

    if (s_presentationDepth > 0) {
        s_presentationDepth++;
        return;
    }

    s_presentationDepth = 1;
    camera_apply_presentation();
    callbacks_run_begin();
}

void end_presentation() {
    if (s_presentationDepth == 0) {
        return;
    }
    s_presentationDepth--;
    if (s_presentationDepth > 0) {
        return;
    }

    callbacks_run_end();
    camera_restore_presentation();
}

bool is_presentation_active() {
    return s_presentationDepth > 0;
}


void add_interpolation_callback(InterpolationCallBack pCallBack, void* pUserWork) {
    add_presentation_callbacks(pCallBack, nullptr, pUserWork);
}

void add_presentation_callbacks(InterpolationCallBack begin, InterpolationCallBack end,
                                void* pUserWork) {
    if (!is_enabled() || is_presentation_active() || !game_clock::is_sim_frame()) {
        return;
    }
    if (begin == nullptr && end == nullptr) {
        return;
    }

    s_interpolationCallBackWork.push_back({begin, end, pUserWork});
}

void add_model_interpolation_callbacks(J3DModel* model, InterpolationCallBack before,
                                       InterpolationCallBack after, void* pUserWork) {
    if (!is_enabled() || is_presentation_active() || !game_clock::is_sim_frame() ||
        model == nullptr)
    {
        return;
    }

    s_modelInterpolationCallBackWork[model] = {before, after, pUserWork};
}

bool has_model_interpolation_callbacks(const J3DModel* model) {
    return s_modelInterpolationCallBackWork.contains(model);
}

void begin_model_interpolation(J3DModel* model) {
    auto it = s_modelInterpolationCallBackWork.find(model);
    if (it != s_modelInterpolationCallBackWork.end() && it->second.before != nullptr) {
        it->second.before(it->second.pUserWork);
    }
}

void end_model_interpolation(J3DModel* model) {
    auto it = s_modelInterpolationCallBackWork.find(model);
    if (it != s_modelInterpolationCallBackWork.end() && it->second.after != nullptr) {
        it->second.after(it->second.pUserWork);
    }
}

}  // namespace dusk::interp
