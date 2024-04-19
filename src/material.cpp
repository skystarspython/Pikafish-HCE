/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2022 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cassert>
#include <cstring>   // For std::memset

#include "material.h"
#include "thread.h"

using namespace std;

namespace Stockfish {

    namespace {
#define S(mg, eg) make_score(mg, eg)

        // Polynomial material imbalance parameters

        // One Score parameter for each pair (our piece, another of our pieces)
        Score QuadraticOurs[][PIECE_TYPE_NB] = {
            // OUR PIECE 2
            // rook   advisor  cannon   pawn     knight    bishop  advisor pair  bishop pair
            {S(71, 3)                                                                       }, // Rook
            {S(24, 74), S(44, -67)                                                          }, // Advisor
            {S(48, 72), S(33, 62), S(-5, -63)                                               }, // Cannon      OUR PIECE 1
            {S(75, -14), S(31, 44), S(-3, 28), S(-11, 11)                                   }, // Pawn
            {S(-92, 53), S(27, -9), S(-3, 234), S(44, 88), S(-30, -29)                      }, // Knight
            {S(54, 104), S(175, -103), S(106, -64), S(43, -113), S(24, 6), S(2, -59)        },  // Bishop
            {S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0)}, // Advisor pair
            {S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0)} // Bishop pair
        };

        // One Score parameter for each pair (our piece, their piece)
        Score QuadraticTheirs[][PIECE_TYPE_NB] = {
            // THEIR PIECE
            // rook   advisor  cannon   pawn     knight    bishop
            {S(-35, -46)                                           }, // Rook
            {S(-92, 32), S(138, -7)                                }, // Advisor
            {S(-83, 13), S(-41, 43), S(20, 28)                     }, // Cannon      OUR PIECE
            {S(-2, 13), S(-57, -118), S(-18, 121), S(70, -58)      }, // Pawn
            {S(-37, 17), S(14, -86), S(38, -24), S(67, 43), S(-21, -42) }, // Knight
            {S(72, 38), S(6, -79), S(24, -2), S(48, 30), S(30, 14), S(-51, 35) },  // Bishop
            {S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0)}, // Advisor pair
            {S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0), S(0,0)} // Bishop pair
        };

TUNE(
    SetRange(-300,300),
    QuadraticOurs[0][0], // Rook vs. rook
    QuadraticOurs[1][0], // Advisor vs. rook
    QuadraticOurs[1][1], // Advisor vs. advisor
    QuadraticOurs[2][0], // Cannon vs. rook
    QuadraticOurs[2][1], // Cannon vs. advisor
    QuadraticOurs[2][2], // Cannon vs. cannon
    QuadraticOurs[3][0], // Pawn vs. rook
    QuadraticOurs[3][1], // Pawn vs. advisor
    QuadraticOurs[3][2], // Pawn vs. cannon
    QuadraticOurs[3][3], // Pawn vs. pawn
    QuadraticOurs[4][0], // Knight vs. rook
    QuadraticOurs[4][1], // Knight vs. advisor
    QuadraticOurs[4][2], // Knight vs. cannon
    QuadraticOurs[4][3], // Knight vs. pawn
    QuadraticOurs[4][4], // Knight vs. knight
    QuadraticOurs[5][0], // Bishop vs. rook
    QuadraticOurs[5][1], // Bishop vs. advisor
    QuadraticOurs[5][2], // Bishop vs. cannon
    QuadraticOurs[5][3], // Bishop vs. pawn
    QuadraticOurs[5][4], // Bishop vs. knight
    QuadraticOurs[5][5], // Bishop vs. bishop
    QuadraticOurs[6][0], // Advisor pair vs. rook
    QuadraticOurs[6][1], // Advisor pair vs. advisor
    QuadraticOurs[6][2], // Advisor pair vs. cannon
    QuadraticOurs[6][3], // Advisor pair vs. pawn
    QuadraticOurs[6][4], // Advisor pair vs. knight
    QuadraticOurs[6][5], // Advisor pair vs. bishop
    QuadraticOurs[6][6], // Advisor pair vs. advisor pair
    QuadraticOurs[7][0], // Bishop pair vs. rook
    QuadraticOurs[7][1], // Bishop pair vs. advisor
    QuadraticOurs[7][2], // Bishop pair vs. cannon
    QuadraticOurs[7][3], // Bishop pair vs. pawn
    QuadraticOurs[7][4], // Bishop pair vs. knight
    QuadraticOurs[7][5], // Bishop pair vs. bishop
    QuadraticOurs[7][6], // Bishop pair vs. advisor pair
    QuadraticOurs[7][7]  // Bishop pair vs. bishop pair
);

