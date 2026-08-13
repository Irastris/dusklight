#include "dusk/frame_interpolation.h"

#include "f_op/f_op_camera_mng.h"
#include "m_Do/m_Do_graphic.h"
#include "mtx.h"

#include <absl/container/flat_hash_map.h>
#include <cstdint>
#include <vector>

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

struct CameraSnapshot {
    cXyz eye{};
    cXyz center{};
    cXyz up{};
    s16 bank{};
    f32 fovy{};
    f32 aspect{};
    f32 near_{};
    f32 far_{};
    bool wideZoom{};
    bool valid{};
};

CameraSnapshot s_camPrev{};
CameraSnapshot s_camCurr{};

view_class s_presentationViewBackup{};
int s_presentationDepth = 0;

struct InterpolationCallBackWork {
    dusk::frame_interp::InterpolationCallBack pCallBack;
    void* pUserWork;
};

std::vector<InterpolationCallBackWork> s_interpolationCallBackWork;

void copy_view_to_snap(CameraSnapshot* dst, const view_class& v) {
    dst->eye = v.lookat.eye;
    dst->center = v.lookat.center;
    dst->up = v.lookat.up;
    dst->bank = v.bank;
    dst->fovy = v.fovy;
    dst->aspect = v.aspect;
    dst->near_ = v.near_;
    dst->far_ = v.far_;
    dst->valid = true;
}

