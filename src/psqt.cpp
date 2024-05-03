/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2023 The Stockfish developers (see AUTHORS file)

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


#include "psqt.h"

#include <algorithm>

#include "bitboard.h"
#include "types.h"

namespace Stockfish {

namespace
{

auto constexpr S = make_score;

// 'Bonus' contains Piece-Square parameters.
// Scores are explicit for files A to E, implicitly mirrored for E to I.
constexpr Score Bonus[][RANK_NB][int(FILE_NB) / 2 + 1] = {
  { },
  { // ROOK
   { S(-218,-132), S(  44,-242), S( 113, -73), S( 331,  -5), S(-407, -45)},
   { S(   1, -25), S(   6, -45), S( -71,-123), S( 196, 122), S(-107, 101)},
   { S(-157, -20), S(   5, -83), S( -39,  47), S(   2,   6), S( -20,  46)},
   { S( -31,  77), S( -14, -53), S( -71,  50), S(   9,  49), S( -66, -20)},
   { S( -91,  27), S(  28,  -6), S(-114,  19), S(   8,  18), S(-165,  -6)},
   { S( -41,  -7), S(  37,  30), S(  86, -55), S(  80, -84), S( 118,  65)},
   { S(-148, 135), S( 210, -78), S( 354, -29), S( 451,  11), S( 122,  78)},
   { S( 145, 150), S(  53,-137), S(  92, 122), S( 288,  93), S( 252, -38)},
   { S( -46, -43), S(  80, -10), S( 127, -41), S( 529, 135), S( -77, 518)},
   { S( 116, -64), S(  67, -68), S(  77,   0), S( 118,  58), S( 310,  34)}
  },
  { // ADVISOR
   { S(   0,   0), S(   0,   0), S(   0,   0), S(  -2,  44), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(  92, 119)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(-115,  75), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)}
  },
  { // CANNON
   { S(-121,   0), S( -62, -69), S( -30, -64), S(  80,  90), S( -37,  -9)},
   { S( 126,  -5), S(  45, -41), S( 398,   7), S(  78,  22), S(  99,  29)},
   { S( 134, -62), S(  57, -17), S( 356,  93), S(  79,  88), S( 306,  66)},
   { S(  -5,  77), S(  67, -20), S( 117,  84), S(  76,-153), S( 119, -66)},
   { S( -24, -27), S(-113,  48), S(   5,  11), S( -93, 102), S( 358, -35)},
   { S( -68,  -4), S(  31, -87), S(   5,  39), S( -11,  19), S(  92,  76)},
   { S(  47,-211), S(  70, -84), S( 108, -43), S(  21,  62), S( 354, 156)},
   { S( -20,-133), S( -37, 133), S( 212, -23), S(  57, -99), S( 351,  69)},
   { S( -28, 105), S( -54,   3), S(  20,  18), S( -64, -85), S( 161,  66)},
   { S( 289, -49), S( 225, -15), S( 320,  20), S(   3, 114), S(-318,  -3)}
  },
  { // PAWN
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(  -8,   1), S(   0,   0), S(   6, -32), S(   0,   0), S(  50,  30)},
   { S(  95,   8), S(   0,   0), S( 307, -17), S(   0,   0), S( 182,  31)},
   { S( 119,  93), S( 157,  91), S( 153, 208), S( 230, 199), S( 276,  52)},
   { S(  80,  64), S(-160, 102), S( 335, 178), S( 301, 196), S( 197, 120)},
   { S( -88, -87), S(-133,  83), S(  26, 216), S( 219,  33), S( 232, 165)},
   { S(-176,-149), S(  35,  42), S( 191,  23), S( 500, 150), S( 774, 300)},
   { S( -99, -54), S(  82, -95), S( -79,  10), S( 228, -19), S( 127,-100)}
  },
  { // KNIGHT
   { S( -18, -23), S( -83,-252), S( -18, -38), S(  36,-152), S( -81,  43)},
   { S(  26, -87), S(-100,  78), S(-133, -16), S( 125, -97), S(-445, -18)},
   { S( 138,-119), S(  50, -69), S( 231, -91), S( 121,  16), S( 214,   8)},
   { S(  25,   7), S( -72,-116), S( 220,  -7), S(   4, -87), S(  81,  87)},
   { S(-123,  15), S( 224, 129), S( -22,  88), S( 259,-103), S(  69,  61)},
   { S( 119, -57), S(  85,  73), S(  60, -69), S( 257,  79), S( -72, 123)},
   { S( -44, -61), S( 372,  34), S( 168,  60), S(  25,  21), S( -90,   7)},
   { S(  97, -54), S( -87,-122), S( 124, 124), S( 178, 141), S( -29, 194)},
   { S(-102, -45), S( 383, -44), S( 435, 158), S( -59, -66), S( 155, 121)},
   { S( 128,  31), S(  23, 139), S(  22,   7), S(-132, -75), S( -58,  45)}
  },
  { // BISHOP
   { S(   0,   0), S(   0,   0), S( -58, 134), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S( -92,  81), S(   0,   0), S(   0,   0), S(   0,   0), S( 206, 159)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(-113,  86), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)}
  },
  { // KING
   { S(   0,   0), S(   0,   0), S(   0,   0), S( 105, 336), S( 145,  90)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   6,  -3), S(-250,  80)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(-232,-131), S(-304, -66)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)}
  }
};

} // namespace


namespace PSQT
{
Score psq[PIECE_NB][SQUARE_NB];

// PSQT::init() initializes piece-square tables: the white halves of the tables are
// copied from Bonus[] and PBonus[], adding the piece value, then the black halves of
// the tables are initialized by flipping and changing the sign of the white scores.
void init() {

  for (Piece pc : {W_ROOK, W_ADVISOR, W_CANNON, W_PAWN, W_KNIGHT, W_BISHOP, W_KING})
  {
    Score score = make_score(PieceValue[MG][pc], PieceValue[EG][pc]);

    for (Square s = SQ_A0; s <= SQ_I9; ++s)
    {
      File f = File(edge_distance(file_of(s)));
      if (f > FILE_E) --f;
      psq[pc  ][s] = score + Bonus[pc][rank_of(s)][f];
      psq[pc+8][flip_rank(s)] = -psq[pc][s];
    }
  }
}
} // namespace PSQT

} // namespace Stockfish
