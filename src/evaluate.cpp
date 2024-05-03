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

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>   // For std::memset
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <streambuf>
#include <vector>
#include <random>

#include "bitboard.h"
#include "evaluate.h"
#include "misc.h"
#include "thread.h"
#include "uci.h"
#include "material.h"

using namespace std;

namespace Stockfish {

namespace Trace {

    enum Tracing { NO_TRACE, TRACE };

    enum Term { // The first 8 entries are reserved for PieceType
        MATERIAL = 8, IMBALANCE, PAIR, MOBILITY, THREAT, PIECES, WINNABLE, TOTAL, TERM_NB
    };

    Score scores[TERM_NB][COLOR_NB];

    double to_cp(Value v) { return double(v) / PawnValueEg; }

    /*
    // unused function
    static void add(int idx, Color c, Score s) {
        scores[idx][c] = s;
    }*/

    static void add(int idx, Score w, Score b = SCORE_ZERO) {
        scores[idx][WHITE] = w;
        scores[idx][BLACK] = b;
    }

    static std::ostream& operator<<(std::ostream& os, Score s) {
        os << std::setw(5) << to_cp(mg_value(s)) << " "
            << std::setw(5) << to_cp(eg_value(s));
        return os;
    }

    static std::ostream& operator<<(std::ostream& os, Term t) {

        if (t == MATERIAL || t == IMBALANCE || t == WINNABLE || t == TOTAL)
            os << " ----  ----" << " | " << " ----  ----";
        else
            os << scores[t][WHITE] << " | " << scores[t][BLACK];

        os << " | " << scores[t][WHITE] - scores[t][BLACK] << " |\n";
        return os;
    }
}

int generateRandomNumber(int range) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(-range, range);

    return distrib(gen);
}

using namespace Trace;

namespace {

#define S(mg, eg) make_score(mg, eg)
    constexpr Score HollowCannon = S(1285, 201);
    constexpr Score CentralKnight = S(1800, 1500);
    constexpr Score BottomCannon = S(418, 108);
    constexpr Score IronBolt = S(400, 200);
    constexpr Score PinnedRook = S(-600, -800);
    constexpr Score AdvisorBishopPair = S(204, 243);
    constexpr Score CrossedPawn[3][6] = {
        { S(-248, -40), S(116, 224), S(211, 317), S(429, 527), S(619, 651), S(814, 877) },
        { S(-116, -35), S(100, 202), S(159, 200), S(306, 400), S(414, 500), S(636, 713) },
        { S(-17, 5), S(60, -8), S(132, 111), S(222, 300), S(310, 400), S(440, 530) }
    };
    constexpr Score ConnectedPawn = S(205, 105);
    constexpr Score RookOnOpenFile[2] = { S(11, 111), S(214, 161) };
    constexpr Score PiecesOnOneSide[5] = { S(-3, 5), S(50, 36), S(218, 126), S(819, 126), S(1520, 314) };
    constexpr Score mobilityBonus[PIECE_TYPE_NB][18] = {
        {}, // NO_PIECE_TYPE
        {S(-19555, -19045), S(-14423, -14895), S(-12344, -12170), S(-10229, -10012), S(-8867, -5183), S(-6197, -3787), S(-4637, -2581), S(-2577, -1604), S(-412, -371), S(2254, 1076), S(4018, 2178), S(6029, 3946), S(8410, 4079), S(9004, 5200), S(10081, 6500), S(11035, 7212), S(11433, 8483), S(19686, 9329)}, // ROOK
        {S(-6686, -4329), S(4461, 745), S(4657, 329), S(5853, 1929), S(9140, 275)}, // ADVISOR
        {S(-19672, -11629), S(-15310, -11438), S(-13199, -1728), S(-9840, 456), S(-1115, 3069), S(1085, 4029), S(1724, 5532), S(2894, 6181), S(3461, 7018), S(4461, 8196), S(5398, 9107), S(8568, 10268), S(10408, 11591), S(11933, 12727), S(13036, 13283), S(14008, 14094), S(15000, 15741), S(15309, 16672)}, // CANNON
        {}, // PAWN
        {S(-20582, -5894), S(2260, -2360), S(4002, -2435), S(8595, 1090), S(10389, 2949), S(15760, 3209), S(17500, 3453), S(19956, 6472), S(23619, 7657)}, // KNIGHT
        {S(-11692, -2811), S(911, -1898), S(3017, -904), S(7134, 1537), S(9276, -1351)}, // BISHOP
    };
#undef S

