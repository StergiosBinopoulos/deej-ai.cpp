#include "deejai/scanner.hpp"
#include "deejai/utils.hpp"
#include "librosa.h"

#include <Eigen/Dense>
#include <filesystem>
#include <mutex>
#include <onnxruntime_cxx_api.h>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <unsupported/Eigen/CXX11/Tensor>
#include <vector>

#ifdef _WIN32
static std::wstring str_to_wstr(const std::string &str) {
    std::wstring wstr(str.begin(), str.end());
    return wstr;
}
#endif // _WIN32

namespace deejai {

scanner::scanner(const std::string &model_path, const std::string &save_directory) :
    m_env(ORT_LOGGING_LEVEL_WARNING, "ONNXModel"),
#ifdef _WIN32
    m_session(m_env, str_to_wstr(model_path).c_str(), session_options())
#else
    m_session(m_env, model_path.c_str(), session_options())
#endif // _WIN32
{
    m_save_directory = std::u8string(save_directory.begin(), save_directory.end());
}

Ort::SessionOptions scanner::session_options() {
    Ort::SessionOptions options;
    options.DisableCpuMemArena();
    options.DisableMemPattern();
    return options;
}

std::vector<int64_t> scanner::input_shape() const {
    Ort::TypeInfo type_info = m_session.GetInputTypeInfo(0);
    auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
    std::vector<int64_t> input_shape = tensor_info.GetShape();
    return input_shape;
}

std::vector<Ort::Value> scanner::predict(const audio_file_tensor &input_tensor) {
    Ort::AllocatorWithDefaultOptions allocator;
    std::vector<Ort::AllocatedStringPtr> input_name_ptrs;
    std::vector<const char *> input_names;
    size_t num_inputs = m_session.GetInputCount();
    for (size_t i = 0; i < num_inputs; i++) {
        input_name_ptrs.emplace_back(m_session.GetInputNameAllocated(i, allocator));
        input_names.push_back(input_name_ptrs.back().get());
    }

    std::vector<Ort::AllocatedStringPtr> output_name_ptrs;
    std::vector<const char *> output_names;
    size_t num_outputs = m_session.GetOutputCount();
    for (size_t i = 0; i < num_outputs; i++) {
        output_name_ptrs.emplace_back(m_session.GetOutputNameAllocated(i, allocator));
        output_names.push_back(output_name_ptrs.back().get());
    }

    std::vector<Ort::Value> output_tensors =
        m_session.Run(Ort::RunOptions{nullptr}, input_names.data(), &input_tensor.tensor, 1,
                      output_names.data(), output_names.size());
    return output_tensors;
}

bool scanner::is_batch_file(const std::string &path) {
    return path.starts_with("batch_") && path.ends_with(".bin");
}

std::optional<audio_file_tensor> scanner::tensor_from_audio(const std::string &audio_path) const {
    const int sampling_rate = 22050;
    const int n_fft = 2048;
    const int hop_length = 512;
    const auto shape = input_shape();
    const int n_mels = shape[2];
    const int slice_size = shape[3];

    auto vec = utils::load_audio(audio_path, sampling_rate);
    if (!vec.has_value()) {
        return std::nullopt;
    }

    auto vector = vec.value();
    if (vector.size() < slice_size) {
        return std::nullopt;
    }

    matrixf S = librosa::internal::melspectrogram(vector, sampling_rate, n_fft, hop_length, "hann", true,
                                                  "constant", 2, n_mels, 0, sampling_rate / 2);
    int batch = S.cols() / slice_size;
    Eigen::Tensor<float, 4> x(batch, 1, n_mels, slice_size);
    for (int slice = 0; slice < batch; slice++) {
        matrixf submatrix = S.block(0, slice * slice_size, S.rows(), slice_size);
        matrixf log_S = librosa::internal::power2db(submatrix);

        float max_val = log_S.maxCoeff();
        float min_val = log_S.minCoeff();
        float denom = max_val - min_val;

        if (denom != 0) {
            log_S = (log_S.array() - min_val) / denom;
        }

        for (int xi = 0; xi < n_mels; xi++) {
            for (int yi = 0; yi < slice_size; yi++) {
                x(slice, 0, xi, yi) = log_S(xi, yi);
            }
        }
    }

    // Flatten the Eigen tensor to a 1D std::vector
    std::vector<float> input_values(x.size());
    std::copy(x.data(), x.data() + x.size(), input_values.begin());
    // Define the shape (same order as in Eigen: [N, C, H, W])
    std::vector<int64_t> input_shape = {
        static_cast<int64_t>(x.dimension(0)), static_cast<int64_t>(x.dimension(1)),
        static_cast<int64_t>(x.dimension(2)), static_cast<int64_t>(x.dimension(3))};

    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(memory_info, input_values.data(), input_values.size(),
                                                              input_shape.data(), input_shape.size());

    audio_file_tensor tensor;
    tensor.buffer = std::move(input_values);
    tensor.tensor = std::move(input_tensor);
    tensor.audio_path = audio_path;
    return tensor;
}

void scanner::set_batch_size(int batch_size) {
    m_batch_size = batch_size;
}

int scanner::batch_size() const {
    return m_batch_size;
}

void scanner::set_epsilon(double epsilon) {
    m_epsilon_distance = epsilon;
}

double scanner::epsilon() const {
    return m_epsilon_distance;
}

bool scanner::scan(const std::vector<std::string> &paths, int jobs) {
    const std::filesystem::path bundled_dir = std::filesystem::path(m_save_directory) / BUNDLED_VECS_DIRNAME;
    const std::filesystem::path bundled_vecs_path = bundled_dir / BUNDLED_VECS_FILENAME;

    auto files = utils::find_audio_files_recursively(paths);
    std::mt19937 random_engine(std::random_device{}());
    std::shuffle(files.begin(), files.end(), random_engine);

    if (!std::filesystem::exists(m_save_directory)) {
        if (!std::filesystem::create_directory(m_save_directory)) {
            std::cerr << "Failed to create the scan directory" << std::endl;
            return false;
        }
    }

    if (!std::filesystem::exists(bundled_dir)) {
        if (!std::filesystem::create_directory(bundled_dir)) {
            std::cerr << "Failed to create the bundle directory" << std::endl;
            return false;
        }
    }

    const int total_files = files.size();
    std::mutex scan_mutex;
    std::atomic<int> current{0};
    auto scan_worker_fun = [&](const std::string &file) {
        int value = current.fetch_add(1, std::memory_order_relaxed) + 1;
        std::u8string u8(file.begin(), file.end());
        std::u8string scanned_filename = utils::scanned_filename(u8);
        std::filesystem::path vec_file = std::filesystem::path(m_save_directory) / std::filesystem::path(scanned_filename);
        if (!(std::filesystem::exists(vec_file) && std::filesystem::is_regular_file(vec_file))) {
            scan_file(file);
            {
                std::lock_guard<std::mutex> lock(scan_mutex);
                if (value % 10 == 0) {
                    std::cout << "Scan progress: " << value << " / " << total_files << std::endl;
                }
            }
        }
    };

    size_t max_concurrent = std::thread::hardware_concurrency();
    if (max_concurrent <= 0) {
        max_concurrent = 1;
    }

    if (jobs != -1 && jobs > 0) {
        max_concurrent = std::min(max_concurrent, static_cast<size_t>(jobs));
    }

    std::queue<std::thread> threads;
    for (const auto &file : files) {
        if (threads.size() >= max_concurrent) {
            threads.front().join();
            threads.pop();
        }

        threads.emplace(scan_worker_fun, file);
    }

    // Join remaining threads
    while (!threads.empty()) {
        threads.front().join();
        threads.pop();
    }

    // load individual file vectors
    std::unordered_map<std::string, matrixf> loaded_individual_vecs;
    for (const auto &entry : std::filesystem::directory_iterator(m_save_directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".bin") {
            auto matrix_map = utils::load_matrix_map(entry.path());
            for (const auto &[audio_path, matrix] : matrix_map) {
                loaded_individual_vecs.emplace(audio_path, matrix);
            }
        }
    }

    // load bundled vectors
    std::unordered_map<std::string, matrixf> loaded_bundled_vecs;
    if (std::filesystem::is_regular_file(bundled_vecs_path)) {
        loaded_bundled_vecs = utils::load_matrix_map(bundled_vecs_path);
    }
    // append vectors from batches
    int start_batch = 1;
    if (std::filesystem::is_directory(bundled_dir)) {
        for (const auto &entry : std::filesystem::directory_iterator(bundled_dir)) {
            const std::u8string u8filename = entry.path().u8string();
            const std::string filename = std::string(u8filename.begin(), u8filename.end());
            if (entry.is_regular_file() && is_batch_file(filename)) {
                start_batch += 1;
                auto vecs_batch = utils::load_matrix_map(filename);
                loaded_bundled_vecs.merge(vecs_batch);
            }
        }
    }
    // delete the vectors of removed files
    clean_deleted_items(loaded_bundled_vecs);

    std::vector<std::string> remainings_vecs;
    for (const auto &[audio_path, matrix] : loaded_individual_vecs) {
        if (!loaded_bundled_vecs.contains(audio_path)) {
            remainings_vecs.push_back(audio_path);
        }
    }
    int num_audio = remainings_vecs.size();

    std::vector<int> batch_indices = utils::random_permutation(num_audio);
    int num_batches = num_audio / m_batch_size + 1;
    for (int batch = 0; batch < num_batches; batch++) {
        std::vector<std::string> audio_keys;
        for (int i = 0; i < m_batch_size; i++) {
            int idx = batch * m_batch_size + i;
            if (static_cast<size_t>(idx) >= batch_indices.size()) {
                break;
            }

            const std::string key = remainings_vecs[batch_indices[idx]];
            audio_keys.push_back(key);
        }
        std::vector<vectorf> audio_vecs;
        std::unordered_map<std::string, std::vector<int>> audio_indices;
        for (const auto &key : audio_keys) {
            audio_indices[key] = std::vector<int>();
            const matrixf matrix = loaded_individual_vecs[key];
            for (int i = 0; i < matrix.rows(); i++) {
                vectorf row = matrix.row(i);
                audio_indices[key].push_back(audio_vecs.size());
                audio_vecs.emplace_back(row / row.norm());
            }
        }

        // cosine distances
        int num_audio_vecs = audio_vecs.size();
        matrixf cos_distances = matrixf::Zero(num_audio_vecs, num_audio_vecs);
        for (int i = 0; i != num_audio_vecs; i++) {
            for (int j = i + 1; j != num_audio_vecs; j++) {
                float dot = audio_vecs[i].dot(audio_vecs[j]);
                cos_distances(i, j) = 1.0f - dot;
            }
        }
        cos_distances = cos_distances.selfadjointView<Eigen::Upper>();

        // IDF weights
        std::vector<float> idfs;
        for (int i = 0; i != num_audio_vecs; i++) {
            int idf_count = 0;
            for (const auto &key : audio_keys) {
                const auto &indices = audio_indices.at(key);
                for (int j : indices) {
                    if (cos_distances(i, j) < m_epsilon_distance) {
                        idf_count++;
                        break;
                    }
                }
            }
            float ratio = static_cast<float>(idf_count) / static_cast<float>(audio_keys.size());
            idfs.push_back(-std::log(ratio));
        }

        // TF weights
        std::unordered_map<std::string, matrixf> batch_vec;
        for (const auto &key : audio_keys) {
            vectorf vec = vectorf::Zero(loaded_individual_vecs[key].cols());
            const auto &indices = audio_indices.at(key);
            for (int i : indices) {
                int tf = 0;
                for (int j : indices) {
                    if (cos_distances(i, j) < m_epsilon_distance) {
                        tf++;
                    }
                }
                vec += audio_vecs.at(i) * (tf * idfs[i]);
            }
            batch_vec[key] = vec;
            loaded_bundled_vecs[key] = vec;
        }

        const std::string batch_filename = std::string("batch_") + std::to_string(start_batch + batch) + ".bin";
        const std::filesystem::path batch_path = bundled_dir / batch_filename;
        utils::save_matrix_map(batch_vec, batch_path);
    }

    const bool save_status = utils::save_matrix_map(loaded_bundled_vecs, bundled_vecs_path);
    for (const auto &entry : std::filesystem::directory_iterator(bundled_dir)) {
        if (entry.is_regular_file()) {
            const std::u8string u8filename = entry.path().filename().u8string();
            const std::string filename = std::string(u8filename.begin(), u8filename.end());
            if (is_batch_file(filename)) {
                std::filesystem::remove(entry.path());
            }
        }
    }
    return save_status;
}

void scanner::scan_file(const std::string &path) {
    const auto tensor = tensor_from_audio(path);
    if (!tensor.has_value()) {
        return;
    }
    std::vector<Ort::Value> prediction = predict(*tensor);

    if (!prediction.empty()) {
        std::u8string u8path = std::u8string(path.begin(), path.end());
        const std::u8string filename = utils::scanned_filename(u8path);
        const std::filesystem::path save_path = std::filesystem::path(m_save_directory) / filename;

        matrixf matrix = utils::ort_to_matrix(prediction[0]);
        std::unordered_map<std::string, matrixf> map = {{tensor->audio_path, matrix}};
        utils::save_matrix_map(map, save_path);
    }
}

void scanner::clean_deleted_items(std::unordered_map<std::string, matrixf> &audio_vecs) const {
    for (auto it = audio_vecs.begin(); it != audio_vecs.end();) {
        const std::string &key = it->first;
        std::u8string u8 = std::u8string(key.begin(), key.end());
        if (!std::filesystem::is_regular_file(u8)) {
            const auto path = std::filesystem::path(m_save_directory) / utils::scanned_filename(u8);
            if (std::filesystem::exists(path)) {
                std::filesystem::remove(path);
            }
            it = audio_vecs.erase(it);
        } else {
            it++;
        }
    }
}
} // namespace deejai