const Mtx* resolve_replacement(const Mtx* source, Mtx* scratch) {
    if (!g_interpolating || source == nullptr || dusk::frame_interp::presentation_sync_active()) {
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

}  // namespace

namespace dusk::frame_interp {

void begin_sim_tick() {
    if (!g_enabled) {
        return;
    }

    s_interpolationCallBackWork.clear();
    s_camPrev = std::move(s_camCurr);
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

void begin_record() {
    if (!g_enabled) {
        g_interpolating = false;
        s_syncPresentation = false;
        s_previousRecording = {};
        s_currentRecording = {};
        clear_replacements();
        s_camPrev.valid = false;
        s_camCurr.valid = false;
        return;
    }

    s_syncPresentation = false;
    s_previousRecording = std::move(s_currentRecording);
    s_currentRecording = {};
    s_recording = true;
    g_interpolating = false;
    clear_replacements();

    if (dComIfGp_getCamera(0) == nullptr) {
        s_camPrev.valid = false;
        s_camCurr.valid = false;
    }
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

void record_camera(::camera_process_class* cam, int camera_id) {
    if (!g_enabled || camera_id != 0 || cam == nullptr) {
        return;
    }
    copy_view_to_snap(&s_camCurr, cam->view);
#if WIDESCREEN_SUPPORT
    s_camCurr.wideZoom = mDoGph_gInf_c::isWideZoom();
#endif
}

void interp_view(::view_class* view) {
    if (!g_enabled)
        return;

    if (!s_camPrev.valid || !s_camCurr.valid)
        return;

    const f32 step = get_interpolation_step();
    const bool is_cam_curr_authoritative = g_is_sim_frame && step <= 0.0f;

    cXyz eye;
    cXyz center;
    cXyz up;
    if (is_cam_curr_authoritative) {
        eye = s_camCurr.eye;
        center = s_camCurr.center;
        up = s_camCurr.up;
    } else {
        lerp(eye, s_camPrev.eye, s_camCurr.eye, step);
        lerp(center, s_camPrev.center, s_camCurr.center, step);
        lerp(up, s_camPrev.up, s_camCurr.up, step);
    }
    if (!up.normalizeRS()) {
        up = s_camCurr.up;
        up.normalizeRS();
    }

    view->lookat.eye = eye;
    view->lookat.center = center;
    view->lookat.up = up;
    if (is_cam_curr_authoritative) {
        view->bank = s_camCurr.bank;
        view->fovy = s_camCurr.fovy;
        view->aspect = s_camCurr.aspect;
        view->near_ = s_camCurr.near_;
        view->far_ = s_camCurr.far_;
    } else {
        view->bank = lerp(s_camPrev.bank, s_camCurr.bank, step);
        view->fovy = s_camPrev.fovy + (s_camCurr.fovy - s_camPrev.fovy) * step;
        view->aspect = s_camPrev.aspect + (s_camCurr.aspect - s_camPrev.aspect) * step;
        view->near_ = s_camPrev.near_ + (s_camCurr.near_ - s_camPrev.near_) * step;
        view->far_ = s_camPrev.far_ + (s_camCurr.far_ - s_camPrev.far_) * step;
    }

    // FRAME INTERP TODO: It might be better if I rewired the game to not clear this flag until the
    // next sim frame, but I don't care enough to right now
#if WIDESCREEN_SUPPORT
    const f32 wide_step = is_cam_curr_authoritative ? 1.0f : step;
    if (mDoGph_gInf_c::isWide() && !mDoGph_gInf_c::isWideZoom() && wide_step >= 0.5f ? s_camCurr.wideZoom : s_camPrev.wideZoom) {
        mDoGph_gInf_c::onWideZoom();
    }
#endif
}

static void run_interpolation_callbacks() {
    for (size_t i = 0; i < s_interpolationCallBackWork.size(); i++) {
        auto const& work = s_interpolationCallBackWork[i];
        work.pCallBack(work.pUserWork);
    }
}

void add_interpolation_callback(InterpolationCallBack pCallBack, void* pUserWork) {
    if (!is_enabled() || s_presentationDepth > 0 || !g_is_sim_frame) {
        return;
    }

    s_interpolationCallBackWork.emplace_back(pCallBack, pUserWork);
}

void begin_presentation() {
    if (!g_enabled) {
        return;
    }
    if (s_presentationDepth > 0) {
        s_presentationDepth++;
        return;
    }
    if (!s_camPrev.valid || !s_camCurr.valid) {
        return;
    }

    view_class* const view = dComIfGd_getView();
    if (view == nullptr) {
        return;
    }

    std::memcpy(&s_presentationViewBackup, view, sizeof(view_class));
    interp_view(view);

    // FRAME INTERP TODO: Largely copied from d_camera's camera_draw function from this point, got any better ideas?
    C_MTXPerspective(view->projMtx, view->fovy, view->aspect, view->near_, view->far_);
    mDoMtx_lookAt(view->viewMtx, &view->lookat.eye, &view->lookat.center, &view->lookat.up, view->bank);
#if WIDESCREEN_SUPPORT
    mDoGph_gInf_c::setWideZoomProjection(view->projMtx);
#endif
    j3dSys.setViewMtx(view->viewMtx);
    cMtx_inverse(view->viewMtx, view->invViewMtx);

    bool camera_attention_status = dComIfGp_getCameraAttentionStatus(0) & 0x80;
    Z2GetAudience()->setAudioCamera(view->viewMtx, view->lookat.eye, view->lookat.center, view->fovy, view->aspect, camera_attention_status, 0, false);

    dBgS_GndChk gndchk;
    gndchk.OnWaterGrp();
    gndchk.SetPos(&view->lookat.eye);
    f32 cross = dComIfG_Bgsp().GroundCross(&gndchk);
    if (cross != -G_CM3D_F_INF) {
        if (dComIfG_Bgsp().ChkGrpInf(gndchk, 0x100)) {
            mDoAud_getCameraMapInfo(6);
        } else {
            mDoAud_getCameraMapInfo(dComIfG_Bgsp().GetMtrlSndId(gndchk));
        }
        mDoAud_setCameraGroupInfo(dComIfG_Bgsp().GetGrpSoundId(gndchk));
        Vec spDC;
        spDC.x = view->lookat.eye.x;
        spDC.y = cross;
        spDC.z = view->lookat.eye.z;
        Z2AudioMgr::getInterface()->setCameraPolygonPos(&spDC);
    } else {
        Z2AudioMgr::getInterface()->setCameraPolygonPos(nullptr);
    }

    MTXCopy(view->viewMtx, view->viewMtxNoTrans);
    view->viewMtxNoTrans[0][3] = 0.0f;
    view->viewMtxNoTrans[1][3] = 0.0f;
    view->viewMtxNoTrans[2][3] = 0.0f;
    cMtx_concatProjView(view->projMtx, view->viewMtx, view->projViewMtx);

    f32 far_;
    f32 var_f30;
    if (dComIfGp_getCameraAttentionStatus(0) & 8) {
        far_ = view->far_;
    } else {
#if DEBUG
        if (g_envHIO.mOther.mAdjustCullFar != 0) {
            var_f30 = g_envHIO.mOther.mCullFarValue;
        } else
#endif
        {
            var_f30 = dStage_stagInfo_GetCullPoint(dComIfGp_getStageStagInfo());
        }
        far_ = var_f30;
    }

    mDoLib_clipper::setup(view->fovy, view->aspect, view->near_, far_);

    // FRAME INTERP NOTE: Removed the call to offWideZoom that was here, it causes problems with presentation during cutscenes.

    s_presentationDepth = 1;

    run_interpolation_callbacks();
}

void end_presentation() {
    if (s_presentationDepth == 0) {
        return;
    }
    s_presentationDepth--;
    if (s_presentationDepth > 0) {
        return;
    }

    view_class* const view = dComIfGd_getView();
    if (view != nullptr) {
        std::memcpy(view, &s_presentationViewBackup, sizeof(view_class));
    }
}

namespace {

struct Slot {
    const void* type;
    void* ptr;
    void (*destroy)(void*);
};

using OwnerMap = absl::flat_hash_map<uintptr_t, std::vector<Slot>>;

OwnerMap& owner_map() {
    static OwnerMap s_map;
    return s_map;
}

}  // namespace

void* detail::acquire(const void* key, const void* type, void* (*make)(), void (*destroy)(void*)) {
    const uintptr_t id = reinterpret_cast<uintptr_t>(key);
    auto& slots = owner_map()[id];
    for (Slot& slot : slots) {
        if (slot.type == type) {
            return slot.ptr;
        }
    }

    void* ptr = make();
    slots.push_back({type, ptr, destroy});
    return ptr;
}

void erase_owned_buffers(const void* key) {
    if (key == nullptr) {
        return;
    }

    OwnerMap& stored = owner_map();
    auto it = stored.find(reinterpret_cast<uintptr_t>(key));
    if (it == stored.end()) {
        return;
    }

    for (Slot& slot : it->second) {
        slot.destroy(slot.ptr);
    }
    stored.erase(it);
}

void clear_owned_buffers() {
    OwnerMap& stored = owner_map();
    for (auto& entry : stored) {
        for (Slot& slot : entry.second) {
            slot.destroy(slot.ptr);
        }
    }
    stored.clear();
}
}  // namespace dusk::frame_interp
