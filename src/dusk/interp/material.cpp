#include "dusk/interp/material.h"

#include "JSystem/J3DGraphAnimator/J3DAnimation.h"
#include "dusk/game_clock.h"
#include "dusk/interp/anim.h"
#include "dusk/interp/frame_interpolation.h"
#include "m_Do/m_Do_ext.h"

#include <absl/container/flat_hash_map.h>

namespace {

struct Record {
    f32 prev = 0.0f;
    J3DAnmBase* anm = nullptr;
    uint64_t tick = ~uint64_t{0};
    bool lerp = false;
};

absl::flat_hash_map<mDoExt_baseAnm*, Record> s_anims;
uint64_t s_epoch = 0;
uint64_t s_tick = 0;

void sweep() {
    const uint64_t epoch = dusk::game_clock::g_frameTiming.presentationEpoch;
    const uint64_t tick = dusk::interp::sim_tick_seq();
    if (epoch != s_epoch) {
        s_anims.clear();
        s_epoch = epoch;
        s_tick = tick;
        return;
    }
    if (tick == s_tick) {
        return;
    }
    for (auto it = s_anims.begin(); it != s_anims.end();) {
        if (it->second.tick + 1 < tick) {
            auto stale = it++;
            s_anims.erase(stale);
        } else {
            ++it;
        }
    }
    s_tick = tick;
}

bool try_present_record(const Record& rec, mDoExt_baseAnm* ctrl, float* out) {
    J3DFrameCtrl* const frame_ctrl = ctrl->getFrameCtrl();
    return dusk::interp::anim::try_present(rec.prev,
        {
            .frame = frame_ctrl->getFrame(),
            .rate = frame_ctrl->getRate(),
            .loop = static_cast<float>(frame_ctrl->getLoop()),
            .end = static_cast<float>(frame_ctrl->getEnd()),
            .attribute = frame_ctrl->getAttribute(),
        },
        dusk::interp::get_interpolation_step(), out);
}

}  // namespace

namespace dusk::interp::material {

int play(mDoExt_baseAnm* ctrl, J3DAnmBase* anm) {
    if (ctrl == nullptr) {
        return 1;
    }
    if (!interp::is_enabled()) {
        ctrl->getFrameCtrl()->update();
        return ctrl->isStop();
    }
    if (!game_clock::is_sim_frame()) {
        return ctrl->isStop();
    }

    sweep();
    Record& rec = s_anims[ctrl];
    const uint64_t tick = interp::sim_tick_seq();
    if (rec.tick == tick) {
        return ctrl->isStop();
    }

    rec.lerp = rec.anm == nullptr || rec.anm == anm;
    rec.prev = ctrl->getFrame();
    rec.anm = anm;
    rec.tick = tick;
    ctrl->getFrameCtrl()->update();
    return ctrl->isStop();
}

float resolve_entry_frame(mDoExt_baseAnm* ctrl, J3DAnmBase* anm, float requestedFrame) {
    if (ctrl == nullptr) {
        return requestedFrame;
    }

    sweep();
    auto it = s_anims.find(ctrl);
    if (it == s_anims.end()) {
        return requestedFrame;
    }

    Record& rec = it->second;
    if (anm != nullptr) {
        rec.anm = anm;
    }
    if (requestedFrame != ctrl->getFrame()) {
        rec.lerp = false;
        return requestedFrame;
    }
    if (!rec.lerp || !interp::is_enabled() || game_clock::is_sim_frame()) {
        return requestedFrame;
    }

    float frame;
    if (!try_present_record(rec, ctrl, &frame)) {
        return requestedFrame;
    }
    return frame;
}

void apply_presentation_frames() {
    if (!interp::is_enabled() || game_clock::is_sim_frame()) {
        return;
    }

    sweep();
    for (auto& entry : s_anims) {
        mDoExt_baseAnm* ctrl = entry.first;
        Record& rec = entry.second;
        if (ctrl == nullptr || rec.anm == nullptr || !rec.lerp) {
            continue;
        }

        float frame;
        if (!try_present_record(rec, ctrl, &frame)) {
            continue;
        }
        rec.anm->setFrame(frame);
    }
}

}  // namespace dusk::interp::material