    // Evaluation class computes and stores attacks tables and other working data
    template<Tracing T>
    class Evaluation {

    public:
        Evaluation() = delete;
        explicit Evaluation(const Position& p) : pos(p) {}
        Evaluation& operator=(const Evaluation&) = delete;
        Value value();

    private:
        template<Color Us> void initialize();
        template<Color Us, PieceType Pt> Score pieces();
        template<Color Us> Score threat();
        Value winnable(Score score) const;

        const Position& pos;
        Material::Entry* me;

        // attackedBy[color][piece type] is a bitboard representing all squares
        // attacked by a given color and piece type. Special "piece types" which
        // is also calculated is ALL_PIECES.
        Bitboard attackedBy[COLOR_NB][PIECE_TYPE_NB];

        // attackedBy2[color] are the squares attacked by at least 2 units of a given
        // color, including x-rays. But diagonal x-rays through pawns are not computed.
        Bitboard attackedBy2[COLOR_NB];

        Score mobility[COLOR_NB] = { SCORE_ZERO, SCORE_ZERO };
    };


    // Evaluation::initialize() computes king and pawn attacks, and the king ring
    // bitboard for a given color. This is done at the beginning of the evaluation.

    template<Tracing T> template<Color Us>
    void Evaluation<T>::initialize() {
        const Square ksq = pos.square<KING>(Us);

        // Initialize attackedBy[] for king and pawns
        attackedBy[Us][KING] = attacks_bb<KING>(ksq);
        attackedBy[Us][PAWN] = pawn_attacks_bb<Us>(pos.pieces(Us, PAWN));
        attackedBy[Us][ALL_PIECES] = attackedBy[Us][KING] | attackedBy[Us][PAWN];
        attackedBy2[Us] = attackedBy[Us][KING] & attackedBy[Us][PAWN];
    }


    // Evaluation::pieces() scores pieces of a given color and type

    template<Tracing T> template<Color Us, PieceType Pt>
    Score Evaluation<T>::pieces() {

        constexpr Color Them = ~Us;
        const Square ksq = pos.square<KING>(Them);
        Bitboard b1 = pos.pieces(Us, Pt);
        Bitboard b;
        Score score = SCORE_ZERO;

        attackedBy[Us][Pt] = 0;

        while (b1)
        {
            Square s = pop_lsb(b1);

            // Find attacked squares, including x-ray attacks for bishops and rooks
            b = attacks_bb<Pt>(s, pos.pieces());

            if (pos.blockers_for_king(Us) & s)
                b &= line_bb(pos.square<KING>(Us), s);

            attackedBy2[Us] |= attackedBy[Us][ALL_PIECES] & b;
            attackedBy[Us][Pt] |= b;
            attackedBy[Us][ALL_PIECES] |= b;

            int mob = popcount(b & ~attackedBy[Them][PAWN]);
            mobility[Us] += mobilityBonus[Pt][mob];

            if constexpr (Pt == CANNON) { // 炮的评估
                int blockerCount = popcount(between_bb(s, ksq) & pos.pieces()) - 1;
                constexpr Bitboard originalAdvisor = ((FileDBB | FileFBB) & (Rank0BB | Rank9BB));
                Bitboard advisorBB = pos.pieces(Them, ADVISOR);
                if (file_of(s) == FILE_E && (ksq == SQ_E0 || ksq == SQ_E9)) {
                    if (popcount(originalAdvisor & advisorBB) == 2) {
                        if (!blockerCount) { // 空头炮
                            score += HollowCannon;
                        }
                        if (blockerCount == 2 && (between_bb(s, ksq) & pos.pieces(Them, KNIGHT) & attackedBy[Them][KING])) { // 炮镇窝心马
                            score += CentralKnight;
                        }
                    }
                    else if (blockerCount == 2 && pos.count<ADVISOR>(Them) + pos.count<BISHOP>(Them) == 4 // 铁门栓
                        && popcount(between_bb(s, ksq) & pos.pieces(Them, ADVISOR, BISHOP)) == 2) {
                        score += IronBolt;
                    }
                }
                Rank enemyBottom = (Us == WHITE ? RANK_9 : RANK_0);
                Square enemyCenter = (Us == WHITE ? SQ_E8 : SQ_E1);
                if (rank_of(s) == enemyBottom && !blockerCount && (ksq == SQ_E0 || ksq == SQ_E9)
                    && (pos.pieces(Them) & enemyCenter)) { // 沉底炮
                    score += BottomCannon;
                }
            }
            if constexpr (Pt == ROOK)
            {
                if (pos.is_on_semiopen_file(Us, s))
                    score += RookOnOpenFile[pos.is_on_semiopen_file(Them, s)];
                Bitboard enemyRooks = pos.pieces(Them, ROOK);
                Bitboard ourRookFileRank = file_bb(file_of(s)) | rank_bb(rank_of(s));
                if ((s & ~attackedBy[Us][ALL_PIECES]) && !(attacks_bb<ROOK>(s, pos.pieces()) & pos.pieces(Us, ROOK))) {
                    enemyRooks = enemyRooks & ourRookFileRank;
                    while (enemyRooks) {
                        Square enemyRookSq = pop_lsb(enemyRooks);
                        int blockerCount = popcount(between_bb(s, enemyRookSq) & pos.pieces()) - 1;
                        if (blockerCount != 1)
                            break;
                        Bitboard weak = between_bb(s, enemyRookSq) & ~attackedBy[Us][ALL_PIECES];
                        Bitboard knightBB = pos.pieces(Us, KNIGHT) & weak;
                        Bitboard cannonBB = pos.pieces(Us, CANNON) & weak;
                        if (knightBB | cannonBB) {
                            if (knightBB) {
                                Square knightSq = lsb(knightBB);
                                Bitboard knightAttacks = attacks_bb<KNIGHT>(knightSq, pos.pieces());
                                if (knightAttacks & attacks_bb<KNIGHT_TO>(s, pos.pieces()))
                                    break;
                            }
                            score += PinnedRook;
                        }
                    }
                }
            }
        }
        return score;
    }

