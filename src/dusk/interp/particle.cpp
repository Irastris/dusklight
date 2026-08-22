#include "dusk/interp/particle.h"

#include "dusk/interp/frame_interpolation.h"
#include "dusk/interp/lerp.h"

#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"

#include "JSystem/JParticle/JPABaseShape.h"
#include "JSystem/JParticle/JPAChildShape.h"
#include "JSystem/JParticle/JPAEmitter.h"
#include "JSystem/JParticle/JPAParticle.h"
#include "SSystem/SComponent/c_xyz.h"

#include <absl/container/flat_hash_map.h>
#include <cmath>

namespace {

struct ParticleSample {
    cXyz position{};
    cXyz baseAxis{};
    cXyz direction{};
    f32 scaleX = 0.0f;
    f32 scaleY = 0.0f;
    u16 rotateAngle = 0;
    u8 alpha = 0;
    u8 texAnm = 0;
    f32 age = 0.0f;
    GXColor prmClr{};
    GXColor envClr{};
};

struct Record {
    ParticleSample prev{};
    ParticleSample curr{};
    ParticleSample presented{};
    JPABaseParticle const* parent{};
    bool prev_valid = false;
    bool curr_valid = false;
    bool presented_valid = false;
    bool born = false;
    bool inherit_scale = false;
    f32 parent_sim_scaleX = 0.0f;
    f32 parent_sim_scaleY = 0.0f;
    u16 parent_sim_rotate = 0;
};

absl::flat_hash_map<JPABaseParticle const*, Record> s_records;
bool s_recording = false;

Record* record_for(JPABaseParticle const* ptcl) {
    if (ptcl == nullptr) {
        return nullptr;
    }
    auto it = s_records.find(ptcl);
    if (it == s_records.end()) {
        return nullptr;
    }
    return &it->second;
}

void lerp_direction(cXyz& out, const cXyz& lhs, const cXyz& rhs, float step) {
    cXyz a = lhs;
    cXyz b = rhs;
    const bool a_ok = a.normalizeRS();
    const bool b_ok = b.normalizeRS();
    if (a_ok && b_ok) {
        dusk::interp::lerp(out, a, b, step);
        if (!out.normalizeRS()) {
            out = b;
        }
        return;
    }
    out = b_ok ? b : a;
}

void lerp_color(GXColor& out, const GXColor& lhs, const GXColor& rhs, float step) {
    out.r = dusk::interp::lerp(lhs.r, rhs.r, step);
    out.g = dusk::interp::lerp(lhs.g, rhs.g, step);
    out.b = dusk::interp::lerp(lhs.b, rhs.b, step);
    out.a = dusk::interp::lerp(lhs.a, rhs.a, step);
}

void interpolate_pose(ParticleSample& out, const ParticleSample& previous, const ParticleSample& current,
                      float step, bool born) {
    dusk::interp::lerp(out.position, previous.position, current.position, step);
    out.rotateAngle = dusk::interp::lerp(previous.rotateAngle, current.rotateAngle, step);
    out.texAnm = current.texAnm;
    if (born) {
        out.baseAxis = current.baseAxis;
        out.direction = current.direction;
        out.scaleX = current.scaleX;
        out.scaleY = current.scaleY;
        out.alpha = current.alpha;
        out.age = current.age;
        out.prmClr = current.prmClr;
        out.envClr = current.envClr;
    } else {
        lerp_direction(out.baseAxis, previous.baseAxis, current.baseAxis, step);
        lerp_direction(out.direction, previous.direction, current.direction, step);
        out.scaleX = dusk::interp::lerp(previous.scaleX, current.scaleX, step);
        out.scaleY = dusk::interp::lerp(previous.scaleY, current.scaleY, step);
        out.alpha = dusk::interp::lerp(previous.alpha, current.alpha, step);
        out.age = dusk::interp::lerp(previous.age, current.age, step);
        lerp_color(out.prmClr, previous.prmClr, current.prmClr, step);
        lerp_color(out.envClr, previous.envClr, current.envClr, step);
    }
}

}  // namespace

