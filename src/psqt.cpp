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
   { S(-203,-131), S(  46,-225), S(-147, -86), S( -17,   5), S(   8, -13)},
   { S(-203, -52), S(  58, -67), S( -89,-110), S( -78, 121), S(-106, 106)},
   { S(-138, -61), S(   7, -96), S( -65,  -9), S(-110,   7), S(  -8,  45)},
   { S( -60,  77), S( -48, -33), S( -58,  61), S(  88,  54), S( 175,  -4)},
   { S( -61,  32), S(  42,  -7), S(-112,  34), S( 181,  13), S(-170,  14)},
   { S( -69, -12), S( 192,  24), S(  88, -76), S(  53, -74), S( 110,  86)},
   { S(-199, 103), S(  -8, -85), S( 179, -39), S(  48,  23), S(  12,  79)},
   { S( 139, 130), S(  20,-149), S(  95, 113), S(  92, 101), S( -20, -69)},
   { S( -72, -15), S( 163, -21), S( 124, -79), S(  32,  46), S( -78,-100)},
   { S( 109, -97), S(  66, -29), S( -86,  -4), S(  39,  55), S(  22,  54)}
  },
  { // ADVISOR
   { S(   0,   0), S(   0,   0), S(   0,   0), S(  41,  33), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(  38, 113)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(-152,  47), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)}
  },
  { // CANNON
   { S(  -4,  15), S( -42, -59), S( -44, -53), S(  61, 124), S(   4,  34)},
   { S(  38,   6), S( 141, -34), S( -63,  22), S(  -1,  37), S( 113,  41)},
   { S(  21, -24), S(  72, -11), S(  58,  82), S( 104,  60), S( 212,  42)},
   { S(  35, 106), S(-194, -36), S( 112,  97), S( 102,-151), S(  -2, -43)},
   { S( -40, -30), S( -56,  78), S( -82,  32), S(-113, 136), S( 246,   6)},
   { S( -66,  13), S(  66,-102), S(   2,  40), S(  -7,  34), S(  79, 112)},
   { S(  51,-196), S( 100, -46), S(  20, -34), S(   1,  52), S(  48, 163)},
   { S( 149,-165), S( -13,  84), S(  -2,   9), S(  67,-107), S( 180,  58)},
   { S( -48, 100), S(  55, -17), S(  -2,  16), S( -42, -91), S(  88,  51)},
   { S( 135, -53), S( 225, -12), S( -15,  26), S( 189, 144), S(  13,  12)}
  },
  { // PAWN
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S( -27, -19), S(   0,   0), S( -19, -27), S(   0,   0), S(  56,  34)},
   { S( -12, -18), S(   0,   0), S(  41,  -2), S(   0,   0), S(  39,  61)},
   { S( -28,  70), S( -53,  93), S( 138,  46), S(  26, 197), S(  73,  52)},
   { S( -74,  69), S(-145,  80), S(  53, 161), S(   2, 210), S( -65,  22)},
   { S(  91, -92), S(-120, 104), S(  18,  41), S(  19,  46), S(-121, -67)},
   { S(-181,-148), S(  50,  56), S( 182,  34), S(  11,  41), S(  62,  17)},
   { S( 116, -57), S(  85, -85), S( -84,   3), S(  35, -33), S(  19,-119)}
  },
  { // KNIGHT
   { S( -25, -48), S(-180,-201), S( -30, -32), S(-112,-133), S( -99,  56)},
   { S(-126, -95), S( -93,  59), S(-142, -26), S( -37, -82), S( -64,-136)},
   { S( -82, -88), S(  43, -51), S( -35,-109), S(  11,  54), S(  -4, -16)},
   { S(  25,   7), S( -86,-111), S(  82, -30), S( 172, -90), S( -36, 101)},
   { S(-154,  35), S( -58,  68), S(   5,  89), S(  26, -50), S( 103,  56)},
   { S( 117, -34), S(  70,  66), S(  43, -50), S( 151,  74), S( -53, 110)},
   { S( -62, -72), S(  62,  47), S( 170,  63), S(  26,  34), S( -74,  -2)},
   { S( 122, -69), S( -69,-134), S(   4,  25), S(  78, 151), S(   1, 198)},
   { S(-107, -19), S( -69, -57), S( -11, 100), S( -64, -74), S( 187, 125)},
   { S( -53,  20), S(  12, 139), S(  30, -12), S(-139, -79), S( -65,  25)}
  },
  { // BISHOP
   { S(   0,   0), S(   0,   0), S( 111, 129), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(  18, 105), S(   0,   0), S(   0,   0), S(   0,   0), S( 206, 148)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(  -4, 102), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)}
  },
  { // KING
   { S(   0,   0), S(   0,   0), S(   0,   0), S( -74,  42), S(  63,  65)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(-221, -16), S( -71,  63)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   1,-167), S( -36, -59)},
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
