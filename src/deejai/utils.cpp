#include "deejai/utils.hpp"
#include "deejai/common.hpp"

#include <Eigen/Dense>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <onnxruntime_cxx_api.h>
#include <optional>
#include <ostream>
#include <random>
#include <regex>
#include <string>
#include <vector>

namespace deejai::utils {

static std::string espace_string_for_ffmpeg(const std::string &str) {
    std::string espaced = str;
#ifdef _WIN32
#else
    std::regex dollar("\\$");
    espaced = std::regex_replace(espaced, dollar, "\\$");
#endif // _WIN32
    std::regex quote("`");
    espaced = std::regex_replace(espaced, quote, "\\`");
    return espaced;
}

// The function loads the audio to mono channel
std::optional<vectorf> load_audio(const char *filename, int sampling_rate) {
    std::vector<int16_t> samples;
    std::string input_filename = espace_string_for_ffmpeg(filename);

#ifdef _WIN32
    std::string cmd = deejai::utils::FFMPEG_PATH + " -i \"" + std::string(filename) +
                      "\" -f s16le -acodec pcm_s16le -ac 1 -ar " +
                      std::to_string(sampling_rate) + " - 2>nul";
    FILE *pipe = _popen(cmd.c_str(), "rb");
#else
    std::string cmd = deejai::utils::FFMPEG_PATH + " -i \"" + input_filename +
                      "\" -f s16le -acodec pcm_s16le -ac 1 -ar " +
                      std::to_string(sampling_rate) + " - 2>/dev/null";
    FILE *pipe = popen(cmd.c_str(), "r");
#endif // _WIN32

    if (!pipe)
        throw std::runtime_error("Failed to open pipe to FFmpeg");

    int16_t buffer[4096];
    const int max_sample_size = 12 * 60 * sampling_rate;
    bool should_skip = false;
    while (fread(buffer, sizeof(int16_t), 4096, pipe) > 0) {
        samples.insert(samples.end(), buffer, buffer + 4096);
        // skip audio files longer than 12 min to avoid running out of memory
        if (samples.size() > max_sample_size) {
            should_skip = true;
            break;
        }
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif // _WIN32

    if (should_skip) {
        return std::nullopt;
    }

    if (samples.empty()) {
        std::cerr << "Couldn't load the audio file: " << filename << std::endl;
        std::cerr << "Make sure that FFmpeg is installed and that the provided path points to an audio file." << std::endl;
        return std::nullopt;
    }

    deejai::vectorf vec(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        vec[i] = samples[i] / 32768.0f;
    }

    return vec;
}

std::vector<std::filesystem::path> find_audio_files_recursively(const std::vector<std::string> &paths) {
    auto hasAudioExtension = [](std::string filename) {
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        return filename.ends_with(".mp3") || filename.ends_with(".flac") || filename.ends_with(".m4a");
    };

    std::vector<std::filesystem::path> results;

    for (const auto &pathStr : paths) {
        std::filesystem::path path(pathStr);

        if (std::filesystem::is_regular_file(path)) {
            if (hasAudioExtension(path.string())) {
                results.emplace_back(std::filesystem::absolute(path));
            }
        }

        if (std::filesystem::is_directory(path)) {
            for (auto it = std::filesystem::recursive_directory_iterator(path, std::filesystem::directory_options::skip_permission_denied);
                 it != std::filesystem::recursive_directory_iterator(); it++) {
                if (!std::filesystem::is_regular_file(*it)) {
                    continue;
                }
                if (hasAudioExtension(it->path().string())) {
                    results.emplace_back(std::filesystem::absolute(it->path()));
                }
            }
        }
    }

    return results;
}

// Helper to get the length of a UTF-8 character from its first byte
static size_t utf8_char_length(unsigned char c) {
    if ((c & 0x80) == 0x00)
        return 1; // 0xxxxxxx
    if ((c & 0xE0) == 0xC0)
        return 2; // 110xxxxx
    if ((c & 0xF0) == 0xE0)
        return 3; // 1110xxxx
    if ((c & 0xF8) == 0xF0)
        return 4; // 11110xxx
    return 1;     // Invalid, treat as 1 to prevent infinite loops
}

static bool is_valid_leading_byte(unsigned char c) {
    return (c & 0xC0) != 0x80;
}

static std::string truncate_utf8(const std::string &input, size_t max_bytes) {
    if (input.size() <= max_bytes)
        return input;

    size_t i = input.size();
    size_t total_bytes = 0;

    // We'll collect characters backward
    while (i > 0) {
        // Find the start of the previous UTF-8 character
        size_t char_start = i - 1;
        while (char_start > 0 && !is_valid_leading_byte(static_cast<unsigned char>(input[char_start]))) {
            --char_start;
        }

        size_t char_len = utf8_char_length(static_cast<unsigned char>(input[char_start]));
        if (char_start + char_len != i) {
            // malformed character, skip
            --i;
            continue;
        }

        if (total_bytes + char_len > max_bytes)
            break;

        total_bytes += char_len;
        i = char_start;
    }

    return input.substr(i);
}

std::string scanned_filename(const std::string &path) {
    std::string scanned_name = path + ".bin";
    std::replace(scanned_name.begin(), scanned_name.end(), '/', '_');
    std::replace(scanned_name.begin(), scanned_name.end(), '\\', '_');
    std::replace(scanned_name.begin(), scanned_name.end(), ':', '_');
    std::replace(scanned_name.begin(), scanned_name.end(), '?', '_');

    // account for the max filename length (255 bytes is a safe length for most OS)
    // this can be error prone, might need to add a hash in the filename to avoid conflicts
    scanned_name = truncate_utf8(scanned_name, 255);

    return scanned_name;
}

std::vector<int> random_permutation(int n) {
    std::vector<int> indices(n);
    for (int i = 0; i < n; i++) {
        indices[i] = i;
    }

    std::mt19937 random_engine(std::random_device{}());
    std::shuffle(indices.begin(), indices.end(), random_engine);
    return indices;
}

matrixf ort_to_matrix(Ort::Value &value) {
    if (!value.IsTensor()) {
        throw std::invalid_argument("Ort::Value is not a tensor.");
    }

    Ort::TensorTypeAndShapeInfo tensor_info = value.GetTensorTypeAndShapeInfo();
    auto shape = tensor_info.GetShape();
    if (shape.size() != 2) {
        throw std::invalid_argument("Tensor is not 2D.");
    }

    int rows = static_cast<int>(shape[0]);
    int cols = static_cast<int>(shape[1]);
    float *data = value.GetTensorMutableData<float>();
    Eigen::Map<matrixf> eigen_matrix(data, rows, cols);
    return eigen_matrix;
}

void save_matrix_to_stream(std::ofstream &ofs, const matrixf &matrix) {
    int rows = matrix.rows();
    int cols = matrix.cols();

    ofs.write(reinterpret_cast<char *>(&rows), sizeof(int));
    ofs.write(reinterpret_cast<char *>(&cols), sizeof(int));
    ofs.write(reinterpret_cast<const char *>(matrix.data()), sizeof(float) * rows * cols);
}

matrixf load_matrix_from_stream(std::ifstream &ifs) {
    int rows;
    int cols;
    ifs.read(reinterpret_cast<char *>(&rows), sizeof(int));
    ifs.read(reinterpret_cast<char *>(&cols), sizeof(int));

    matrixf mat(rows, cols);
    ifs.read(reinterpret_cast<char *>(mat.data()), sizeof(float) * rows * cols);
    return mat;
}

bool save_matrix_map(const std::unordered_map<std::string, matrixf> &matrix_map, const std::string &filename) {
    std::ofstream ofs(filename, std::ios::binary);

    if (!ofs) {
        std::cerr << "Failed to open file for writing " << filename << std::endl;
        return false;
    }
    uint32_t map_size = static_cast<uint32_t>(matrix_map.size());
    ofs.write(reinterpret_cast<const char *>(&map_size), sizeof(map_size));

    for (const auto &[audio_path, matrix] : matrix_map) {
        uint32_t path_len = static_cast<uint32_t>(audio_path.size());
        ofs.write(reinterpret_cast<const char *>(&path_len), sizeof(path_len));
        ofs.write(audio_path.data(), path_len);

        save_matrix_to_stream(ofs, matrix);
    }
    return true;
}

std::unordered_map<std::string, matrixf> load_matrix_map(const std::string &filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) {
        std::cerr << "Failed to open file for reading " << filename << std::endl;
        return {};
    }
    std::unordered_map<std::string, matrixf> matrix_map;

    // Read number of map entries
    uint32_t map_size = 0;
    ifs.read(reinterpret_cast<char *>(&map_size), sizeof(map_size));

    for (uint32_t i = 0; i < map_size; i++) {
        uint32_t key_len = 0;
        ifs.read(reinterpret_cast<char *>(&key_len), sizeof(key_len));
        std::string key(key_len, '\0');
        ifs.read(key.data(), key_len);

        matrixf matrix = load_matrix_from_stream(ifs);
        matrix_map.emplace(std::move(key), std::move(matrix));
    }

    return matrix_map;
}

std::unordered_map<std::string, vectorf> matrix_to_vector(const std::unordered_map<std::string, matrixf> &matrix_map) {
    std::unordered_map<std::string, vectorf> vector_map;
    for (const auto &[key, mat] : matrix_map) {
        vectorf vec(Eigen::Map<const vectorf>(mat.data(), mat.size())); // Flatten matrix
        vector_map[key] = vec;
    }
    return vector_map;
}

void add_noise(vectorf &vec, float noise) {
    std::mt19937 random_engine(std::random_device{}());
    if (noise > 0.0f) {
        std::normal_distribution<float> distribution(0.0f, noise * vec.norm());
        for (int i = 0; i < vec.size(); i++) {
            vec[i] += distribution(random_engine);
        }
    }
}

bool save_as_m3u(const std::string &filename, const std::vector<std::string> &paths) {
    std::string name = filename;
    if (!name.ends_with(".m3u")) {
        name = filename + ".m3u";
    }
    std::ofstream file(name);
    if (!file.is_open()) {
        std::cerr << "Error opening file for writing: " << name << "\n";
        return false;
    }

    file << "#EXTM3U\n";
    for (const auto &path : paths) {
        file << path << "\n";
    }

    file.close();
    return true;
}
} // namespace deejai::utils
