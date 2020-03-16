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
#include <charconv>
#include <cstdlib>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

#include "chart.hpp"

// This represents a bundle of data akin to a SongHeader, except it is only for
// mid-parser usage. Unlike a SongHeader, there are no invariants.
struct PreSongHeader {
    static constexpr float DEFAULT_RESOLUTION = 192.F;
    float offset = 0.F;
    float resolution = DEFAULT_RESOLUTION;
};

// This represents a bundle of data akin to a NoteTrack, except it is only for
// mid-parser usage. Unlike a NoteTrack, there are no invariants.
struct PreNoteTrack {
    std::vector<Note> notes;
    std::vector<StarPower> sp_phrases;
    std::vector<ChartEvent> events;
};

static bool is_empty(const PreNoteTrack& track)
{
    return track.notes.empty() && track.sp_phrases.empty()
        && track.events.empty();
}

// This represents a bundle of data akin to a SyncTrack, except it is only for
// mid-parser usage. Unlike a SyncTrack, there are no invariants.
struct PreSyncTrack {
    std::vector<TimeSignature> time_sigs;
    std::vector<BPM> bpms;
};

SongHeader::SongHeader(float offset, float resolution)
    : m_offset {offset}
    , m_resolution {resolution}
{
    if (m_resolution <= 0.F) {
        throw std::invalid_argument("Songs with resolution < 0 are invalid");
    }
}

NoteTrack::NoteTrack(std::vector<Note> notes, std::vector<StarPower> sp_phrases,
                     std::vector<ChartEvent> events)
    : m_sp_phrases {std::move(sp_phrases)}
    , m_events {std::move(events)}
{
    std::stable_sort(notes.begin(), notes.end(),
                     [](const auto& lhs, const auto& rhs) {
                         return std::tie(lhs.position, lhs.colour)
                             < std::tie(rhs.position, rhs.colour);
                     });

    if (!notes.empty()) {
        auto prev_note = notes.cbegin();
        for (auto p = notes.cbegin(); p < notes.cend(); ++p) {
            if (p->position != prev_note->position
                || p->colour != prev_note->colour) {
                m_notes.push_back(*prev_note);
            }
            prev_note = p;
        }
        m_notes.push_back(*prev_note);
    }
}

SyncTrack::SyncTrack(std::vector<TimeSignature> time_sigs,
                     std::vector<BPM> bpms)
    : m_time_sigs {std::move(time_sigs)}
    , m_bpms {std::move(bpms)}
{
}

static bool string_starts_with(std::string_view input, std::string_view pattern)
{
    if (input.size() < pattern.size()) {
        return false;
    }

    return input.substr(0, pattern.size()) == pattern;
}

static std::string_view skip_whitespace(std::string_view input)
{
    const auto first_non_ws_location = input.find_first_not_of(" \f\n\r\t\v");
    input.remove_prefix(std::min(first_non_ws_location, input.size()));
    return input;
}

// This returns a string_view from the start of input until a carriage return
// or newline. input is changed to point to the first character past the
// detected newline character that is not a whitespace character.
static std::string_view break_off_newline(std::string_view& input)
{
    const auto newline_location = input.find_first_of("\r\n");
    if (newline_location == std::string_view::npos) {
        const auto line = input;
        input.remove_prefix(input.size());
        return line;
    }

    const auto line = input.substr(0, newline_location);
    input.remove_prefix(newline_location);
    input = skip_whitespace(input);
    return line;
}

// Split input by space characters, similar to .Split(' ') in C#. Note that
// the lifetime of the string_views in the output is the same as that of the
// input.
static std::vector<std::string_view> split_by_space(std::string_view input)
{
    std::vector<std::string_view> substrings;

    while (true) {
        const auto space_location = input.find(' ');
        if (space_location == std::string_view::npos) {
            break;
        }
        substrings.push_back(input.substr(0, space_location));
        input.remove_prefix(space_location + 1);
    }

    substrings.push_back(input);
    return substrings;
}

// Return the substring with no leading or trailing quotation marks.
static std::string_view trim_quotes(std::string_view input)
{
    const auto first_non_quote = input.find_first_not_of('"');
    if (first_non_quote == std::string_view::npos) {
        return input.substr(0, 0);
    }
    const auto last_non_quote = input.find_last_not_of('"');
    return input.substr(first_non_quote, last_non_quote + 1);
}

// Convert a string_view to a uint32_t. If there are any problems with the
// input, this function throws.
static std::optional<uint32_t> string_view_to_uint(std::string_view input)
{
    uint32_t result;
    const char* last = input.data() + input.size();
    auto [p, ec] = std::from_chars(input.data(), last, result);
    if ((ec != std::errc()) || (p != last)) {
        return {};
    }
    return result;
}