namespace dusk::interp::particle {

void clear() {
    s_records.clear();
    s_recording = false;
}

void begin_record() {
    const bool held = dScnPly_c::isPause() || dComIfGp_isPauseFlag();
    for (auto it = s_records.begin(); it != s_records.end();) {
        Record& rec = it->second;
        if (!rec.curr_valid) {
            if (held && rec.prev_valid) {
                rec.curr = rec.prev;
                rec.curr_valid = true;
            } else {
                auto dead = it++;
                s_records.erase(dead);
                continue;
            }
        }
        rec.prev = rec.curr;
        rec.prev_valid = true;
        rec.curr_valid = false;
        rec.born = false;
        rec.presented_valid = false;
        ++it;
    }
    s_recording = true;
}

void end_record() {
    s_recording = false;
}

void apply_presentation() {
    for (auto& entry : s_records) {
        entry.second.presented_valid = false;
    }
    if (!is_enabled() || presentation_sync_active()) {
        return;
    }

    const float step = get_interpolation_step();
    for (auto& entry : s_records) {
        Record& rec = entry.second;
        if (rec.prev_valid && rec.curr_valid) {
            interpolate_pose(rec.presented, rec.prev, rec.curr, step, rec.born);
            rec.presented_valid = true;
        }
    }

    for (auto& entry : s_records) {
        Record& child = entry.second;
        if (!child.presented_valid || child.parent == nullptr) {
            continue;
        }
        Record* parent_rec = record_for(child.parent);
        if (parent_rec == nullptr || !parent_rec->presented_valid) {
            continue;
        }
        ParticleSample const& parent_pose = parent_rec->presented;

        if (child.inherit_scale) {
            if (child.parent_sim_scaleX != 0.0f) {
                child.presented.scaleX *= parent_pose.scaleX / child.parent_sim_scaleX;
            }
            if (child.parent_sim_scaleY != 0.0f) {
                child.presented.scaleY *= parent_pose.scaleY / child.parent_sim_scaleY;
            }
        }
        const s16 drot = static_cast<s16>(parent_pose.rotateAngle - child.parent_sim_rotate);
        child.presented.rotateAngle =
            static_cast<u16>(static_cast<s16>(child.presented.rotateAngle + drot));
    }
}

static ParticleSample sample_pose(JPABaseParticle* ptcl, const cXyz& direction) {
    ParticleSample pose;
    pose.position.set(ptcl->mPosition.x, ptcl->mPosition.y, ptcl->mPosition.z);
    pose.baseAxis.set(ptcl->mBaseAxis.x, ptcl->mBaseAxis.y, ptcl->mBaseAxis.z);
    pose.direction = direction;
    pose.scaleX = ptcl->mParticleScaleX;
    pose.scaleY = ptcl->mParticleScaleY;
    pose.rotateAngle = ptcl->mRotateAngle;
    pose.alpha = ptcl->mPrmColorAlphaAnm;
    pose.texAnm = ptcl->mTexAnmIdx;
    pose.age = static_cast<f32>(ptcl->mAge);
    pose.prmClr = ptcl->mPrmClr;
    pose.envClr = ptcl->mEnvClr;
    return pose;
}

void capture(JPABaseParticle* ptcl, JPAEmitterWorkData* work, JPABaseParticle* parent) {
    if (!s_recording || ptcl == nullptr || work == nullptr || !is_enabled()) {
        return;
    }

    Record& rec = s_records[ptcl];

    if (parent != nullptr) {
        rec.parent = parent;
        rec.inherit_scale = false;
        if (work->mpRes != nullptr) {
            JPAChildShape* csp = work->mpRes->getCsp();
            if (csp != nullptr) {
                rec.inherit_scale = csp->isScaleInherited();
            }
        }
        rec.parent_sim_scaleX = parent->mParticleScaleX;
        rec.parent_sim_scaleY = parent->mParticleScaleY;
        rec.parent_sim_rotate = parent->mRotateAngle;
    } else if (rec.parent != nullptr) {
        Record* parent_rec = record_for(rec.parent);
        if (parent_rec != nullptr && parent_rec->curr_valid) {
            rec.parent_sim_scaleX = parent_rec->curr.scaleX;
            rec.parent_sim_scaleY = parent_rec->curr.scaleY;
            rec.parent_sim_rotate = parent_rec->curr.rotateAngle;
        } else if (parent_rec != nullptr && parent_rec->prev_valid) {
            rec.parent_sim_scaleX = parent_rec->prev.scaleX;
            rec.parent_sim_scaleY = parent_rec->prev.scaleY;
            rec.parent_sim_rotate = parent_rec->prev.rotateAngle;
        } else {
            rec.parent = nullptr;
        }
    }

    JGeometry::TVec3<f32> dir;
    JPAGetParticleDir(work, ptcl, &dir);
    cXyz direction(dir.x, dir.y, dir.z);

    if (ptcl->mAge == -1) {
        cXyz spawn(ptcl->mOffsetPosition.x + ptcl->mLocalPosition.x * work->mPublicScale.x,
                   ptcl->mOffsetPosition.y + ptcl->mLocalPosition.y * work->mPublicScale.y,
                   ptcl->mOffsetPosition.z + ptcl->mLocalPosition.z * work->mPublicScale.z);

        ParticleSample pose = sample_pose(ptcl, direction);
        pose.position = spawn;
        rec.prev = pose;
        rec.prev_valid = true;
        rec.curr_valid = false;
        rec.presented_valid = false;
        rec.born = true;
        return;
    }

    rec.curr = sample_pose(ptcl, direction);
    rec.curr_valid = true;
}

JPABaseParticle* present_for_draw(JPABaseParticle* src, JPABaseParticle* scratch) {
    if (presentation_sync_active() || src == nullptr || scratch == nullptr) {
        return src;
    }

    Record* rec = record_for(src);
    if (rec == nullptr || !rec->presented_valid) {
        return src;
    }

    ParticleSample const& pose = rec->presented;
    *scratch = *src;
    scratch->mPosition.set(pose.position.x, pose.position.y, pose.position.z);
    scratch->mBaseAxis.set(pose.baseAxis.x, pose.baseAxis.y, pose.baseAxis.z);
    scratch->mParticleScaleX = pose.scaleX;
    scratch->mParticleScaleY = pose.scaleY;
    scratch->mRotateAngle = pose.rotateAngle;
    scratch->mPrmColorAlphaAnm = pose.alpha;
    scratch->mTexAnmIdx = pose.texAnm;
    scratch->mPrmClr = pose.prmClr;
    scratch->mEnvClr = pose.envClr;
    scratch->mLocalPosition.set(pose.direction.x, pose.direction.y, pose.direction.z);
    scratch->mVelocity.set(pose.direction.x, pose.direction.y, pose.direction.z);
    scratch->mAge = static_cast<s16>(std::lround(pose.age));
    if (src->mLifeTime != 0) {
        scratch->mTime = pose.age / (f32)src->mLifeTime;
    }
    return scratch;
}

}  // namespace dusk::interp::particle
