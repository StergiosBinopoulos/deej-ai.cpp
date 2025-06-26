#pragma once

#include "deejai/common.hpp"

#include <onnxruntime_cxx_api.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace deejai {

struct audio_file_tensor {
    std::vector<float> buffer;
    Ort::Value tensor;
    std::string audio_path;
};

class scanner {
  public:
    scanner(const std::string &model_path, const std::string &save_directory);
    ~scanner() = default;
    scanner(const scanner &other) = delete;
    scanner &operator=(const scanner &) = delete;
    scanner(scanner &&) = default;
    scanner &operator=(scanner &&) = default;

    bool scan(const std::vector<std::string> &paths);
    std::vector<Ort::Value> predict(const audio_file_tensor &input_tensor);

    std::vector<int64_t> input_shape() const;
    audio_file_tensor tensor_from_audio(const std::string &audio_path) const;

    void set_batch_size(int batch_size);
    int batch_size() const;
    void set_epsilon(double epsilon);
    double epsilon() const;

  private:
    void scan_file(const std::string &path);
    static bool is_batch_file(const std::string &path);
    void clean_deleted_items(std::unordered_map<std::string, matrixf> &audio_vecs) const;

    Ort::Env m_env;
    Ort::SessionOptions m_session_options;
    Ort::Session m_session;
    Ort::AllocatorWithDefaultOptions m_allocator;

    std::string m_save_directory;

    int m_batch_size = 100;
    double m_epsilon_distance = 0.001;
};

} // namespace deejai
