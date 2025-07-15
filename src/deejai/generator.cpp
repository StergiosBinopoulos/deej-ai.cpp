#include "deejai/generator.hpp"
#include "deejai/common.hpp"
#include "deejai/utils.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iostream>
#include <random>
#include <vector>

namespace deejai {

generator::generator(const std::string &vecs_dir) {
    const std::filesystem::path path = std::filesystem::path(vecs_dir) / BUNDLED_VECS_DIRNAME / BUNDLED_VECS_FILENAME;
    auto temp_map = utils::load_matrix_map(path.string());
    m_audio_vec = utils::matrix_to_vector(temp_map);
}

std::vector<std::string> generator::generate_playlist(const std::string &method,
                                                      std::vector<std::string> seed_tracks,
                                                      int nsongs, int lookback, float noise) const {
    remove_invalid_tracks(seed_tracks);
    if (seed_tracks.empty()) {
        return {};
    }

    if (method == "connect") {
        if (seed_tracks.size() < 2) {
            return generate_playlist("append", seed_tracks, nsongs, lookback, noise);
        }

        return generate_playlist_connect(seed_tracks, nsongs, noise);
    }

    vectorf vec_sum;
    if (method == "cluster") {
        vec_sum = calculate_vector(seed_tracks, noise);
    }

    std::vector<std::string> playlist = seed_tracks;
    std::unordered_set<std::string> seen(seed_tracks.begin(), seed_tracks.end());
    while (playlist.size() < static_cast<size_t>(nsongs)) {
        if (method == "append") {
            const int start_idx =
                std::max(0, static_cast<int>(playlist.size()) - lookback);
            const std::vector<std::string> context(playlist.begin() + start_idx,
                                                   playlist.end());
            vec_sum = calculate_vector(context, noise);
        }

        auto similar = most_similar(seen, vec_sum, 1);
        if (similar.empty()) {
            break;
        }
        const std::string next_song = similar.front().first;
        playlist.push_back(next_song);
        seen.insert(next_song);
    }

    return playlist;
}

bool generator::remove_invalid_tracks(std::vector<std::string> &tracks) const {
    const int original_size = tracks.size();
    for (auto it = tracks.begin(); it != tracks.end();) {
        if (!m_audio_vec.contains(*it)) {
            std::cerr << *it << ": is not in the scanned vector directory. Removing it from input." << std::endl;
            it = tracks.erase(it);
        } else {
            it++;
        }
    }
    return original_size == tracks.size();
}

std::vector<std::string> generator::generate_playlist_connect(
    const std::vector<std::string> &seed_tracks, int nsongs,
    float noise) const {
    const int max_tries = 100;

    std::vector<std::string> playlist;
    std::unordered_set<std::string> seen;
    seen.insert(seed_tracks.begin(), seed_tracks.end());
    playlist.push_back(seed_tracks[0]);

    for (size_t t = 1; t < seed_tracks.size(); t++) {
        const std::string &start = seed_tracks[t - 1];
        const std::string &end = seed_tracks[t];

        for (int i = 0; i < nsongs; i++) {
            float alpha =
                static_cast<float>(nsongs - i + 1) / static_cast<float>(nsongs + 1);
            float beta = 1.0f - alpha;

            const vectorf start_vec = m_audio_vec.at(start);
            const vectorf end_vec = m_audio_vec.at(end);
            vectorf blended = alpha * start_vec + beta * end_vec;
            utils::add_noise(blended, noise);

            auto similar = most_similar(seen, blended, max_tries);
            std::string next_song;
            for (const auto &[candidate, _] : similar) {
                if (candidate != end) {
                    next_song = candidate;
                    break;
                }
            }

            if (next_song.empty()) {
                break;
            }
            playlist.push_back(next_song);
            seen.insert(next_song);
        }
        playlist.push_back(end);
    }
    return playlist;
}

std::vector<std::pair<std::string, float>> generator::most_similar(const std::unordered_set<std::string> &excluded,
                                                                   const vectorf &vec_sum, int topn) const {
    float vec_sum_norm = vec_sum.norm();
    std::vector<std::pair<std::string, float>> similar;
    for (const auto &[track, vec] : m_audio_vec) {
        if (excluded.contains(track)) {
            continue;
        }

        float sim = vec_sum.dot(vec) / (vec_sum_norm * vec.norm());
        similar.emplace_back(track, sim);
    }

    sort(similar.begin(), similar.end(),
         [](const auto &a, const auto &b) { return a.second > b.second; });

    if (similar.size() > static_cast<size_t>(topn)) {
        similar.resize(topn);
    }

    return similar;
}

vectorf generator::calculate_vector(const std::vector<std::string> &tracks, float noise) const {
    vectorf vec_sum = vectorf::Zero(m_audio_vec.begin()->second.size());
    for (const std::string &name : tracks) {
        if (m_audio_vec.contains(name)) {
            vec_sum += m_audio_vec.at(name);
        }
    }
    utils::add_noise(vec_sum, noise);
    return vec_sum;
}

static float cos_distance(const vectorf &a, const vectorf &b) {
    float denom = (a.norm() * b.norm());
    if (denom < 0.001) {
        return 1.f;
    }

    float sim = 1.f - a.dot(b) / denom;
    return sim;
}

static float total_distance(const std::unordered_map<std::string, vectorf> &vecs, const std::vector<std::string> &tour) {
    float dist = 0.0;
    for (size_t i = 0; i < tour.size(); ++i) {
        dist += cos_distance(vecs.at(tour.at(i)), vecs.at(tour.at((i + 1) % tour.size())));
    }
    return dist;
}

static void simulated_annealing(const std::unordered_map<std::string, vectorf> &vecs, std::vector<std::string> &bestTour) {
    double T = 10;
    double coolingRate = 0.9995;
    double absoluteTemperature = 1e-4;

    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist01(0.0, 1.0);

    std::vector<std::string> currentTour = bestTour;
    double currentDist = total_distance(vecs, currentTour);
    double bestDist = currentDist;

    while (T > absoluteTemperature) {
        std::vector<std::string> newTour = currentTour;
        int i = std::rand() % newTour.size();
        int j = std::rand() % newTour.size();
        std::swap(newTour[i], newTour[j]);

        double newDist = total_distance(vecs, newTour);
        double delta = newDist - currentDist;

        if (delta < 0 || dist01(rng) < std::exp(-delta / T)) {
            currentTour = newTour;
            currentDist = newDist;
            if (currentDist < bestDist) {
                bestTour = currentTour;
                bestDist = currentDist;
            }
        }

        T *= coolingRate;
    }
}

std::vector<std::string> generator::reorder(const std::vector<std::string> &seed_tracks, const std::string &first_song) {
    std::vector<std::string> result = seed_tracks;
    if (!first_song.empty() && std::find(result.begin(), result.end(), first_song) == result.end()) {
        result.push_back(first_song);
    }

    remove_invalid_tracks(result);
    if (result.empty()) {
        return {};
    }
    simulated_annealing(m_audio_vec, result);

    // Rotate to bring the first song at the front of the vector
    auto it = std::find(result.begin(), result.end(), first_song);
    if (it != result.end()) {
        std::rotate(result.begin(), it, result.end());
    }

    // Reverse if needed to reduce the cost from the first song
    if (result.size() >= 3) {
        const auto &current_vec = m_audio_vec[*result.begin()];
        const auto &prev = *(result.end() - 1);
        const auto &prev_vec = m_audio_vec[prev];
        const auto &next = *(result.begin() + 1);
        const auto &next_vec = m_audio_vec[next];
        if (cos_distance(prev_vec, current_vec) < cos_distance(current_vec, next_vec)) {
            std::reverse(result.begin() + 1, result.end());
        }
    }

    return result;
}

} // namespace deejai
