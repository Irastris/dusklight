#pragma once

class J3DAnmBase;
class mDoExt_baseAnm;

namespace dusk::interp::material {

int play(mDoExt_baseAnm* ctrl, J3DAnmBase* anm);
float resolve_entry_frame(mDoExt_baseAnm* ctrl, J3DAnmBase* anm, float requestedFrame);
void apply_presentation_frames();

}  // namespace dusk::interp::material
