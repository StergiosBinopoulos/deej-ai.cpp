#pragma once

#include "deejai/common.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace deejai {

class generator {
  public:
    explicit generator(const std::string &vecs_dir);
    ~generator() = default;
    generator(const generator &other) = default;
    generator &operator=(const generator &) = default;
    generator(generator &&) = default;
    generator &operator=(generator &&) = default;

    std::vector<std::string> generate_playlist(
        const std::string &method,
        std::vector<std::string> seed_tracks,
        int nsongs = 10,
        int lookback = 3,
        float noise = 0.0f) const;

    std::vector<std::pair<std::string, float>> most_similar(
        const std::unordered_set<std::string> &excluded,
        const vectorf &vec_sum,
        int topn = 5) const;

  private:
    bool remove_invalid_tracks(std::vector<std::string> &tracks) const;
    std::vector<std::string> generate_playlist_connect(
        const std::vector<std::string> &seed_tracks,
        int nsongs = 10,
        float noise = 0.0f) const;
    vectorf calculate_vector(const std::vector<std::string> &tracks, float noise) const;

    std::unordered_map<std::string, vectorf> m_audio_vec;
};

} // namespace deejai
