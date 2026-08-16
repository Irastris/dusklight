#include "dusk/interp/pane.h"

#include "d/d_pane_class_alpha.h"
#include "dusk/interp/vdt.h"

namespace dusk::interp::pane {
namespace {

struct PresentAlpha {
    u8 start;
    u8 end;
    u8 calc;
};

struct PresentRate {
    f32 full;
    bool fade_in;
};

vdt::Table<CPaneMgrAlpha, PresentAlpha> s_alphas{};
vdt::Table<CPaneMgrAlpha, PresentRate, vdt::kRateCapacity> s_rates{};

f32& alpha_timer(CPaneMgrAlpha* pane) {
    return pane->mAlphaTimer;
}

void apply_alpha(CPaneMgrAlpha* pane, const PresentAlpha& payload, f32 rate) {
    pane->setAlpha(payload.start + rate * (f32)(payload.end - payload.start));
}

void apply_rate(CPaneMgrAlpha* pane, f32 full, s16 duration) {
    pane->setAlphaRate(full * vdt::clamped_frac(pane->mAlphaTimer, duration));
}

void step_rate(vdt::Entry<CPaneMgrAlpha, PresentRate>& entry) {
    CPaneMgrAlpha* pane = entry.owner;
    bool done = vdt::step_rate_timer(pane->mAlphaTimer, entry.duration, entry.payload.fade_in);
    apply_rate(pane, entry.payload.full, entry.duration);
    if (done) {
        entry = {};
    }
}

void advance_pane_alpha() {
    s_alphas.drain(&alpha_timer, &apply_alpha);
    s_rates.each(&step_rate);
}

bool request_rate(CPaneMgrAlpha* pane, s16 duration, f32 full, bool fade_in) {
    vdt::register_advance(&advance_pane_alpha);
    s_alphas.unlink(pane);
    vdt::Entry<CPaneMgrAlpha, PresentRate>* entry = s_rates.acquire(pane);
    if (entry == nullptr) {
        return false;
    }
    entry->duration = duration;
    entry->payload = PresentRate{full, fade_in};
    return true;
}

}  // namespace

void cancel_alpha(CPaneMgrAlpha* pane) {
    s_alphas.unlink(pane);
    s_rates.unlink(pane);
}

bool request_alpha(CPaneMgrAlpha* pane, s16 timer, u8 start, u8 end, u8 calc) {
    vdt::register_advance(&advance_pane_alpha);
    s_rates.unlink(pane);
    return s_alphas.request(pane, timer, PresentAlpha{start, end, calc});
}

void present_alpha_rate_min(CPaneMgrAlpha* pane, f32 full, s16 duration) {
    if (pane->mAlphaTimer > duration) {
        pane->mAlphaTimer = duration;
    }

    if (pane->mAlphaTimer <= 0.0f || duration <= 0 || !request_rate(pane, duration, full, false)) {
        s_rates.unlink(pane);
        pane->mAlphaTimer = 0.0f;
        if (pane->getAlphaRate() != 0.0f) {
            pane->setAlphaRate(0.0f);
        }
        return;
    }

    apply_rate(pane, full, duration);
    if (pane->mAlphaTimer - 1.0f <= 0.0f && pane->getAlphaRate() != 0.0f) {
        pane->setAlphaRate(0.0f);
    }
}

void present_alpha_rate_max(CPaneMgrAlpha* pane, f32 full, s16 duration) {
    if (pane->mAlphaTimer >= duration || duration <= 0 || !request_rate(pane, duration, full, true)) {
        s_rates.unlink(pane);
        if (duration > 0) {
            pane->mAlphaTimer = duration;
        }
        if (pane->getAlphaRate() != full) {
            pane->setAlphaRate(full);
        }
        return;
    }

    apply_rate(pane, full, duration);
    if (pane->mAlphaTimer + 1.0f >= (f32)duration && pane->getAlphaRate() != full) {
        pane->setAlphaRate(full);
    }
}

}  // namespace dusk::interp::pane
