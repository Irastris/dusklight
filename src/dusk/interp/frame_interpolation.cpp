#include "dusk/interp/frame_interpolation.h"
#include "dusk/interp/lerp.h"

#include "dusk/game_clock.h"
#include "mtx.h"

#include <absl/container/flat_hash_map.h>
#include <vector>

namespace dusk::interp {
void camera_on_sim_tick();
void camera_on_begin_record();
bool camera_apply_presentation();
void camera_restore_presentation();
void camera_invalidate_snapshots();
}  // namespace dusk::interp

namespace {

struct Recording {
    absl::flat_hash_map<uintptr_t, Mtx> matrix_values;
};

bool g_enabled = false;
bool s_recording = false;
bool g_interpolating = false;
bool s_syncPresentation = false;

float s_step = 0.0f;
bool g_is_sim_frame = false;
bool s_uiTickPending = false;
uint64_t s_simTickSeq = 0;

Recording s_currentRecording;
Recording s_previousRecording;

absl::flat_hash_map<uintptr_t, Mtx> g_replacements;

int s_presentationDepth = 0;

const Mtx* resolve_replacement(const Mtx* source, Mtx* scratch) {
    if (!g_interpolating || source == nullptr || dusk::interp::presentation_sync_active()) {
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

struct InterpolationCallBackWork {
    dusk::interp::InterpolationCallBack pCallBack;
    void* pUserWork;
};

std::vector<InterpolationCallBackWork> s_interpolationCallBackWork;

void clear_callbacks() {
    s_interpolationCallBackWork.clear();
}

void callbacks_run_begin() {
    for (size_t i = 0; i < s_interpolationCallBackWork.size(); i++) {
        auto const& work = s_interpolationCallBackWork[i];
        work.pCallBack(work.pUserWork);
    }
}

}  // namespace

namespace dusk::interp {

void begin_sim_tick() {
    if (!g_enabled) {
        return;
    }

    clear_callbacks();
    camera_on_sim_tick();
    ++s_simTickSeq;
}

uint64_t sim_tick_seq() {
    return s_simTickSeq;
}

void begin_frame(FrameInterpMode mode, bool is_sim_frame, float step) {
    g_enabled = mode != FrameInterpMode::Off;
    g_is_sim_frame = is_sim_frame;
    s_step = std::clamp(step, 0.0f, 1.0f);
    if (!g_enabled) {
        g_interpolating = false;
        clear_replacements();
    }
}

bool is_enabled() {
    return g_enabled;
}

bool is_sim_frame() {
    return g_is_sim_frame;
}

bool is_presentation_frame() {
    return !game_clock::g_frameTiming.separatePresentation || !game_clock::is_sim_tick_active();
}

bool is_presentation_active() {
    return s_presentationDepth > 0;
}

void begin_record() {
    if (!g_enabled) {
        g_interpolating = false;
        s_syncPresentation = false;
        s_previousRecording = {};
        s_currentRecording = {};
        clear_replacements();
        camera_invalidate_snapshots();
        return;
    }

    s_syncPresentation = false;
    s_previousRecording = std::move(s_currentRecording);
    s_currentRecording = {};
    s_recording = true;
    g_interpolating = false;
    clear_replacements();
    camera_on_begin_record();
}

void end_record() {
    s_recording = false;
}

void interpolate() {
    clear_replacements();
    g_interpolating = g_enabled && !s_recording && !s_syncPresentation && has_recording_data(s_currentRecording);
    if (!g_interpolating) {
        return;
    }
    for (auto const& old : s_previousRecording.matrix_values) {
        if (auto it = s_currentRecording.matrix_values.find(old.first);
            it != s_currentRecording.matrix_values.end())
        {
            lerp(g_replacements[old.first], old.second, it->second, s_step);
        }
    }
}

void request_presentation_sync() {
    if (!g_enabled) {
        return;
    }
    s_syncPresentation = true;
}

bool presentation_sync_active() {
    if (!g_enabled) {
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
    return g_enabled ? s_uiTickPending : true;
}

void record_final_mtx(Mtx m, const void* key) {
    if (!s_recording || m == nullptr) {
        return;
    }

    auto& it = s_currentRecording.matrix_values[reinterpret_cast<uintptr_t>(key)];
    MTXCopy(m, it);
}

void record_final_mtx(Mtx m) {
    record_final_mtx(m, m);
}

bool lookup_replacement(const void* key, Mtx out) {
    if (presentation_sync_active() || !g_interpolating || key == nullptr) {
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
    if (presentation_sync_active() || !g_interpolating || lhs == nullptr || rhs == nullptr) {
        return false;
    }

    Mtx lhs_scratch;
    Mtx rhs_scratch;
    const Mtx* resolved_lhs = resolve_replacement(reinterpret_cast<const Mtx*>(lhs), &lhs_scratch);
    const Mtx* resolved_rhs = resolve_replacement(reinterpret_cast<const Mtx*>(rhs), &rhs_scratch);
    if (resolved_lhs == reinterpret_cast<const Mtx*>(lhs) && resolved_rhs == reinterpret_cast<const Mtx*>(rhs)) {
        return false;
    }

    MTXConcat(*resolved_lhs, *resolved_rhs, out);
    return true;
}

void begin_presentation() {
    if (!g_enabled) {
        return;
    }
    if (s_presentationDepth > 0) {
        s_presentationDepth++;
        return;
    }
    if (!camera_apply_presentation()) {
        return;
    }

    s_presentationDepth = 1;
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

    camera_restore_presentation();
}


void add_interpolation_callback(InterpolationCallBack pCallBack, void* pUserWork) {
    if (!is_enabled() || is_presentation_active() || !is_sim_frame()) {
        return;
    }

    s_interpolationCallBackWork.emplace_back(pCallBack, pUserWork);
}

}  // namespace dusk::interp
