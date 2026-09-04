#include "dusk/interp/camera.h"

#include "dusk/interp/actor_pose.h"
#include "dusk/interp/frame_interpolation.h"
#include "dusk/game_clock.h"
#include "dusk/interp/lerp.h"

#include "d/d_bg_s_lin_chk.h"
#include "d/d_camera.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_camera_mng.h"
#include "m_Do/m_Do_graphic.h"
#include "m_Do/m_Do_lib.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <utility>

namespace {

struct CameraSnapshot {
    cXyz eye{};
    cXyz center{};
    cXyz up{};
    s16 bank = 0;
    f32 fovy = 0.f;
    f32 aspect = 0.f;
    f32 near_ = 0.f;
    f32 far_ = 0.f;
    int mode = 0;
    int type = 0;
    int style = 0;
    int algorithm = -1;
    int roomNo = 0;
    const fopAc_ac_c* targetActor = nullptr;
    fpc_ProcID targetActorId = fpcM_ERROR_PROCESS_ID_e;
    cXyz targetAttentionPosition{};
    const fopAc_ac_c* secondaryTargetActor = nullptr;
    fpc_ProcID secondaryTargetActorId = fpcM_ERROR_PROCESS_ID_e;
    cXyz secondaryTargetAttentionPosition{};
    u32 collisionFlags = 0;
    f32 gazeBackMargin = 0.f;
    Mtx44 projViewMtx{};
    bool active = false;
    bool wideZoom = false;
    bool valid = false;
};

void cache_snapshot_proj_view(CameraSnapshot* dst) {
    Mtx44 proj;
    Mtx viewMtx;
    C_MTXPerspective(proj, dst->fovy, dst->aspect, dst->near_, dst->far_);
#if WIDESCREEN_SUPPORT
    mDoGph_gInf_c::setWideZoomProjection(proj);
#endif
    mDoMtx_lookAt(viewMtx, &dst->eye, &dst->center, &dst->up, dst->bank);
    cMtx_concatProjView(proj, viewMtx, dst->projViewMtx);
}

CameraSnapshot s_camPrev{};
CameraSnapshot s_camCurr{};
const camera_process_class* s_cameraOwner = nullptr;
dusk::interp::CameraInterpolationDiagnostics s_cameraDiagnostics{};
f32 s_maxLinearRadiusError = 0.0f;
f32 s_maxCollisionCorrection = 0.0f;
uint64_t s_collisionHitCount = 0;

struct PresentationIntervalState {
    uint64_t simTickSeq = 0;
    f32 lastStep = 0.f;
    bool valid = false;
};

PresentationIntervalState s_presentationInterval{};

view_class s_presentationViewBackup{};
bool s_presentationCameraApplied = false;

void copy_camera_to_snap(CameraSnapshot* dst, camera_process_class* camera) {
    const view_class& v = camera->view;
    dst->eye = v.lookat.eye;
    dst->center = v.lookat.center;
    dst->up = v.lookat.up;
    dst->bank = v.bank;
    dst->fovy = v.fovy;
    dst->aspect = v.aspect;
    dst->near_ = v.near_;
    dst->far_ = v.far_;
    dst->mode = camera->mCamera.Mode();
    dst->type = camera->mCamera.Type();
    dst->style = camera->mCamera.mCamStyle;
    dst->algorithm = camera->mCamera.mCamParam.Algorythmn(dst->style);
    dst->roomNo = camera->mCamera.mRoomCtx.mRoomNo;
    dst->active = camera->mCamera.Active();
    dst->collisionFlags = camera->mCamera.mBumpCheckFlags;
    dst->gazeBackMargin = camera->mCamera.mCamSetup.mBGChk.GazeBackMargin() + 0.5f;
    if (dst->active && (dst->algorithm == 1 || dst->algorithm == 2) &&
        camera->mCamera.mpPlayerActor != nullptr)
    {
        dst->targetActor = camera->mCamera.mpPlayerActor;
        dst->targetActorId = fopAcM_GetID(dst->targetActor);
        dst->targetAttentionPosition = dst->targetActor->attention_info.position;
    } else {
        dst->targetActor = nullptr;
        dst->targetActorId = fpcM_ERROR_PROCESS_ID_e;
        dst->targetAttentionPosition = cXyz{};
    }
    if (dst->active && dst->algorithm == 2 && camera->mCamera.mpLockonTarget != nullptr) {
        dst->secondaryTargetActor = camera->mCamera.mpLockonTarget;
        dst->secondaryTargetActorId = fopAcM_GetID(dst->secondaryTargetActor);
        dst->secondaryTargetAttentionPosition = dst->secondaryTargetActor->attention_info.position;
    } else {
        dst->secondaryTargetActor = nullptr;
        dst->secondaryTargetActorId = fpcM_ERROR_PROCESS_ID_e;
        dst->secondaryTargetAttentionPosition = cXyz{};
    }
    dst->valid = true;
    cache_snapshot_proj_view(dst);
}

struct SphericalOffset {
    f32 radius;
    f32 yaw;
    f32 pitch;
};

constexpr f32 kMinimumOrbitRadius = 0.01f;

SphericalOffset spherical_offset(const cXyz& eye, const cXyz& center) {
    const f32 x = eye.x - center.x;
    const f32 y = eye.y - center.y;
    const f32 z = eye.z - center.z;
    const f32 horizontal = sqrtf(x * x + z * z);
    return {
        .radius = sqrtf(horizontal * horizontal + y * y),
        .yaw = atan2f(x, z),
        .pitch = atan2f(y, horizontal),
    };
}

f32 lerp_angle_radians(f32 lhs, f32 rhs, f32 step) {
    return lhs + remainderf(rhs - lhs, 2.0f * static_cast<f32>(M_PI)) * step;
}

cXyz cartesian_offset(const SphericalOffset& offset) {
    const f32 horizontal = offset.radius * cosf(offset.pitch);
    return {
        horizontal * sinf(offset.yaw),
        offset.radius * sinf(offset.pitch),
        horizontal * cosf(offset.yaw),
    };
}

bool same_camera_rig(const CameraSnapshot& lhs, const CameraSnapshot& rhs) {
    return lhs.active && rhs.active && lhs.mode == rhs.mode && lhs.type == rhs.type &&
           lhs.style == rhs.style && lhs.algorithm == rhs.algorithm && lhs.roomNo == rhs.roomNo;
}

bool same_camera_target(const CameraSnapshot& lhs, const CameraSnapshot& rhs) {
    return lhs.targetActor != nullptr && lhs.targetActor == rhs.targetActor &&
           lhs.targetActorId == rhs.targetActorId;
}

bool same_secondary_camera_target(const CameraSnapshot& lhs, const CameraSnapshot& rhs) {
    return lhs.secondaryTargetActor != nullptr &&
           lhs.secondaryTargetActor == rhs.secondaryTargetActor &&
           lhs.secondaryTargetActorId == rhs.secondaryTargetActorId;
}

cXyz midpoint(const cXyz& lhs, const cXyz& rhs) {
    return (lhs + rhs) * 0.5f;
}

bool interpolate_camera_orbit(cXyz* eye, const CameraSnapshot& prev, const CameraSnapshot& curr,
                              const cXyz& center, f32 step) {
    if (!same_camera_rig(prev, curr)) {
        return false;
    }

    const auto prevOffset = spherical_offset(prev.eye, prev.center);
    const auto currOffset = spherical_offset(curr.eye, curr.center);
    if (prevOffset.radius < kMinimumOrbitRadius || currOffset.radius < kMinimumOrbitRadius) {
        return false;
    }

    const SphericalOffset offset{
        .radius = prevOffset.radius + (currOffset.radius - prevOffset.radius) * step,
        .yaw = lerp_angle_radians(prevOffset.yaw, currOffset.yaw, step),
        .pitch = lerp_angle_radians(prevOffset.pitch, currOffset.pitch, step),
    };
    const cXyz relativeEye = cartesian_offset(offset);
    *eye = cXyz{
        center.x + relativeEye.x,
        center.y + relativeEye.y,
        center.z + relativeEye.z,
    };
    return true;
}

f32 distance_between(const cXyz& lhs, const cXyz& rhs) {
    const f32 x = lhs.x - rhs.x;
    const f32 y = lhs.y - rhs.y;
    const f32 z = lhs.z - rhs.z;
    return sqrtf(x * x + y * y + z * z);
}

bool evaluate_semantic_orbit(cXyz* eye, cXyz* center, const CameraSnapshot& prev,
                             const CameraSnapshot& curr, f32 step,
                             dusk::interp::CameraInterpolationFallbackReason* reason) {
    using FallbackReason = dusk::interp::CameraInterpolationFallbackReason;

    if (!same_camera_rig(prev, curr)) {
        *reason = FallbackReason::IncompatibleCamera;
        return false;
    }
    if (prev.algorithm != 1 && prev.algorithm != 2) {
        *reason = FallbackReason::UnsupportedAlgorithm;
        return false;
    }
    if (prev.targetActor == nullptr || curr.targetActor == nullptr) {
        *reason = FallbackReason::MissingTarget;
        return false;
    }
    if (!same_camera_target(prev, curr)) {
        *reason = FallbackReason::TargetChanged;
        return false;
    }

    dusk::interp::ActorPresentationPose targetPose;
    if (!dusk::interp::sample_actor_pose(curr.targetActor, step, &targetPose)) {
        *reason = FallbackReason::TargetPoseUnavailable;
        return false;
    }

    cXyz previousAnchor = prev.targetAttentionPosition;
    cXyz currentAnchor = curr.targetAttentionPosition;
    cXyz presentedAnchor = targetPose.attentionPosition;
    if (prev.algorithm == 2) {
        if (prev.secondaryTargetActor == nullptr || curr.secondaryTargetActor == nullptr) {
            *reason = FallbackReason::MissingTarget;
            return false;
        }
        if (!same_secondary_camera_target(prev, curr)) {
            *reason = FallbackReason::TargetChanged;
            return false;
        }
        dusk::interp::ActorPresentationPose secondaryPose;
        if (!dusk::interp::sample_actor_pose(curr.secondaryTargetActor, step, &secondaryPose))
        {
            *reason = FallbackReason::TargetPoseUnavailable;
            return false;
        }
        previousAnchor = midpoint(previousAnchor, prev.secondaryTargetAttentionPosition);
        currentAnchor = midpoint(currentAnchor, curr.secondaryTargetAttentionPosition);
        presentedAnchor = midpoint(presentedAnchor, secondaryPose.attentionPosition);
    }

    const cXyz previousOffset = prev.center - previousAnchor;
    const cXyz currentOffset = curr.center - currentAnchor;
    cXyz centerOffset;
    dusk::interp::lerp(centerOffset, previousOffset, currentOffset, step);
    *center = presentedAnchor + centerOffset;
    if (!interpolate_camera_orbit(eye, prev, curr, *center, step)) {
        *reason = FallbackReason::IncompatibleCamera;
        return false;
    }

    *reason = FallbackReason::None;
    return true;
}

bool clamp_presentation_eye(cXyz* eye, const cXyz& center, const CameraSnapshot& camera,
                            f32* correction) {
    if ((camera.collisionFlags & 0xb7) == 0 || camera.gazeBackMargin < 0.0f) {
        return false;
    }

    cXyz direction = *eye - center;
    const f32 distance = direction.abs();
    if (distance <= kMinimumOrbitRadius) {
        return false;
    }
    direction *= 1.0f / distance;
    const cXyz probe = *eye + direction * camera.gazeBackMargin;

    dBgS_CamLinChk lineCheck;
    if (camera.collisionFlags & 0x8000) {
        lineCheck.ClrCam();
        lineCheck.SetObj();
    } else {
        lineCheck.ClrObj();
        lineCheck.SetCam();
    }
    lineCheck.Set(&center, &probe, nullptr);
    if (camera.collisionFlags & 4) {
        lineCheck.ClrSttsRoofOff();
    } else {
        lineCheck.SetSttsRoofOff();
    }
    if (camera.collisionFlags & 2) {
        lineCheck.ClrSttsWallOff();
    } else {
        lineCheck.SetSttsWallOff();
    }
    if (camera.collisionFlags & 1) {
        lineCheck.ClrSttsGroundOff();
    } else {
        lineCheck.SetSttsGroundOff();
    }
    if (camera.collisionFlags & 8) {
        lineCheck.OnWaterGrp();
    } else {
        lineCheck.OffWaterGrp();
    }
    if (!dComIfG_Bgsp().LineCross(&lineCheck)) {
        return false;
    }

    cM3dGPla plane;
    if (!dComIfG_Bgsp().GetTriPla(lineCheck, &plane)) {
        return false;
    }

    const cXyz unclampedEye = *eye;
    *eye = lineCheck.GetCross() + *plane.GetNP() * camera.gazeBackMargin;
    *correction = distance_between(unclampedEye, *eye);
    return true;
}

}  // namespace

