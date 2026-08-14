#pragma once

#include <stdint.h>

class camera_process_class;
class view_class;

#ifdef __cplusplus
namespace dusk::interp {

enum class CameraInterpolationKind {
    Unavailable,
    Authoritative,
    Previous,
    Linear,
    Orbit,
    SemanticOrbit,
};

enum class CameraInterpolationFallbackReason {
    None,
    MissingSnapshots,
    IncompatibleCamera,
    UnsupportedAlgorithm,
    MissingTarget,
    TargetChanged,
    TargetPoseUnavailable,
};

struct CameraInterpolationDiagnostics {
    CameraInterpolationKind kind = CameraInterpolationKind::Unavailable;
    float step = 0.0f;
    float previousRadius = 0.0f;
    float currentRadius = 0.0f;
    float presentedRadius = 0.0f;
    float linearRadius = 0.0f;
    float linearRadiusError = 0.0f;
    float maxLinearRadiusError = 0.0f;
    float cameraFrames = 0.0f;
    float collisionCorrection = 0.0f;
    float maxCollisionCorrection = 0.0f;
    uint64_t simTickSeq = 0;
    uint64_t collisionHitCount = 0;
    int algorithm = -1;
    int mode = -1;
    int type = -1;
    int style = -1;
    CameraInterpolationFallbackReason fallbackReason =
        CameraInterpolationFallbackReason::MissingSnapshots;
    bool compatibleRig = false;
    bool actorAnchored = false;
    bool collisionHit = false;
    bool rebased = false;
    bool valid = false;
};

void record_camera(::camera_process_class* cam, int camera_id);
void reset_camera();
void interp_view(::view_class* view);
const CameraInterpolationDiagnostics& camera_interpolation_diagnostics();

}  // namespace dusk::interp
#endif