TUNE(
    SetRange(-300,300),
    QuadraticTheirs[0][0], // Rook vs. rook
    QuadraticTheirs[1][0], // Advisor vs. rook
    QuadraticTheirs[1][1], // Advisor vs. advisor
    QuadraticTheirs[2][0], // Cannon vs. rook
    QuadraticTheirs[2][1], // Cannon vs. advisor
    QuadraticTheirs[2][2], // Cannon vs. cannon
    QuadraticTheirs[3][0], // Pawn vs. rook
    QuadraticTheirs[3][1], // Pawn vs. advisor
    QuadraticTheirs[3][2], // Pawn vs. cannon
    QuadraticTheirs[3][3], // Pawn vs. pawn
    QuadraticTheirs[4][0], // Knight vs. rook
    QuadraticTheirs[4][1], // Knight vs. advisor
    QuadraticTheirs[4][2], // Knight vs. cannon
    QuadraticTheirs[4][3], // Knight vs. pawn
    QuadraticTheirs[4][4], // Knight vs. knight
    QuadraticTheirs[5][0], // Bishop vs. rook
    QuadraticTheirs[5][1], // Bishop vs. advisor
    QuadraticTheirs[5][2], // Bishop vs. cannon
    QuadraticTheirs[5][3], // Bishop vs. pawn
    QuadraticTheirs[5][4], // Bishop vs. knight
    QuadraticTheirs[5][5], // Bishop vs. bishop
    QuadraticTheirs[6][0], // Advisor pair vs. rook
    QuadraticTheirs[6][1], // Advisor pair vs. advisor
    QuadraticTheirs[6][2], // Advisor pair vs. cannon
    QuadraticTheirs[6][3], // Advisor pair vs. pawn
    QuadraticTheirs[6][4], // Advisor pair vs. knight
    QuadraticTheirs[6][5], // Advisor pair vs. bishop
    QuadraticTheirs[6][6], // Advisor pair vs. advisor pair
    QuadraticTheirs[7][0], // Bishop pair vs. rook
    QuadraticTheirs[7][1], // Bishop pair vs. advisor
    QuadraticTheirs[7][2], // Bishop pair vs. cannon
    QuadraticTheirs[7][3], // Bishop pair vs. pawn
    QuadraticTheirs[7][4], // Bishop pair vs. knight
    QuadraticTheirs[7][5], // Bishop pair vs. bishop
    QuadraticTheirs[7][6], // Bishop pair vs. advisor pair
    QuadraticTheirs[7][7]  // Bishop pair vs. bishop pair
);



#undef S

        /// imbalance() calculates the imbalance by comparing the piece count of each
        /// piece type for both colors.

        template<Color Us>
        Score imbalance(const int pieceCount[][PIECE_TYPE_NB]) {

            constexpr Color Them = ~Us;

            Score bonus = SCORE_ZERO;

            // Second-degree polynomial material imbalance, by Tord Romstad
            for (int pt1 = NO_PIECE_TYPE; pt1 <= 7; ++pt1)
            {
                if (!pieceCount[Us][pt1])
                    continue;

                int v = QuadraticOurs[pt1][pt1] * pieceCount[Us][pt1];

                for (int pt2 = NO_PIECE_TYPE; pt2 < pt1; ++pt2)
                    v += QuadraticOurs[pt1][pt2] * pieceCount[Us][pt2]
                    + QuadraticTheirs[pt1][pt2] * pieceCount[Them][pt2];

                bonus += pieceCount[Us][pt1] * v;
            }

            return bonus;
        }

    } // namespace

    namespace Material {


        /// Material::probe() looks up the current position's material configuration in
        /// the material hash table. It returns a pointer to the Entry if the position
        /// is found. Otherwise a new Entry is computed and stored there, so we don't
        /// have to recompute all when the same material configuration occurs again.

        Entry* probe(const Position& pos) {

            Key key = pos.material_key();
            Entry* e = pos.this_thread()->materialTable[key];

            if (e->key == key)
                return e;

            std::memset(e, 0, sizeof(Entry));
            e->key = key;
            Value sum = pos.material_sum();
            const int MidgameLimit = 15258, EndgameLimit = 3915;
            e->gamePhase = Phase(((sum - EndgameLimit) * PHASE_MIDGAME) / (MidgameLimit - EndgameLimit));

            const int pieceCount[COLOR_NB][PIECE_TYPE_NB] = {
            { pos.count<ROOK>(WHITE), pos.count<ADVISOR>(WHITE), pos.count<CANNON>(WHITE),
              pos.count<PAWN>(WHITE), pos.count<KNIGHT >(WHITE), pos.count<BISHOP>(WHITE),
              pos.count<ADVISOR>(WHITE) > 1, pos.count<BISHOP>(WHITE) > 1 },
            { pos.count<ROOK>(BLACK), pos.count<ADVISOR>(BLACK), pos.count<CANNON>(BLACK),
              pos.count<PAWN>(BLACK), pos.count<KNIGHT >(BLACK), pos.count<BISHOP>(BLACK),
              pos.count<ADVISOR>(BLACK) > 1, pos.count<BISHOP>(BLACK) > 1 }
            };

            e->score = (imbalance<WHITE>(pieceCount) - imbalance<BLACK>(pieceCount)) / 16;
            return e;
        }

    } // namespace Material

} // namespace Stockfish