namespace dusk::interp {

void record_camera(camera_process_class* cam, int camera_id) {
    if (!is_enabled() || camera_id != 0 || cam == nullptr) {
        return;
    }
    if (s_cameraOwner != cam) {
        reset_camera();
        s_cameraOwner = cam;
    }
    copy_camera_to_snap(&s_camCurr, cam);
#if WIDESCREEN_SUPPORT
    s_camCurr.wideZoom = mDoGph_gInf_c::isWideZoom();
#endif
}

void reset_camera() {
    s_camPrev = {};
    s_camCurr = {};
    s_cameraOwner = nullptr;
    s_cameraDiagnostics = {};
    s_maxLinearRadiusError = 0.0f;
    s_maxCollisionCorrection = 0.0f;
    s_collisionHitCount = 0;
    s_presentationInterval = {};
}

const CameraInterpolationDiagnostics& camera_interpolation_diagnostics() {
    return s_cameraDiagnostics;
}

bool project_recorded_pair(Vec const* previous_world, Vec const* current_world, float step, Vec* out_screen) {
    if (previous_world == nullptr || current_world == nullptr || out_screen == nullptr ||
        !s_camPrev.valid || !s_camCurr.valid)
    {
        return false;
    }

    if (dComIfGd_getView() == nullptr) {
        return false;
    }

    Vec previous_screen;
    Vec current_screen;
    mDoLib_project(const_cast<Vec*>(previous_world), &previous_screen, &s_camPrev.projViewMtx);
    mDoLib_project(const_cast<Vec*>(current_world), &current_screen, &s_camCurr.projViewMtx);

    out_screen->x = lerp(previous_screen.x, current_screen.x, step);
    out_screen->y = lerp(previous_screen.y, current_screen.y, step);
    out_screen->z = lerp(previous_screen.z, current_screen.z, step);
    return true;
}

void interp_view(view_class* view) {
    if (!is_enabled()) {
        s_cameraDiagnostics = {};
        return;
    }

    if (!s_camPrev.valid || !s_camCurr.valid) {
        s_cameraDiagnostics = {};
        return;
    }

    const f32 step = get_interpolation_step();
    const bool is_cam_curr_authoritative = game_clock::is_sim_frame() && step <= 0.0f;
    bool rebased = false;
    f32 cameraFrames = 0.0f;
    if (!game_clock::is_sim_frame()) {
        if (!s_presentationInterval.valid || s_presentationInterval.simTickSeq != sim_tick_seq() ||
            step < s_presentationInterval.lastStep)
        {
            s_presentationInterval = {
                .simTickSeq = sim_tick_seq(),
                .lastStep = 0.0f,
                .valid = true,
            };
            rebased = true;
        }
        cameraFrames = step - s_presentationInterval.lastStep;
        s_presentationInterval.lastStep = step;
    }

    const f32 previousRadius = distance_between(s_camPrev.eye, s_camPrev.center);
    const f32 currentRadius = distance_between(s_camCurr.eye, s_camCurr.center);
    cXyz linearCenter;
    cXyz linearEye;
    lerp(linearCenter, s_camPrev.center, s_camCurr.center, step);
    lerp(linearEye, s_camPrev.eye, s_camCurr.eye, step);
    const f32 linearRadius = distance_between(linearEye, linearCenter);
    const f32 orbitRadius = previousRadius + (currentRadius - previousRadius) * step;
    const f32 linearRadiusError = fabsf(orbitRadius - linearRadius);
    s_maxLinearRadiusError = std::max(s_maxLinearRadiusError, linearRadiusError);
    s_cameraDiagnostics = {
        .kind = CameraInterpolationKind::Unavailable,
        .step = step,
        .previousRadius = previousRadius,
        .currentRadius = currentRadius,
        .linearRadius = linearRadius,
        .linearRadiusError = linearRadiusError,
        .maxLinearRadiusError = s_maxLinearRadiusError,
        .cameraFrames = cameraFrames,
        .maxCollisionCorrection = s_maxCollisionCorrection,
        .simTickSeq = sim_tick_seq(),
        .collisionHitCount = s_collisionHitCount,
        .algorithm = s_camCurr.algorithm,
        .mode = s_camCurr.mode,
        .type = s_camCurr.type,
        .style = s_camCurr.style,
        .fallbackReason = CameraInterpolationFallbackReason::None,
        .compatibleRig = same_camera_rig(s_camPrev, s_camCurr),
        .rebased = rebased,
        .valid = true,
    };

    cXyz eye;
    cXyz center;
    cXyz up;
    if (is_cam_curr_authoritative || step >= 1.0f) {
        eye = s_camCurr.eye;
        center = s_camCurr.center;
        up = s_camCurr.up;
        s_cameraDiagnostics.kind = CameraInterpolationKind::Authoritative;
    } else if (step <= 0.0f) {
        eye = s_camPrev.eye;
        center = s_camPrev.center;
        up = s_camPrev.up;
        s_cameraDiagnostics.kind = CameraInterpolationKind::Previous;
    } else {
        CameraInterpolationFallbackReason fallbackReason = CameraInterpolationFallbackReason::None;
        if (evaluate_semantic_orbit(&eye, &center, s_camPrev, s_camCurr, step, &fallbackReason)) {
            s_cameraDiagnostics.kind = CameraInterpolationKind::SemanticOrbit;
            s_cameraDiagnostics.actorAnchored = true;
            s_cameraDiagnostics.collisionHit = clamp_presentation_eye(
                &eye, center, s_camCurr, &s_cameraDiagnostics.collisionCorrection);
            if (s_cameraDiagnostics.collisionHit) {
                ++s_collisionHitCount;
                s_maxCollisionCorrection =
                    std::max(s_maxCollisionCorrection, s_cameraDiagnostics.collisionCorrection);
                s_cameraDiagnostics.collisionHitCount = s_collisionHitCount;
                s_cameraDiagnostics.maxCollisionCorrection = s_maxCollisionCorrection;
            }
        } else {
            s_cameraDiagnostics.fallbackReason = fallbackReason;
            lerp(center, s_camPrev.center, s_camCurr.center, step);
        }
        if (s_cameraDiagnostics.kind == CameraInterpolationKind::SemanticOrbit) {
            // The semantic path already produced both center and eye.
        } else if (interpolate_camera_orbit(&eye, s_camPrev, s_camCurr, center, step)) {
            s_cameraDiagnostics.kind = CameraInterpolationKind::Orbit;
        } else {
            lerp(eye, s_camPrev.eye, s_camCurr.eye, step);
            s_cameraDiagnostics.kind = CameraInterpolationKind::Linear;
        }
        lerp(up, s_camPrev.up, s_camCurr.up, step);
    }
    s_cameraDiagnostics.presentedRadius = distance_between(eye, center);
    if (!up.normalizeRS()) {
        up = s_camCurr.up;
        if (!up.normalizeRS()) {
            up = cXyz{0.0f, 1.0f, 0.0f};
        }
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
    if (mDoGph_gInf_c::isWide() && !mDoGph_gInf_c::isWideZoom() &&
        wide_step >= 0.5f ? s_camCurr.wideZoom : s_camPrev.wideZoom)
    {
        mDoGph_gInf_c::onWideZoom();
    }
#endif
}

void camera_on_sim_tick() {
    s_camPrev = std::move(s_camCurr);
}

void camera_on_begin_record() {
    if (dComIfGp_getCamera(0) == nullptr) {
        s_camPrev.valid = false;
        s_camCurr.valid = false;
    }
}

void clear_camera() {
    reset_camera();
    s_presentationCameraApplied = false;
}

void camera_apply_presentation() {
    if (!s_camPrev.valid || !s_camCurr.valid) {
        return;
    }

    view_class* const view = dComIfGd_getView();
    if (view == nullptr) {
        return;
    }

    std::memcpy(&s_presentationViewBackup, view, sizeof(view_class));
    interp_view(view);
    dCam_applyPresentedView(view);
    s_presentationCameraApplied = true;
}

void camera_restore_presentation() {
    if (!s_presentationCameraApplied) {
        return;
    }
    view_class* const view = dComIfGd_getView();
    if (view != nullptr) {
        std::memcpy(view, &s_presentationViewBackup, sizeof(view_class));
    }
    s_presentationCameraApplied = false;
}

}  // namespace dusk::interp