// Convert a string_view to an int32_t. If there are any problems with the
// input, this function throws.
static std::optional<int32_t> string_view_to_int(std::string_view input)
{
    int32_t result;
    const char* last = input.data() + input.size();
    auto [p, ec] = std::from_chars(input.data(), last, result);
    if ((ec != std::errc()) || (p != last)) {
        return {};
    }
    return result;
}

// Convert a string_view to a float. If there are any problems with the
// input, this function throws.
static std::optional<float> string_view_to_float(std::string_view input)
{
// We need to do this conditional because for now only MSVC's STL
// implements std::from_chars for floats.
#if defined(_MSC_VER) && _MSC_VER >= 1924
    float result;
    const char* last = input.data() + input.size();
    auto [p, ec] = std::from_chars(input.data(), last, result);
    if ((ec != std::errc()) || (p != last)) {
        return {};
    }
    return result;
#else
    std::string null_terminated_input(input);
    size_t chars_processed = 0;
    float result = std::stof(null_terminated_input, &chars_processed);
    if (chars_processed != null_terminated_input.size()) {
        return {};
    }
    return result;
#endif
}

static std::string_view skip_section(std::string_view input)
{
    auto next_line = break_off_newline(input);
    if (next_line != "{") {
        throw std::runtime_error("Section does not open with {");
    }

    do {
        next_line = break_off_newline(input);
    } while (next_line != "}");

    return input;
}

static std::string_view read_song_header(std::string_view input,
                                         PreSongHeader& header)
{
    if (break_off_newline(input) != "{") {
        throw std::runtime_error("[Song] does not open with {");
    }

    while (true) {
        auto line = break_off_newline(input);
        if (line == "}") {
            break;
        }

        if (string_starts_with(line, "Offset = ")) {
            constexpr auto OFFSET_LEN = 9;
            line.remove_prefix(OFFSET_LEN);
            const auto result = string_view_to_float(line);
            if (result) {
                header.offset = *result;
            }
        } else if (string_starts_with(line, "Resolution = ")) {
            constexpr auto RESOLUTION_LEN = 13;
            line.remove_prefix(RESOLUTION_LEN);
            const auto result = string_view_to_float(line);
            if (result) {
                header.resolution = *result;
            }
        }
    }

    return input;
}

static std::string_view read_sync_track(std::string_view input,
                                        PreSyncTrack& sync_track)
{
    if (break_off_newline(input) != "{") {
        throw std::runtime_error("[SyncTrack] does not open with {");
    }

    while (true) {
        const auto line = break_off_newline(input);
        if (line == "}") {
            break;
        }

        const auto split_string = split_by_space(line);
        if (split_string.size() < 4) {
            throw std::invalid_argument("Event missing data");
        }
        const auto pre_position = string_view_to_uint(split_string[0]);
        if (!pre_position) {
            continue;
        }
        const auto position = *pre_position;

        const auto type = split_string[2];

        if (type == "TS") {
            const auto numerator = string_view_to_uint(split_string[3]);
            if (!numerator) {
                continue;
            }
            uint32_t denominator = 2;
            if (split_string.size() > 4) {
                const auto result = string_view_to_uint(split_string[4]);
                if (result) {
                    denominator = *result;
                } else {
                    continue;
                }
            }
            sync_track.time_sigs.push_back(
                {position, *numerator, 1U << denominator});
        } else if (type == "B") {
            const auto bpm = string_view_to_uint(split_string[3]);
            if (!bpm) {
                continue;
            }
            sync_track.bpms.push_back({position, *bpm});
        }
    }

    return input;
}

static std::string_view read_events(std::string_view input,
                                    std::vector<Section>& sections)
{
    if (break_off_newline(input) != "{") {
        throw std::runtime_error("[Events] does not open with {");
    }

    while (true) {
        const auto line = break_off_newline(input);
        if (line == "}") {
            break;
        }

        const auto split_string = split_by_space(line);
        if (split_string.size() < 4) {
            throw std::invalid_argument("Event missing data");
        }
        const auto position = string_view_to_uint(split_string[0]);

        const auto type = split_string[2];

        if (type == "E" && position) {
            if ((split_string[3] == "\"section") || (split_string.size() > 4)) {
                std::string section_name(trim_quotes(split_string[4]));
                constexpr auto NAME_START = 5U;
                for (auto i = NAME_START; i < split_string.size(); ++i) {
                    section_name += " ";
                    section_name += trim_quotes(split_string[i]);
                }
                sections.push_back({*position, section_name});
            }
        }
    }

    return input;
}

