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
#include <iostream>

#include "bitboard.h"
#include "endgame.h"
#include "movegen.h"

namespace Stockfish {

namespace {

  // Map the square as if strongSide is white and strongSide's only pawn
  // is on the left half of the board.
  Square normalize(const Position& pos, Color strongSide, Square sq) {

    assert(pos.count<PAWN>(strongSide) == 1);

    if (file_of(pos.square<PAWN>(strongSide)) >= FILE_E)
        sq = flip_file(sq);

    return strongSide == WHITE ? sq : flip_rank(sq);
  }

} // namespace

int kaabbkr = 32;
TUNE(SetRange(0, 600), kaabbkr);

template<>
Value Endgame<KAABBKR>::operator()(const Position& pos) const {

    assert(!pos.checkers()); // Eval is never called when in check
    Value v = Value(kaabbkr); // Default value of KAABBKR
    const Square ksq = pos.square<KING>(weakSide);
    const Square strong_ksq = pos.square<KING>(strongSide);
    const Square rookSq = pos.square<ROOK>(strongSide);

    constexpr Bitboard left = (FileABB | FileBBB | FileCBB | FileDBB);
    constexpr Bitboard right = (FileFBB | FileGBB | FileHBB | FileIBB);


    if (relative_rank(weakSide, ksq) == RANK_1
        && file_of(ksq) == file_of(strong_ksq) && (between_bb(ksq, strong_ksq) & pos.pieces(weakSide, ADVISOR))
        && (rookSq & (FileHBB | FileBBB))
        && ((rookSq & left && ksq & left) ||
            (rookSq & right && ksq & right))) {
        v = Value(VALUE_MATE_IN_MAX_PLY - 1);
    }

    return v;
}

} // namespace Stockfish
