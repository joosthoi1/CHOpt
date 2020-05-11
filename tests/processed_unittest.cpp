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

#include "catch.hpp"

#include "processed.hpp"

TEST_CASE("3 arg total_available_sp counts SP correctly")
{
    std::vector<Note> notes {{0},        {192},  {384},  {576},
                             {768, 192}, {1152}, {1344}, {1536}};
    std::vector<StarPower> phrases {{0, 50}, {384, 50}, {768, 400}, {1344, 50}};
    NoteTrack note_track {notes, phrases, {}};
    ProcessedSong song {note_track, 192, {}, 1.0, 1.0};
    const auto& points = song.points();

    SECTION("Phrases are counted correctly")
    {
        REQUIRE(song.total_available_sp(Beat(0.0), points.cbegin(),
                                        points.cbegin() + 1)
                == SpBar {0.25, 0.25});
        REQUIRE(song.total_available_sp(Beat(0.0), points.cbegin(),
                                        points.cbegin() + 2)
                == SpBar {0.25, 0.25});
        REQUIRE(song.total_available_sp(Beat(0.5), points.cbegin() + 2,
                                        points.cbegin() + 3)
                == SpBar {0.25, 0.25});
    }

    SECTION("Whammy is counted correctly")
    {
        auto result = song.total_available_sp(Beat(4.0), points.cbegin() + 4,
                                              points.cbegin() + 5);
        REQUIRE(result.min() == Approx(0.0));
        REQUIRE(result.max() == Approx(0.00112847));
    }

    SECTION("Whammy is counted correctly even started mid hold")
    {
        auto result = song.total_available_sp(Beat(4.5), points.cend() - 3,
                                              points.cend() - 3);
        REQUIRE(result.min() == Approx(0.0));
        REQUIRE(result.max() == Approx(0.0166667));
    }

    SECTION("required_whammy_end is accounted for")
    {
        auto result = song.total_available_sp(Beat(4.0), points.cbegin() + 4,
                                              points.cbegin() + 5, Beat(4.02));

        REQUIRE(result.min() == Approx(0.000666667));
        REQUIRE(result.max() == Approx(0.00112847));

        result = song.total_available_sp(Beat(4.0), points.cbegin() + 4,
                                         points.cbegin() + 5, Beat(4.10));

        REQUIRE(result.min() == Approx(0.00112847));
        REQUIRE(result.max() == Approx(0.00112847));
    }

    SECTION("SP does not exceed full bar")
    {
        REQUIRE(song.total_available_sp(Beat(0.0), points.cbegin(),
                                        points.cend() - 1)
                == SpBar {1.0, 1.0});
    }

    SECTION("SP notes are counted from first_point when start is past middle")
    {
        REQUIRE(song.total_available_sp(Beat(0.05), points.cbegin(),
                                        points.cbegin() + 1)
                == SpBar {0.25, 0.25});
    }
}

