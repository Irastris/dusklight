#pragma once

class JPABaseParticle;
struct JPAEmitterWorkData;

namespace dusk::interp::particle {

void clear();
void begin_record();
void end_record();
void apply_presentation();
void capture(JPABaseParticle* ptcl, JPAEmitterWorkData* work, JPABaseParticle* parent = nullptr);
JPABaseParticle* present_for_draw(JPABaseParticle* src, JPABaseParticle* scratch);

}  // namespace dusk::interp::particle
