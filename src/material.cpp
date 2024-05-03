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
#include "endgame.h"

using namespace std;

namespace Stockfish {

    namespace {
#define S(mg, eg) make_score(mg, eg)

        // Polynomial material imbalance parameters

        // One Score parameter for each pair (our piece, another of our pieces)
        constexpr Score QuadraticOurs[][PIECE_TYPE_NB] = {
            // OUR PIECE 2
            // rook   advisor  cannon   pawn     knight    bishop
            {S(-191, 23)                                             }, // Rook
            {S(24, 74), S(244, -67)                                }, // Advisor
            {S(48, 72), S(133, 62), S(315, -63)                     }, // Cannon      OUR PIECE 1
            {S(75, -14), S(31, 44), S(113, 28), S(111, 11)         }, // Pawn
            {S(-92, 53), S(27, -9), S(513, 234), S(244, 88), S(-30, -29)}, // Knight
            {S(54, 104), S(275, -103), S(-6, -64), S(3, -113), S(24, 6), S(2, -59)}  // Bishop
        };

        // One Score parameter for each pair (our piece, their piece)
        constexpr Score QuadraticTheirs[][PIECE_TYPE_NB] = {
            // THEIR PIECE
            // rook   advisor  cannon   pawn     knight    bishop
            {S(-135, -46)                                           }, // Rook
            {S(-92, 32), S(138, -7)                                }, // Advisor
            {S(-83, 13), S(-41, 43), S(-220, 28)                     }, // Cannon      OUR PIECE
            {S(-2, 13), S(-57, -118), S(-18, 121), S(70, -58)      }, // Pawn
            {S(-37, 17), S(14, -86), S(38, -24), S(567, 43), S(-21, -42) }, // Knight
            {S(72, 38), S(-111, -79), S(24, -2), S(48, 30), S(30, 14), S(51, 35) }  // Bishop
        };

#undef S

        // Endgame evaluation and scaling functions are accessed directly and not through
        // the function maps because they correspond to more than one material hash key.
        Endgame<KAABBKR>    EvaluateKAABBKR[] = { Endgame<KAABBKR>(WHITE),    Endgame<KAABBKR>(BLACK) };
        Endgame<KPKP>       EvaluateKPKP[] =    { Endgame<KPKP>(WHITE),       Endgame<KPKP>(WHITE) };
        Endgame<KBKN>       EvaluateKBKN[] =    { Endgame<KBKN>(WHITE),       Endgame<KBKN>(WHITE) };
        Endgame<INSUFFICIENT_MATERIAL> EvaluateIM[] = { Endgame<INSUFFICIENT_MATERIAL>(WHITE), Endgame<INSUFFICIENT_MATERIAL>(BLACK) };

        // Helper used to detect a given material distribution
        // 车(任意士象) vs 士象全
        bool is_KAABBKR(const Position& pos, Color us) {
            return pos.material(~us) == AdvisorValueMg * 2 + BishopValueMg * 2
                && pos.material(us) >= RookValueMg
                && pos.count<ALL_PIECES>(us) == pos.count<ROOK>(us) + pos.count<ADVISOR>(us) + pos.count<BISHOP>(us) + 1;
        }

        // 兵 vs 卒
        bool is_KPKP(const Position& pos, Color us) {
            return us == WHITE && pos.material(us) == PawnValueMg && pos.material(~us) == PawnValueMg;
        }

        // 马(任意士象) vs 象
        bool is_KBKN(const Position& pos, Color us) {
            return pos.material(~us) == BishopValueMg && pos.material(us) >= KnightValueMg &&
                pos.count<ALL_PIECES>(us) == pos.count<KNIGHT>(us) + pos.count<ADVISOR>(us) + pos.count<BISHOP>(us) + 1;
        }

        // 大子
        Value major_material(const Position& pos) {
            return pos.count<KNIGHT>() * KnightValueMg + pos.count<ROOK>() * RookValueMg + pos.count<CANNON>() * CannonValueMg;
        }

        /// imbalance() calculates the imbalance by comparing the piece count of each
        /// piece type for both colors.
        // 子力平衡
        template<Color Us>
        Score imbalance(const int pieceCount[][PIECE_TYPE_NB]) {

            constexpr Color Them = ~Us;

            Score bonus = SCORE_ZERO;

            // Second-degree polynomial material imbalance, by Tord Romstad
            for (int pt1 = NO_PIECE_TYPE; pt1 < BISHOP; ++pt1)
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

            for (Color c : { WHITE, BLACK }) {
                if (is_KAABBKR(pos, c))
                {
                    e->evaluationFunction = &EvaluateKAABBKR[c];
                    return e;
                }
                if (is_KPKP(pos, c))
                {
                    e->evaluationFunction = &EvaluateKPKP[c];
                    return e;
                }
                if (is_KBKN(pos, c))
                {
                    e->evaluationFunction = &EvaluateKBKN[c];
                    return e;
                }
            }

            // Draw by insufficient material
            if ([&] {
                        if (pos.count<PAWN>() == 0)
                        {
                            // No attacking pieces left
                            if (!major_material(pos))
                                return true;

                            // Only one cannon left on the board
                            if (major_material(pos) == CannonValueMg)
                            {
                                // No advisors left on the board
                                if (pos.count<ADVISOR>() == 0)
                                    return true;

                                // The side not holding the cannon can possess one advisor
                                // The side holding the cannon should only have cannon
                                if ((pos.count<ALL_PIECES>(WHITE) == 2 && pos.count<CANNON>(WHITE) == 1
                                    && pos.count<ADVISOR>(BLACK) == 1)
                                    || (pos.count<ALL_PIECES>(BLACK) == 2 && pos.count<CANNON>(BLACK) == 1
                                        && pos.count<ADVISOR>(WHITE) == 1))
                                    return true;
                            }

                            // Two cannons left on the board, one for each side, but no other pieces left on the board
                            if (pos.count<ALL_PIECES>() == 4 && pos.count<CANNON>(WHITE) == 1
                                && pos.count<CANNON>(BLACK) == 1)
                                return true;
                        }

                        return false;
                    }())
            {
                Color strongSide = pos.material_diff() > 0 ? pos.side_to_move() : ~pos.side_to_move();
                e->evaluationFunction = &EvaluateIM[strongSide];
                return e;
            }

            const int pieceCount[COLOR_NB][PIECE_TYPE_NB] = {
            { pos.count<ROOK>(WHITE), pos.count<ADVISOR>(WHITE), pos.count<CANNON>(WHITE),
              pos.count<PAWN>(WHITE), pos.count<KNIGHT >(WHITE), pos.count<BISHOP>(WHITE) },
            { pos.count<ROOK>(BLACK), pos.count<ADVISOR>(BLACK), pos.count<CANNON>(BLACK),
              pos.count<PAWN>(BLACK), pos.count<KNIGHT >(BLACK), pos.count<BISHOP>(BLACK) }
            };

            e->score = (imbalance<WHITE>(pieceCount) - imbalance<BLACK>(pieceCount)) / 16;
            return e;
        }

    } // namespace Material

} // namespace Stockfish
