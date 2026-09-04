#include "dusk/interp/actor_pose.h"

#include "dusk/interp/frame_interpolation.h"
#include "dusk/interp/lerp.h"
#include "f_op/f_op_actor_mng.h"

#include <absl/container/flat_hash_map.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

struct ActorPoseSnapshot {
    cXyz position{};
    cXyz attentionPosition{};
    cXyz eyePosition{};
    csXyz shapeAngle{};
    s8 roomNo = 0;
};

struct ActorPoseRecord {
    fpc_ProcID processId = fpcM_ERROR_PROCESS_ID_e;
    ActorPoseSnapshot previous{};
    ActorPoseSnapshot current{};
    bool previousValid = false;
    bool currentValid = false;
    bool discontinuous = false;
};

absl::flat_hash_map<uintptr_t, ActorPoseRecord> s_actorPoses;

f32 distance_between(const cXyz& lhs, const cXyz& rhs) {
    const f32 x = lhs.x - rhs.x;
    const f32 y = lhs.y - rhs.y;
    const f32 z = lhs.z - rhs.z;
    return sqrtf(x * x + y * y + z * z);
}

ActorPoseSnapshot actor_pose_snapshot(const fopAc_ac_c& actor) {
    return {
        .position = actor.current.pos,
        .attentionPosition = actor.attention_info.position,
        .eyePosition = actor.eyePos,
        .shapeAngle = actor.shape_angle,
        .roomNo = actor.current.roomNo,
    };
}

bool actor_pose_discontinuous(const ActorPoseSnapshot& previous, const ActorPoseSnapshot& current) {
    constexpr f32 kTeleportDistance = 2000.0f;
    return previous.roomNo != current.roomNo ||
           distance_between(previous.position, current.position) > kTeleportDistance;
}

void roll_actor_poses() {
    for (auto& entry : s_actorPoses) {
        ActorPoseRecord& record = entry.second;
        if (record.currentValid) {
            record.previous = record.current;
            record.previousValid = true;
            record.discontinuous = false;
        }
    }
}

}  // namespace

namespace dusk::interp {

void actor_pose_on_sim_tick() {
    roll_actor_poses();
}

void clear_actor_pose() {
    s_actorPoses.clear();
}

void capture_actor_pose(fopAc_ac_c* actor) {
    if (!should_capture() || actor == nullptr) {
        return;
    }

    const uintptr_t key = reinterpret_cast<uintptr_t>(actor);
    const fpc_ProcID processId = fopAcM_GetID(actor);
    auto& record = s_actorPoses[key];
    const ActorPoseSnapshot pose = actor_pose_snapshot(*actor);
    if (record.processId != processId || !record.currentValid) {
        record = {
            .processId = processId,
            .previous = pose,
            .current = pose,
            .previousValid = true,
            .currentValid = true,
            .discontinuous = false,
        };
        return;
    }

    record.current = pose;
    record.currentValid = true;
    if (!record.previousValid || actor_pose_discontinuous(record.previous, record.current)) {
        record.previous = record.current;
        record.previousValid = true;
        record.discontinuous = true;
    }
}

void erase_actor_pose(fopAc_ac_c* actor) {
    if (actor != nullptr) {
        s_actorPoses.erase(reinterpret_cast<uintptr_t>(actor));
    }
}

bool sample_actor_pose(const fopAc_ac_c* actor, float step, ActorPresentationPose* pose) {
    if (actor == nullptr || pose == nullptr) {
        return false;
    }

    const auto it = s_actorPoses.find(reinterpret_cast<uintptr_t>(actor));
    if (it == s_actorPoses.end() || it->second.processId != fopAcM_GetID(actor)) {
        return false;
    }

    const ActorPoseRecord& record = it->second;
    if (!record.previousValid || !record.currentValid) {
        return false;
    }
    if (record.discontinuous) {
        return false;
    }

    step = std::clamp(step, 0.0f, 1.0f);
    lerp(pose->position, record.previous.position, record.current.position, step);
    lerp(pose->attentionPosition, record.previous.attentionPosition,
         record.current.attentionPosition, step);
    lerp(pose->eyePosition, record.previous.eyePosition, record.current.eyePosition, step);
    lerp(pose->shapeAngle, record.previous.shapeAngle, record.current.shapeAngle, step);
    return true;
}

size_t recorded_actor_pose_count() {
    return s_actorPoses.size();
}

}  // namespace dusk::interp
