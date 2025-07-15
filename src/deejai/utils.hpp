#pragma once

#include "deejai/common.hpp"

#include <filesystem>
#include <onnxruntime_cxx_api.h>
#include <optional>
#include <vector>

namespace deejai::utils {

inline std::string FFMPEG_PATH = "ffmpeg";

std::optional<vectorf> load_audio(const char *filename, int sampling_rate);
std::vector<std::filesystem::path> find_audio_files_recursively(const std::vector<std::string> &paths);
std::string scanned_filename(const std::string &path);
std::vector<int> random_permutation(int n);
matrixf ort_to_matrix(Ort::Value &value);
void save_matrix_to_stream(std::ofstream &ofs, const matrixf &matrix);
matrixf load_matrix_from_stream(std::ifstream &ifs);
bool save_matrix_map(const std::unordered_map<std::string, matrixf> &tensor_map, const std::string &filename);
std::unordered_map<std::string, matrixf> load_matrix_map(const std::string &filename);
std::unordered_map<std::string, vectorf> matrix_to_vector(const std::unordered_map<std::string, matrixf> &matrix_map);
void add_noise(vectorf &vec, float noise);
bool save_as_m3u(const std::string &filename, const std::vector<std::string> &paths);

} // namespace deejai::utils
