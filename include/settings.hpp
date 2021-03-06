/*
 * CHOpt - Star Power optimiser for Clone Hero
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

#ifndef CHOPT_SETTINGS_HPP
#define CHOPT_SETTINGS_HPP

#include <string>

#include "song.hpp"

// This struct represents the options chosen on the command line by the user.
struct Settings {
    bool blank;
    std::string filename;
    std::string image_path;
    bool draw_bpms;
    bool draw_solos;
    bool draw_time_sigs;
    Difficulty difficulty;
    Instrument instrument;
    double squeeze;
    double early_whammy;
    double lazy_whammy;
    int speed;
};

// Parses the command line options. If something is wrong with the options
// chosen, an exception is thrown.
Settings from_args(int argc, char** argv);

#endif
