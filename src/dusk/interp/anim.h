#pragma once

namespace dusk::interp::anim {

struct State {
    float frame;
    float rate;
    float loop;
    float end;
    unsigned attribute;
};

bool try_present(float previous_frame, const State& current, float step, float* out);

}  // namespace dusk::interp::anim
