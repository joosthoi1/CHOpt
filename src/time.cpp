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

#include <algorithm>
#include <cassert>

#include "time.hpp"

TimeConverter::TimeConverter(const SyncTrack& sync_track,
                             const SongHeader& header)
{
    auto last_tick = 0U;
    auto last_beat_rate = DEFAULT_BEAT_RATE;
    auto last_measure = 0.0;

    for (const auto& ts : sync_track.time_sigs()) {
        last_measure += (ts.position - last_tick)
            / (header.resolution() * last_beat_rate);
        const auto beat
            = static_cast<double>(ts.position) / header.resolution();
        m_measure_timestamps.push_back({Measure(last_measure), Beat(beat)});
        last_beat_rate = (ts.numerator * DEFAULT_BEAT_RATE) / ts.denominator;
        last_tick = ts.position;
    }

    m_last_beat_rate = last_beat_rate;

    assert(!m_measure_timestamps.empty()); // NOLINT
}

Measure TimeConverter::beats_to_measures(Beat beats) const
{
    const auto pos = std::lower_bound(
        m_measure_timestamps.cbegin(), m_measure_timestamps.cend(), beats,
        [](const auto& x, const auto& y) { return x.beat < y; });
    if (pos == m_measure_timestamps.cend()) {
        const auto& back = m_measure_timestamps.back();
        return back.measure + (beats - back.beat).to_measure(m_last_beat_rate);
    }
    if (pos == m_measure_timestamps.cbegin()) {
        return pos->measure - (pos->beat - beats).to_measure(DEFAULT_BEAT_RATE);
    }
    const auto prev = pos - 1;
    return prev->measure
        + (pos->measure - prev->measure)
        * ((beats - prev->beat) / (pos->beat - prev->beat));
}

Beat TimeConverter::measures_to_beats(Measure measures) const
{
    const auto pos = std::lower_bound(
        m_measure_timestamps.cbegin(), m_measure_timestamps.cend(), measures,
        [](const auto& x, const auto& y) { return x.measure < y; });
    if (pos == m_measure_timestamps.cend()) {
        const auto& back = m_measure_timestamps.back();
        return back.beat + (measures - back.measure).to_beat(m_last_beat_rate);
    }
    if (pos == m_measure_timestamps.cbegin()) {
        return pos->beat - (pos->measure - measures).to_beat(DEFAULT_BEAT_RATE);
    }
    const auto prev = pos - 1;
    return prev->beat
        + (pos->beat - prev->beat)
        * ((measures - prev->measure) / (pos->measure - prev->measure));
}
