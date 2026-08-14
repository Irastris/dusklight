#pragma once

#include "SSystem/SComponent/c_sxyz.h"
#include "SSystem/SComponent/c_xyz.h"

#include <stddef.h>

class fopAc_ac_c;

#ifdef __cplusplus
namespace dusk::interp {

struct ActorPresentationPose {
    cXyz position{};
    cXyz attentionPosition{};
    cXyz eyePosition{};
    csXyz shapeAngle{};
};

void capture_actor_pose(::fopAc_ac_c* actor);
void erase_actor_pose(::fopAc_ac_c* actor);
bool sample_actor_pose(const ::fopAc_ac_c* actor, float step, ActorPresentationPose* pose);
size_t recorded_actor_pose_count();

}  // namespace dusk::interp
#endif
