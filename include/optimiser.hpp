/*
 * chopt - Star Power optimiser for Clone Hero
 * Copyright (C) 2020 Raymond Wright
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CHOPT_OPTIMISER_HPP
#define CHOPT_OPTIMISER_HPP

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "chart.hpp"
#include "points.hpp"
#include "sp.hpp"
#include "time.hpp"

class ProcessedSong {
private:
    // The order of these members is important. We must have m_converter before
    // m_points.
    TimeConverter m_converter;
    PointSet m_points;
    SpData m_sp_data;

public:
    ProcessedSong(const NoteTrack& track, int resolution,
                  const SyncTrack& sync_track, double early_whammy,
                  double squeeze);

    [[nodiscard]] const TimeConverter& converter() const { return m_converter; }
    [[nodiscard]] const PointSet& points() const { return m_points; }
    [[nodiscard]] const SpData& sp_data() const { return m_sp_data; }
};

struct ActivationCandidate {
    PointPtr act_start;
    PointPtr act_end;
    Position earliest_activation_point {Beat(0.0), Measure(0.0)};
    SpBar sp_bar {0.0, 0.0};
};

struct Activation {
    PointPtr act_start;
    PointPtr act_end;
};

// Part of the return value of Optimiser::is_candidate_valid. Says if an
// activation is valid, and if not whether the problem is too little or too much
// Star Power.
enum class ActValidity { success, insufficient_sp, surplus_sp };

// Return value of Optimiser::is_candidate_valid, providing information on
// whether an activation is valid, and if so the earliest position it can end.
struct ActResult {
    Position ending_position;
    ActValidity validity;
};

struct Path {
    std::vector<Activation> activations;
    int score_boost;
};

// Represents a song processed for Star Power optimisation. The constructor
// should only fail due to OOM; invariants on the song are supposed to be
// upheld by the constructors of the arguments.
class Optimiser {
private:
    // The Cache is used to store paths starting from a certain point onwards,
    // i.e., the solution to our subproblems in our dynamic programming
    // algorithm. Cache.full_sp_paths represents the best path with the first
    // activation at the point key or later, under the condition there is
    // already full SP there.
    struct CacheKey {
        PointPtr point;
        Position position {Beat(0.0), Measure(0.0)};

        friend bool operator<(const CacheKey& lhs, const CacheKey& rhs)
        {
            return std::tie(lhs.point, lhs.position.beat)
                < std::tie(rhs.point, rhs.position.beat);
        }
    };

    struct CacheValue {
        Path path;
        std::vector<std::tuple<Activation, CacheKey>> possible_next_acts;
    };

    struct Cache {
        std::map<CacheKey, CacheValue> paths;
        std::map<PointPtr, CacheValue> full_sp_paths;
    };

    ProcessedSong m_song;
    int m_total_solo_boost;
    std::vector<PointPtr> m_next_candidate_points;

    static constexpr double MEASURES_PER_BAR = 8.0;
    static constexpr double MINIMUM_SP_AMOUNT = 0.5;

    [[nodiscard]] PointPtr next_candidate_point(PointPtr point) const;
    [[nodiscard]] CacheKey advance_cache_key(CacheKey key) const;
    [[nodiscard]] PointPtr act_end_lower_bound(PointPtr point, Measure pos,
                                               double sp_bar_amount) const;
    [[nodiscard]] std::optional<CacheValue>
    try_previous_best_subpaths(CacheKey key, const Cache& cache,
                               bool has_full_sp) const;
    CacheValue find_best_subpaths(CacheKey key, Cache& cache,
                                  bool has_full_sp) const;
    Path get_partial_path(CacheKey key, Cache& cache) const;
    Path get_partial_full_sp_path(PointPtr point, Cache& cache) const;

public:
    Optimiser(const NoteTrack& track, int resolution,
              const SyncTrack& sync_track, double early_whammy, double squeeze);
    [[nodiscard]] const PointSet& points() const { return m_song.points(); }
    // Returns an ActResult which says if an activation is valid, and if so the
    // earliest position it can end.
    [[nodiscard]] ActResult
    is_candidate_valid(const ActivationCandidate& activation) const;
    // Return the minimum and maximum amount of SP can be acquired between two
    // points. Does not include SP from the point act_start. first_point is
    // given for the purposes of counting SP grantings notes, e.g. if start is
    // after the middle of first_point's timing window.
    [[nodiscard]] SpBar total_available_sp(Beat start, PointPtr first_point,
                                           PointPtr act_start) const;
    // Return the optimal Star Power path.
    [[nodiscard]] Path optimal_path() const;
    // Return the summary of a path.
    [[nodiscard]] std::string path_summary(const Path& path) const;
};

#endif
