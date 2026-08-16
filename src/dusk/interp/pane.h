#pragma once

#include "dolphin/types.h"

class CPaneMgrAlpha;

namespace dusk::interp::pane {

void cancel_alpha(CPaneMgrAlpha* pane);
bool request_alpha(CPaneMgrAlpha* pane, s16 timer, u8 start, u8 end, u8 calc);
void present_alpha_rate_min(CPaneMgrAlpha* pane, f32 full, s16 duration);
void present_alpha_rate_max(CPaneMgrAlpha* pane, f32 full, s16 duration);

}  // namespace dusk::interp::pane