    template<Tracing T> template<Color Us>
    Score Evaluation<T>::threat() {
        Score score = SCORE_ZERO; // 初始化
        constexpr Color Them = ~Us;
        // 士象全
        if (pos.count<ADVISOR>(Us) + pos.count<BISHOP>(Us) == 4)
            score += AdvisorBishopPair;
        // 过河兵
        constexpr Bitboard crossedWithoutBottom = (Us == WHITE ? (Rank5BB | Rank6BB | Rank7BB | Rank8BB) : (Rank1BB | Rank2BB | Rank3BB | Rank4BB)); // 底线不算
        int crossedPawnCnt = popcount(crossedWithoutBottom & pos.pieces(Us, PAWN));
        score += CrossedPawn[pos.count<ADVISOR>(Them)][crossedPawnCnt];
        // 牵手兵
        score += ConnectedPawn * popcount(shift<EAST>(pos.pieces(Us, PAWN)) & pos.pieces(Us, PAWN));
        constexpr Bitboard crossed = (Us == WHITE ? (Rank5BB | Rank6BB | Rank7BB | Rank8BB | Rank9BB) : (Rank0BB | Rank1BB | Rank2BB | Rank3BB | Rank4BB));
        constexpr Bitboard left = (FileABB | FileBBB | FileCBB | FileDBB);
        constexpr Bitboard right = (FileFBB | FileGBB | FileHBB | FileIBB);
        Bitboard strongPieces = pos.pieces(Us, ROOK) | pos.pieces(Us, KNIGHT) | pos.pieces(Us, CANNON);
        Bitboard attackedPieces = attackedBy[Them][PAWN] | attackedBy[Them][ADVISOR] | attackedBy[Them][BISHOP]
            | attackedBy[Them][CANNON] | attackedBy[Them][KNIGHT] | (attackedBy[Them][ROOK] & ~attackedBy[Us][ALL_PIECES]);
        // 多子归边
        for (int i = 0; i <= 1; i++) {
            Bitboard side = (i == 0 ? left : right);
            int cnt = popcount(strongPieces & side & crossed & (~attackedPieces));
            cnt = cnt >= 5 ? 4 : cnt;
            score += PiecesOnOneSide[cnt];
        }
        return score;
    }


    // Evaluation::winnable() adjusts the midgame and endgame score components, based on
    // the known attacking/defending status of the players. The final value is derived
    // by interpolation from the midgame and endgame values.

