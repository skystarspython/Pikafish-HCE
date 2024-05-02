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

template<>
Value Endgame<KAABBKR>::operator()(const Position& pos) const {

    assert(!pos.checkers()); // Eval is never called when in check
    Value v = Value(32); // Default value of KAABBKR
    const Square ksq = pos.square<KING>(weakSide);
    const Square strong_ksq = pos.square<KING>(strongSide);
    const Square rookSq = pos.square<ROOK>(strongSide);

    constexpr Bitboard left = (FileABB | FileBBB | FileCBB | FileDBB);
    constexpr Bitboard right = (FileFBB | FileGBB | FileHBB | FileIBB);

    Bitboard rookAttacks = attacks_bb<ROOK>(rookSq);
    Bitboard advisorAttacks = 0;
    Bitboard advisorBB = pos.pieces(weakSide, ADVISOR) & ~pos.blockers_for_king(weakSide);
    while (advisorBB) {
        Square s = pop_lsb(advisorBB);
        advisorAttacks |= attacks_bb<ADVISOR>(s);
    }
    Bitboard bishopAttacks = 0;
    Bitboard bishopBB = pos.pieces(weakSide, BISHOP) & ~pos.blockers_for_king(weakSide);
    while (bishopBB) {
        Square s = pop_lsb(bishopBB);
        bishopAttacks |= attacks_bb<BISHOP>(s, pos.pieces(strongSide, ROOK));
    }
    Bitboard kingSide = ksq & left ? left : ksq & right ? right : FileEBB;
    if (relative_rank(weakSide, ksq) == RANK_1
        && file_of(ksq) == file_of(strong_ksq) && (between_bb(ksq, strong_ksq) & pos.pieces(weakSide, ADVISOR))
        && (rookSq & (FileHBB | FileBBB))
        && (rookSq & kingSide)) {
        v = Value(2048);
        return v;
    }
    else if ((rookAttacks & pos.pieces(weakSide, ADVISOR) & ~advisorAttacks)
        || (rookAttacks & pos.pieces(weakSide, BISHOP) & ~bishopAttacks)) {
        v = Value(2048);
        return v;
    }
    else if (rookSq & (advisorAttacks | bishopAttacks)) {
        v = Value(-32);
        return v;
    }
    bool normalAdvisor = popcount(pos.pieces(weakSide, ADVISOR) & (Rank0BB | Rank9BB)) == 1 &&
        popcount(pos.pieces(weakSide, ADVISOR) & (Rank1BB | Rank8BB)) == 1;
    bool normalBishop = popcount(pos.pieces(weakSide, BISHOP) & (Rank0BB | Rank9BB)) == 1 &&
        popcount(pos.pieces(weakSide, BISHOP) & (Rank2BB | Rank7BB)) == 1;
    bool normalKing = ksq & (Rank0BB | Rank9BB);
    if (normalAdvisor && normalBishop) {
        v = Value(16);
    }
    else if (rookSq & (FileHBB | FileBBB)) {
        if (relative_rank(weakSide, ksq) == RANK_1) {
            v = Value(128);
            if (advisorAttacks & pos.pieces(weakSide, ADVISOR) & (Rank2BB | Rank7BB)) {
                v = (pos.pieces(weakSide, BISHOP) & kingSide & (Rank0BB | Rank9BB)) ? Value(16) : Value(1024);
            }
        }
        else
            v = Value(64);
    }

    return v;
}

template<>
Value Endgame<KPKP>::operator()(const Position& pos) const {

    assert(!pos.checkers()); // Eval is never called when in check
    Value v[COLOR_NB] = { VALUE_ZERO, VALUE_ZERO };


    for (Color Us = WHITE; Us <= BLACK; Us = Color(Us + 1)) {
        Bitboard ourKingFileBB = file_bb(file_of(pos.square<KING>(Us)));
        Bitboard kingAttacks = (attacks_bb<KING>(pos.square<KING>(~Us)) | ourKingFileBB) ^ ourKingFileBB;
        if (pos.pieces(Us, PAWN) & kingAttacks)
            v[Us] -= Value(2048);
    }

    if (v[WHITE] - v[BLACK] == 2048 && pos.side_to_move() == BLACK)
        return VALUE_ZERO;
    if (v[WHITE] - v[BLACK] == -2048 && pos.side_to_move() == WHITE)
        return VALUE_ZERO;

    return v[WHITE] - v[BLACK];
}

template<>
Value Endgame<KBKN>::operator()(const Position& pos) const {

    assert(!pos.checkers()); // Eval is never called when in check
    Value v = Value(32);
    const Square ksq = pos.square<KING>(strongSide);
    Rank BishopRank = rank_of(pos.square<BISHOP>(weakSide));
    Rank KnightRank = rank_of(pos.square<KNIGHT>(strongSide));
    File BishopFile = file_of(pos.square<BISHOP>(weakSide));
    File KnightFile = file_of(pos.square<KNIGHT>(strongSide));
    constexpr Bitboard left = (FileABB | FileBBB | FileCBB | FileDBB);
    constexpr Bitboard right = (FileFBB | FileGBB | FileHBB | FileIBB);
    Bitboard kingSide = ksq & left ? left : ksq & right ? right : FileEBB;
    if (BishopRank == KnightRank && (BishopRank == RANK_2 || BishopRank == RANK_7)
        && BishopFile != FILE_E && abs(KnightFile - BishopFile) == 1 &&
        !(kingSide & pos.pieces(weakSide, BISHOP))
        && popcount(attacks_bb<ROOK>(ksq) & file_bb(file_of(ksq))) == 9) {
        v = Value(2048);
    }
    return v;
}

template<>
Value Endgame<INSUFFICIENT_MATERIAL>::operator()(const Position& pos) const {

    assert(!pos.checkers()); // Eval is never called when in check

    if (pos.material_diff() == 0)
        return VALUE_ZERO;

    return Value(16);
}

} // namespace Stockfish
