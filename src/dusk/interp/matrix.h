#pragma once

#include "mtx.h"

namespace dusk::interp::matrix {

enum class DecompositionStatus {
    NotAttempted,
    Failed,
    Valid,
};

struct DecomposedMatrix {
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
    Vec scale{1.0f, 1.0f, 1.0f};
    Vec skew{};
    Vec translation{};
    bool coordinateFlip = false;
    DecompositionStatus status = DecompositionStatus::NotAttempted;
};

struct MatrixSample {
    Mtx value{};
    DecomposedMatrix decomposed;
};

void record(MatrixSample* sample, const Mtx value);
void interpolate(Mtx out, MatrixSample& previous, MatrixSample& current, float step);

}  // namespace dusk::interp::matrix