    template<Tracing T>
    Value Evaluation<T>::winnable(Score score) const {
        int gamePhase = me->game_phase();
        Value mg = mg_value(score), eg = eg_value(score);
        Value v = mg * int(gamePhase)
            + eg * int(128 - gamePhase);
        v /= 128;
        return v;
    }


    // Evaluation::value() is the main function of the class. It computes the various
    // parts of the evaluation and returns the value of the position from the point
    // of view of the side to move.

    template<Tracing T>
    Value Evaluation<T>::value() {

        assert(!pos.checkers());

        // Probe the material hash table
        me = Material::probe(pos);

        // If we have a specialized evaluation function for the current material
        // configuration, call it and return.
        if (me->specialized_eval_exists())
            return me->evaluate(pos);

        Score score = pos.psq_score() + me->imbalance();

        if constexpr (T) {
            Trace::add(MATERIAL, pos.psq_score());
            Trace::add(IMBALANCE, me->imbalance());
        }

        // Main evaluation begins here
        initialize<WHITE>();
        initialize<BLACK>();

        Score piecesWhite = pieces<WHITE, KNIGHT>()
            + pieces<WHITE, BISHOP>()
            + pieces<WHITE, ADVISOR>()
            + pieces<WHITE, CANNON>();

        piecesWhite += pieces<WHITE, ROOK>();

        Score piecesBlack = pieces<BLACK, KNIGHT>()
            + pieces<BLACK, BISHOP>()
            + pieces<BLACK, ADVISOR>()
            + pieces<BLACK, CANNON>();

        piecesBlack += pieces<BLACK, ROOK>();

        score += piecesWhite - piecesBlack;

        if constexpr (T) {
            Trace::add(PIECES, piecesWhite, piecesBlack);
        }

        score += threat<WHITE>() - threat<BLACK>();

        score += (mobility[WHITE] - mobility[BLACK]) / 100;

        if constexpr (T) {
            Trace::add(THREAT, threat<WHITE>(), threat<BLACK>());
            Trace::add(MOBILITY, mobility[WHITE] / 100, mobility[BLACK] / 100);
            Trace::add(TOTAL, score);
        }

        // Derive single value from mg and eg parts of score
        Value v = winnable(score);

        // Side to move point of view
        v = (pos.side_to_move() == WHITE ? v : -v);

        return v;
    }

} // namespace Eval

using namespace Trace;

/// evaluate() is the evaluator for the outer world. It returns a static
/// evaluation of the position from the point of view of the side to move.
constexpr int rule60_a = 118, rule60_b = 221;

Value Eval::evaluate(const Position& pos, int* complexity) {

  Value v = Evaluation<NO_TRACE>(pos).value();

  if (complexity)
      *complexity = abs(v - pos.material_diff());

  // Damp down the evaluation linearly when shuffling
  v = v * (rule60_a - pos.rule60_count()) / rule60_b;

  int count = std::min(pos.count<ROOK>(WHITE) + pos.count<CANNON>(WHITE) + pos.count<KNIGHT>(WHITE),
                       pos.count<ROOK>(BLACK) + pos.count<CANNON>(BLACK) + pos.count<KNIGHT>(BLACK));
  constexpr int rangeTable[] = {0, 2, 4, 8, 16, 32, 88};
  if(count > 0)
    v += generateRandomNumber(rangeTable[count]);

  // Guarantee evaluation does not hit the mate range
  v = std::clamp(v, VALUE_MATED_IN_MAX_PLY + 1, VALUE_MATE_IN_MAX_PLY - 1);
  return v;
}

// format_cp_compact() converts a Value into (centi)pawns and writes it in a buffer.
// The buffer must have capacity for at least 5 chars.
static void format_cp_compact(Value v, char* buffer) {

    buffer[0] = (v < 0 ? '-' : v > 0 ? '+' : ' ');

    int cp = std::abs(100 * v / PawnValueEg);
    if (cp >= 10000)
    {
        buffer[1] = '0' + cp / 10000; cp %= 10000;
        buffer[2] = '0' + cp / 1000; cp %= 1000;
        buffer[3] = '0' + cp / 100;
        buffer[4] = ' ';
    }
    else if (cp >= 1000)
    {
        buffer[1] = '0' + cp / 1000; cp %= 1000;
        buffer[2] = '0' + cp / 100; cp %= 100;
        buffer[3] = '.';
        buffer[4] = '0' + cp / 10;
    }
    else
    {
        buffer[1] = '0' + cp / 100; cp %= 100;
        buffer[2] = '.';
        buffer[3] = '0' + cp / 10; cp %= 10;
        buffer[4] = '0' + cp / 1;
    }
}

