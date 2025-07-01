#include "deejai/common.hpp"
#include "librosa.h"

#include <Eigen/Dense>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <onnxruntime_cxx_api.h>
#include <optional>
#include <ostream>
#include <random>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

namespace deejai::utils {

// The function converts the audio to mono channel
static std::optional<vectorf> load_audio_internal(
    const char *filename,
    int sampling_rate,
    AVFormatContext *fmt_ctx,
    AVCodecContext *codec_ctx,
    SwrContext *swr_ctx,
    AVPacket *pkt,
    AVFrame *frame) {
    av_log_set_level(AV_LOG_ERROR);
    if (avformat_open_input(&fmt_ctx, filename, nullptr, nullptr) < 0) {
        std::cerr << "Could not open input file: " << filename << std::endl;
        return std::nullopt;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        std::cerr << "Could not find stream info: " << filename << std::endl;
        return std::nullopt;
    }

    // Find audio stream
    int audio_stream_index = -1;
    for (unsigned i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
            break;
        }
    }
    if (audio_stream_index == -1) {
        std::cerr << "No audio stream found: " << filename << std::endl;
        return std::nullopt;
    }

    AVCodecParameters *codecpar = fmt_ctx->streams[audio_stream_index]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        std::cerr << "Decoder not found: " << filename << std::endl;
        return std::nullopt;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        std::cerr << "Failed to allocate codec context: " << filename << std::endl;
        return std::nullopt;
    }

    if (avcodec_parameters_to_context(codec_ctx, codecpar) < 0) {
        std::cerr << "Failed to copy codec params: " << filename << std::endl;
        return std::nullopt;
    }

    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        std::cerr << "Failed to open codec: " << filename << std::endl;
        return std::nullopt;
    }

    // Prepare resampler
    swr_ctx = swr_alloc();

    av_opt_set_int(swr_ctx, "in_channel_layout", codec_ctx->ch_layout.u.mask, 0);
    av_opt_set_int(swr_ctx, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate", codec_ctx->sample_rate, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate", sampling_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", codec_ctx->sample_fmt, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_FLT,
                          0); // float output

    if (swr_init(swr_ctx) < 0) {
        std::cerr << "Failed to initialize resampler: " << filename << std::endl;
        return std::nullopt;
    }

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    std::vector<float> samples;

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == audio_stream_index) {
            if (avcodec_send_packet(codec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    // Resample frame
                    int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, codec_ctx->sample_rate) + frame->nb_samples,
                                                        sampling_rate, codec_ctx->sample_rate, AV_ROUND_UP);
                    float **dst_data = nullptr;

                    int ret = av_samples_alloc_array_and_samples((uint8_t ***)&dst_data, nullptr, codec_ctx->ch_layout.nb_channels,
                                                                 dst_nb_samples, AV_SAMPLE_FMT_FLT, 0);
                    if (ret < 0) {
                        std::cerr << "Could not allocate destination samples: " << filename << std::endl;
                        return std::nullopt;
                    }

                    int nb_samples_resampled = swr_convert(swr_ctx, (uint8_t **)dst_data, dst_nb_samples,
                                                           (const uint8_t **)frame->data, frame->nb_samples);

                    if (nb_samples_resampled < 0) {
                        std::cerr << "Error during resampling: " << filename << std::endl;
                        av_freep(&dst_data[0]);
                        av_freep(&dst_data);
                        return std::nullopt;
                    }

                    int total_samples = nb_samples_resampled;
                    for (int i = 0; i < total_samples; i++) {
                        samples.push_back(dst_data[0][i]);
                    }

                    av_freep(&dst_data[0]);
                    av_freep(&dst_data);
                }
            }
        }
        av_packet_unref(pkt);
    }

    // Convert samples vector to Eigen vector
    vectorf eigen_samples = Eigen::Map<vectorf>(samples.data(), 1, samples.size());
    return eigen_samples;
}

std::optional<vectorf> load_audio(const char *filename, int sampling_rate) {
    AVFormatContext *fmt_ctx = nullptr;
    AVCodecContext *codec_ctx = nullptr;
    SwrContext *swr_ctx = nullptr;
    AVPacket *pkt = nullptr;
    AVFrame *frame = nullptr;
    auto ret = load_audio_internal(filename, sampling_rate, fmt_ctx, codec_ctx, swr_ctx, pkt, frame);
    if (frame) {
        av_frame_free(&frame);
    }
    if (pkt) {
        av_packet_free(&pkt);
    }
    if (swr_ctx) {
        swr_free(&swr_ctx);
    }
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }
    return ret;
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
