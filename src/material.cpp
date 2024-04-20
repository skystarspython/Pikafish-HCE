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




#undef S

        /// imbalance() calculates the imbalance by comparing the piece count of each
        /// piece type for both colors.

        template<Color Us>
        Score imbalance(const int pieceCount[][PIECE_TYPE_NB]) {

            return SCORE_ZERO;
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