/// trace() is like evaluate(), but instead of returning a value, it returns
/// a string (suitable for outputting to stdout) that contains the detailed
/// descriptions and values of each evaluation term. Useful for debugging.
/// Trace scores are from white's point of view

std::string Eval::trace(Position& pos) {

    if (pos.checkers())
        return "Final evaluation: none (in check)";

    std::stringstream ss;
    ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2);

    // Reset any global variable used in eval
    pos.this_thread()->bestValue = VALUE_ZERO;
    pos.this_thread()->optimism[WHITE] = VALUE_ZERO;
    pos.this_thread()->optimism[BLACK] = VALUE_ZERO;

    char board[3 * RANK_NB + 1][8 * FILE_NB + 2];
    std::memset(board, ' ', sizeof(board));
    for (int row = 0; row < 3 * RANK_NB + 1; ++row)
        board[row][8 * FILE_NB + 1] = '\0';
    // A lambda to output one box of the board
    auto writeSquare = [&board](File file, Rank rank, Piece pc, Value value) {
        const std::string PieceToChar(" RACPNBK racpnbk XXXXXX  xxxxxx");
        const int x = ((int)file) * 8;
        const int y = (RANK_9 - (int)rank) * 3;
        for (int i = 1; i < 8; ++i)
            board[y][x + i] = board[y + 3][x + i] = '-';
        for (int i = 1; i < 3; ++i)
            board[y + i][x] = board[y + i][x + 8] = '|';
        board[y][x] = board[y][x + 8] = board[y + 3][x + 8] = board[y + 3][x] = '+';
        if (pc != NO_PIECE)
            board[y + 1][x + 4] = PieceToChar[pc];
        if (value != VALUE_NONE)
            format_cp_compact(value, &board[y + 2][x + 2]);
    };
    // We estimate the value of each piece by doing a differential evaluation from
    // the current base eval, simulating the removal of the piece from its square.
    Value base = evaluate(pos);
    base = pos.side_to_move() == WHITE ? base : -base;
    for (File f = FILE_A; f <= FILE_I; ++f)
        for (Rank r = RANK_0; r <= RANK_9; ++r)
        {
            Square sq = make_square(f, r);
            Piece pc = pos.piece_on(sq);
            Value v = VALUE_NONE;
            if (pc != NO_PIECE && type_of(pc) != KING)
            {
                pos.remove_piece(sq);
                Value eval = evaluate(pos);
                eval = pos.side_to_move() == WHITE ? eval : -eval;
                v = base - eval;
                pos.put_piece(pc, sq);
            }
            writeSquare(f, r, pc, v);
        }
    ss << "HCE derived piece values:\n";
    for (int row = 0; row < 3 * RANK_NB + 1; ++row)
        ss << board[row] << '\n';
    ss << '\n';

    Value v = Evaluation<TRACE>(pos).value();

    ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2)
        << " Contributing terms for the classical eval:\n"
        << "+------------+-------------+-------------+-------------+\n"
        << "|    Term    |    White    |    Black    |    Total    |\n"
        << "|            |   MG    EG  |   MG    EG  |   MG    EG  |\n"
        << "+------------+-------------+-------------+-------------+\n"
        << "|   Material | " << Term(MATERIAL)
        << "|  Imbalance | " << Term(IMBALANCE)
        << "|       Pair | " << Term(PAIR)
        << "|     Pieces | " << Term(PIECES)
        << "|   Mobility | " << Term(MOBILITY)
        << "|    Threats | " << Term(THREAT)
        << "|   Winnable | " << Term(WINNABLE)
        << "+------------+-------------+-------------+-------------+\n"
        << "|      Total | " << Term(TOTAL)
        << "+------------+-------------+-------------+-------------+\n";

    ss << std::showpoint << std::showpos << std::fixed << std::setprecision(2) << std::setw(15);

    v = evaluate(pos);
    v = pos.side_to_move() == WHITE ? v : -v;
    ss << "Final evaluation       " << to_cp(v) << " (white side) [with optimism, ...]\n";

    return ss.str();
}

} // namespace Stockfish
