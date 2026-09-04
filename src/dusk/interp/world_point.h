#pragma once

#include "SSystem/SComponent/c_xyz.h"

#ifdef __cplusplus
namespace dusk::interp {

void capture_world_point(const void* key, const cXyz& outgoing);
void invalidate_world_point(const void* key);
void erase_world_point(const void* key);
bool present_world_point(const void* key, const cXyz& current, Vec* out_screen);

}  // namespace dusk::interp
#endif