TEST_CASE("is_candidate_valid works with no whammy")
{
    std::vector<Note> notes {{0}, {1536}, {3072}, {6144}};
    NoteTrack note_track {notes, {}, {}};
    ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
    const auto& points = track.points();
    ActivationCandidate candidate {points.cbegin(),
                                   points.cbegin() + 3,
                                   {Beat(0.0), Measure(0.0)},
                                   {1.0, 1.0}};
    ProcessedSong second_track {note_track, 192, SyncTrack({{0, 3, 4}}, {}),
                                1.0, 1.0};
    const auto& second_points = second_track.points();
    ActivationCandidate second_candidate {second_points.cbegin(),
                                          second_points.cbegin() + 3,
                                          {Beat(0.0), Measure(0.0)},
                                          {1.0, 1.0}};

    SECTION("Full bar works with time signatures")
    {
        REQUIRE(track.is_candidate_valid(candidate).validity
                == ActValidity::success);
        REQUIRE(second_track.is_candidate_valid(second_candidate).validity
                == ActValidity::insufficient_sp);
    }

    SECTION("Half bar works with time signatures")
    {
        candidate.act_end = points.cbegin() + 2;
        candidate.sp_bar = {0.5, 0.5};
        second_candidate.act_end = second_points.cbegin() + 2;
        second_candidate.sp_bar = {0.5, 0.5};

        REQUIRE(track.is_candidate_valid(candidate).validity
                == ActValidity::success);
        REQUIRE(second_track.is_candidate_valid(second_candidate).validity
                == ActValidity::insufficient_sp);
    }

    SECTION("Below half bar never works")
    {
        candidate.act_end = points.cbegin() + 1;
        candidate.sp_bar.max() = 0.25;

        REQUIRE(track.is_candidate_valid(candidate).validity
                == ActValidity::insufficient_sp);
    }

    SECTION("Check next point needs to not lie in activation")
    {
        candidate.act_end = points.cbegin() + 1;
        candidate.sp_bar.max() = 0.6;

        REQUIRE(track.is_candidate_valid(candidate).validity
                == ActValidity::surplus_sp);
    }

    SECTION("Check intermediate SP is accounted for")
    {
        std::vector<StarPower> phrases {{3000, 100}};
        NoteTrack overlap_notes {notes, phrases, {}};
        ProcessedSong overlap_track {overlap_notes, 192, {}, 1.0, 1.0};
        const auto& overlap_points = overlap_track.points();
        ActivationCandidate overlap_candidate {overlap_points.cbegin(),
                                               overlap_points.cbegin() + 3,
                                               {Beat(0.0), Measure(0.0)},
                                               {0.8, 0.8}};

        REQUIRE(overlap_track.is_candidate_valid(overlap_candidate).validity
                == ActValidity::success);
    }

    SECTION("Check only reached intermediate SP is accounted for")
    {
        notes[2].position = 6000;
        std::vector<StarPower> phrases {{6000, 100}};
        NoteTrack overlap_notes {notes, phrases, {}};
        ProcessedSong overlap_track {overlap_notes, 192, {}, 1.0, 1.0};
        const auto& overlap_points = overlap_track.points();
        ActivationCandidate overlap_candidate {overlap_points.cbegin(),
                                               overlap_points.cbegin() + 3,
                                               {Beat(0.0), Measure(0.0)},
                                               {0.8, 0.8}};

        REQUIRE(overlap_track.is_candidate_valid(overlap_candidate).validity
                == ActValidity::insufficient_sp);
    }

    SECTION("Last note's SP status is not ignored")
    {
        notes[3].position = 4000;
        std::vector<StarPower> phrases {{3072, 100}};
        NoteTrack overlap_notes {notes, phrases, {}};
        ProcessedSong overlap_track {overlap_notes, 192, {}, 1.0, 1.0};
        const auto& overlap_points = overlap_track.points();
        ActivationCandidate overlap_candidate {overlap_points.cbegin(),
                                               overlap_points.cbegin() + 2,
                                               {Beat(0.0), Measure(0.0)},
                                               {0.5, 0.5}};

        REQUIRE(overlap_track.is_candidate_valid(overlap_candidate).validity
                == ActValidity::surplus_sp);
    }

    SECTION("SP bar does not exceed full bar")
    {
        std::vector<Note> overlap_notes {{0}, {2}, {7000}};
        std::vector<StarPower> phrases {{0, 1}, {2, 1}};
        NoteTrack overlap_note_track {overlap_notes, phrases, {}};
        ProcessedSong overlap_track {overlap_note_track, 192, {}, 1.0, 1.0};
        const auto& overlap_points = overlap_track.points();
        ActivationCandidate overlap_candidate {overlap_points.cbegin(),
                                               overlap_points.cbegin() + 2,
                                               {Beat(0.0), Measure(0.0)},
                                               {1.0, 1.0}};

        REQUIRE(overlap_track.is_candidate_valid(overlap_candidate).validity
                == ActValidity::insufficient_sp);
    }

    SECTION("Earliest activation point is considered")
    {
        candidate.act_end = points.cbegin() + 1;
        candidate.sp_bar = {0.53125, 0.53125};
        candidate.earliest_activation_point = {Beat(-2.0), Measure(-0.5)};

        REQUIRE(track.is_candidate_valid(candidate).validity
                == ActValidity::success);
    }
}

TEST_CASE("is_candidate_valid works with whammy")
{
    std::vector<Note> notes {{0, 960}, {3840}, {6144}};
    std::vector<StarPower> phrases {{0, 7000}};
    NoteTrack note_track {notes, phrases, {}};
    ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
    const auto& points = track.points();
    ActivationCandidate candidate {points.cbegin(),
                                   points.cend() - 2,
                                   {Beat(0.0), Measure(0.0)},
                                   {0.5, 0.5}};

    SECTION("Check whammy is counted")
    {
        REQUIRE(track.is_candidate_valid(candidate).validity
                == ActValidity::success);
    }

    SECTION("Check compressed activations are counted")
    {
        candidate.sp_bar.max() = 0.9;
        REQUIRE(track.is_candidate_valid(candidate).validity
                == ActValidity::success);
    }
}

