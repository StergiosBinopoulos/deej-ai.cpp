#pragma once

#include <Eigen/Core>
#include <string_view>

namespace deejai {

typedef Eigen::Matrix<float, 1, Eigen::Dynamic, Eigen::RowMajor> vectorf;
typedef Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> matrixf;

constexpr std::string_view BUNDLED_VECS_DIRNAME = "bundled";
constexpr std::string_view BUNDLED_VECS_FILENAME = "audio_vecs.bin";

} // namespace deejai