static std::string_view read_single_track(std::string_view input,
                                          PreNoteTrack& track)
{
    if (!is_empty(track)) {
        return skip_section(input);
    }

    constexpr auto GREEN_CODE = 0;
    constexpr auto RED_CODE = 1;
    constexpr auto YELLOW_CODE = 2;
    constexpr auto BLUE_CODE = 3;
    constexpr auto ORANGE_CODE = 4;
    constexpr auto FORCED_CODE = 5;
    constexpr auto TAP_CODE = 6;
    constexpr auto OPEN_CODE = 7;

    if (break_off_newline(input) != "{") {
        throw std::runtime_error("A [*Single] track does not open with {");
    }

    std::set<uint32_t> forced_flags;
    std::set<uint32_t> tap_flags;

    while (true) {
        const auto line = break_off_newline(input);
        if (line == "}") {
            break;
        }

        const auto split_string = split_by_space(line);
        if (split_string.size() < 4) {
            throw std::invalid_argument("Event missing data");
        }
        const auto pre_position = string_view_to_uint(split_string[0]);
        if (!pre_position) {
            continue;
        }
        const auto position = *pre_position;
        const auto type = split_string[2];

        if (type == "N") {
            constexpr auto NOTE_EVENT_LENGTH = 5;
            if (split_string.size() < NOTE_EVENT_LENGTH) {
                throw std::invalid_argument("Note event missing data");
            }
            const auto fret_type = string_view_to_int(split_string[3]);
            if (!fret_type) {
                continue;
            }
            const auto pre_length = string_view_to_uint(split_string[4]);
            if (!pre_length) {
                continue;
            }
            const auto length = *pre_length;
            switch (*fret_type) {
            case GREEN_CODE:
                track.notes.push_back({position, length, NoteColour::Green});
                break;
            case RED_CODE:
                track.notes.push_back({position, length, NoteColour::Red});
                break;
            case YELLOW_CODE:
                track.notes.push_back({position, length, NoteColour::Yellow});
                break;
            case BLUE_CODE:
                track.notes.push_back({position, length, NoteColour::Blue});
                break;
            case ORANGE_CODE:
                track.notes.push_back({position, length, NoteColour::Orange});
                break;
            case FORCED_CODE:
                forced_flags.insert(position);
                break;
            case TAP_CODE:
                tap_flags.insert(position);
                break;
            case OPEN_CODE:
                track.notes.push_back({position, length, NoteColour::Open});
                break;
            default:
                throw std::invalid_argument("Invalid note type");
            }
        } else if (type == "S") {
            constexpr auto SP_EVENT_LENGTH = 5;
            if (split_string.size() < SP_EVENT_LENGTH) {
                throw std::invalid_argument("SP event missing data");
            }
            if (string_view_to_int(split_string[3]) != 2) {
                continue;
            }
            const auto pre_length = string_view_to_uint(split_string[4]);
            if (!pre_length) {
                continue;
            }
            const auto length = *pre_length;
            track.sp_phrases.push_back({position, length});
        } else if (type == "E") {
            const auto event_name = split_string[3];
            track.events.push_back({position, std::string(event_name)});
        }
    }

    for (auto& note : track.notes) {
        if (forced_flags.count(note.position) != 0) {
            note.is_forced = true;
        }
        if (tap_flags.count(note.position) != 0) {
            note.is_tap = true;
        }
    }

    return input;
}

Chart Chart::parse_chart(std::string_view input)
{
    Chart chart;

    PreSongHeader pre_header;
    PreSyncTrack pre_sync_track;
    std::map<Difficulty, PreNoteTrack> pre_tracks;

    // Trim off UTF-8 BOM if present
    if (string_starts_with(input, "\xEF\xBB\xBF")) {
        input.remove_prefix(3);
    }

    while (!input.empty()) {
        const auto header = break_off_newline(input);
        if (header == "[Song]") {
            input = read_song_header(input, pre_header);
        } else if (header == "[SyncTrack]") {
            input = read_sync_track(input, pre_sync_track);
        } else if (header == "[Events]") {
            input = read_events(input, chart.m_sections);
        } else if (header == "[EasySingle]") {
            input = read_single_track(input, pre_tracks[Difficulty::Easy]);
        } else if (header == "[MediumSingle]") {
            input = read_single_track(input, pre_tracks[Difficulty::Medium]);
        } else if (header == "[HardSingle]") {
            input = read_single_track(input, pre_tracks[Difficulty::Hard]);
        } else if (header == "[ExpertSingle]") {
            input = read_single_track(input, pre_tracks[Difficulty::Expert]);
        } else {
            input = skip_section(input);
        }
    }

    chart.m_header = SongHeader(pre_header.offset, pre_header.resolution);
    chart.m_sync_track = SyncTrack(std::move(pre_sync_track.time_sigs),
                                   std::move(pre_sync_track.bpms));

    for (auto& key_track : pre_tracks) {
        auto diff = key_track.first;
        auto& track = key_track.second;
        auto new_track
            = NoteTrack(std::move(track.notes), std::move(track.sp_phrases),
                        std::move(track.events));
        chart.m_note_tracks.emplace(diff, std::move(new_track));
    }

    return chart;
}