TEST_CASE("is_candidate_valid takes into account minimum SP")
{
    std::vector<Note> notes {{0}, {1536}, {2304}, {3072}, {4608}};
    NoteTrack note_track {notes, {}, {}};
    ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
    const auto& points = track.points();
    ActivationCandidate candidate {points.cbegin(),
                                   points.cbegin() + 3,
                                   {Beat(0.0), Measure(0.0)},
                                   {0.5, 1.0}};

    SECTION("Lower SP is considered")
    {
        REQUIRE(track.is_candidate_valid(candidate).validity
                == ActValidity::success);
    }

    SECTION("Lower SP is only considered down to a half-bar")
    {
        candidate.act_end = points.cbegin() + 1;
        candidate.sp_bar.min() = 0.25;

        REQUIRE(track.is_candidate_valid(candidate).validity
                == ActValidity::surplus_sp);
    }
}

TEST_CASE("is_candidate_valid takes into account squeezing")
{
    SECTION("Front end and back end of the activation endpoints are considered")
    {
        std::vector<Note> notes {{0}, {3110}};
        NoteTrack note_track {notes, {}, {}};
        ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
        const auto& points = track.points();
        ActivationCandidate candidate {points.cbegin(),
                                       points.cbegin() + 1,
                                       {Beat(0.0), Measure(0.0)},
                                       {0.5, 0.5}};

        REQUIRE(track.is_candidate_valid(candidate).validity
                == ActValidity::success);
    }

    SECTION("Next note can be squeezed late to avoid going too far")
    {
        std::vector<Note> notes {{0}, {3034}, {3053}};
        NoteTrack note_track {notes, {}, {}};
        ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
        const auto& points = track.points();
        ActivationCandidate candidate {points.cbegin(),
                                       points.cbegin() + 1,
                                       {Beat(0.0), Measure(0.0)},
                                       {0.5, 0.5}};

        REQUIRE(track.is_candidate_valid(candidate).validity
                == ActValidity::success);
    }

    SECTION("Intermediate SP can be hit early")
    {
        std::vector<Note> notes {{0}, {3102}, {4608}};
        std::vector<StarPower> phrases {{3100, 100}};
        NoteTrack note_track {notes, phrases, {}};
        ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
        const auto& points = track.points();
        ActivationCandidate candidate {points.cbegin(),
                                       points.cbegin() + 2,
                                       {Beat(0.0), Measure(0.0)},
                                       {0.5, 0.5}};

        REQUIRE(track.is_candidate_valid(candidate).validity
                == ActValidity::success);
    }

    SECTION("Intermediate SP can be hit late")
    {
        std::vector<Note> notes {{0}, {768}, {6942}};
        std::vector<StarPower> phrases {{768, 100}};
        NoteTrack note_track {notes, phrases, {}};
        ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
        const auto& points = track.points();
        ActivationCandidate candidate {points.cbegin(),
                                       points.cbegin() + 2,
                                       {Beat(0.0), Measure(0.0)},
                                       {1.0, 1.0}};

        REQUIRE(track.is_candidate_valid(candidate).validity
                == ActValidity::success);
    }
}

TEST_CASE("is_restricted_candidate_valid takes into account squeeze param")
{
    SECTION("Front end and back end are restricted")
    {
        std::vector<Note> notes {{0}, {3110}};
        NoteTrack note_track {notes, {}, {}};
        ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
        const auto& points = track.points();
        ActivationCandidate candidate {points.cbegin(),
                                       points.cbegin() + 1,
                                       {Beat(0.0), Measure(0.0)},
                                       {0.5, 0.5}};

        REQUIRE(track.is_restricted_candidate_valid(candidate, 0.5).validity
                == ActValidity::insufficient_sp);
        REQUIRE(track.is_restricted_candidate_valid(candidate, 1.0).validity
                == ActValidity::success);
    }

    SECTION("Intermediate SP front end is restricted")
    {
        std::vector<Note> notes {{0}, {3102}, {4608}};
        std::vector<StarPower> phrases {{3100, 100}};
        NoteTrack note_track {notes, phrases, {}};
        ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
        const auto& points = track.points();
        ActivationCandidate candidate {points.cbegin(),
                                       points.cbegin() + 2,
                                       {Beat(0.0), Measure(0.0)},
                                       {0.5, 0.5}};

        REQUIRE(track.is_restricted_candidate_valid(candidate, 0.5).validity
                == ActValidity::insufficient_sp);
        REQUIRE(track.is_restricted_candidate_valid(candidate, 1.0).validity
                == ActValidity::success);
    }

    SECTION("Intermediate SP back end is restricted")
    {
        std::vector<Note> notes {{0}, {768}, {6942}};
        std::vector<StarPower> phrases {{768, 100}};
        NoteTrack note_track {notes, phrases, {}};
        ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
        const auto& points = track.points();
        ActivationCandidate candidate {points.cbegin(),
                                       points.cbegin() + 2,
                                       {Beat(0.0), Measure(0.0)},
                                       {1.0, 1.0}};

        REQUIRE(track.is_restricted_candidate_valid(candidate, 0.5).validity
                == ActValidity::insufficient_sp);
        REQUIRE(track.is_restricted_candidate_valid(candidate, 1.0).validity
                == ActValidity::success);
    }

    SECTION("Next note back end is restricted")
    {
        std::vector<Note> notes {{0}, {3034}, {3053}};
        NoteTrack note_track {notes, {}, {}};
        ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
        const auto& points = track.points();
        ActivationCandidate candidate {points.cbegin(),
                                       points.cbegin() + 1,
                                       {Beat(0.0), Measure(0.0)},
                                       {0.5, 0.5}};

        REQUIRE(track.is_restricted_candidate_valid(candidate, 0.5).validity
                == ActValidity::surplus_sp);
        REQUIRE(track.is_restricted_candidate_valid(candidate, 1.0).validity
                == ActValidity::success);
    }

    SECTION("End position is finite if activation goes past last note")
    {
        std::vector<Note> notes {{0}};
        NoteTrack note_track {notes, {}, {}};
        ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
        const auto& points = track.points();
        ActivationCandidate candidate {points.cbegin(),
                                       points.cbegin(),
                                       {Beat(0.0), Measure(0.0)},
                                       {1.0, 1.0}};
        auto result = track.is_restricted_candidate_valid(candidate, 1.0);

        REQUIRE(result.validity == ActValidity::success);
        REQUIRE(result.ending_position.beat.value() < 40.0);
    }
}

TEST_CASE("is_restricted_candidate_valid takes into account forced whammy")
{
    std::vector<Note> notes {{0, 768}, {3072}, {3264}};
    std::vector<StarPower> phrases {{0, 3300}};
    NoteTrack note_track {notes, phrases, {}};
    ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
    const auto& points = track.points();
    ActivationCandidate candidate {points.cbegin(),
                                   points.cend() - 2,
                                   {Beat(0.0), Measure(0.0)},
                                   {0.5, 0.5}};

    REQUIRE(track
                .is_restricted_candidate_valid(candidate, 1.0,
                                               {Beat(0.0), Measure(0.0)})
                .validity
            == ActValidity::success);
    REQUIRE(track
                .is_restricted_candidate_valid(candidate, 1.0,
                                               {Beat(4.0), Measure(1.0)})
                .validity
            == ActValidity::surplus_sp);
}

TEST_CASE("adjusted_hit_window_* functions return correct values")
{
    std::vector<Note> notes {{0}};
    NoteTrack note_track {notes, {}, {}};
    ProcessedSong track {note_track, 192, {}, 1.0, 1.0};
    const auto& points = track.points();

    SECTION("adjusted_hit_window_start returns correct values")
    {
        REQUIRE(
            track.adjusted_hit_window_start(points.cbegin(), 0.5).beat.value()
            == Approx(-0.07));
        REQUIRE(
            track.adjusted_hit_window_start(points.cbegin(), 1.0).beat.value()
            == Approx(-0.14));
    }

    SECTION("adjusted_hit_window_end returns correct values")
    {
        REQUIRE(track.adjusted_hit_window_end(points.cbegin(), 0.5).beat.value()
                == Approx(0.07));
        REQUIRE(track.adjusted_hit_window_end(points.cbegin(), 1.0).beat.value()
                == Approx(0.14));
    }
}
