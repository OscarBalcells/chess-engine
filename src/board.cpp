#include <vector>
#include <iostream>

#include "magicmoves.h"
#include "board.h"
#include "defs.h"
#include "gen.h"

// for debugging purposes
#define clear_square(sq, non_side_piece, _side) assert((bits[non_side_piece + (_side == BLACK ? 6 : 0)] & mask_sq(sq)) && color_at[sq] != EMPTY && piece_at[sq] == non_side_piece && color_at[sq] == _side && (occ_mask & mask_sq(sq))); bits[non_side_piece + (_side == BLACK ? 6 : 0)] ^= mask_sq(sq); color_at[sq] = piece_at[sq] = EMPTY; occ_mask ^= mask_sq(sq); assert((occ_mask & mask_sq(sq)) == 0 && (bits[non_side_piece + (_side == BLACK ? 6 : 0)] & mask_sq(sq)) == 0);
#define clear_square(sq, piece) assert((bits[piece] & mask_sq(sq)) != 0); assert(color_at[sq] != EMPTY && piece_at[sq] == (piece >= BLACK_PAWN ? piece - 6 : piece)); assert((piece <= WHITE_KING && color_at[sq] == WHITE) || (piece >= BLACK_PAWN && color_at[sq] == BLACK)); assert(occ_mask & mask_sq(sq)); bits[piece] ^= mask_sq(sq); color_at[sq] = piece_at[sq] = EMPTY; occ_mask ^= mask_sq(sq); assert((occ_mask & mask_sq(sq)) == 0); assert((bits[piece] & mask_sq(sq)) == 0);

static std::string str_seed = "Dratini is fast!";
static std::seed_seq seed(str_seed.begin(), str_seed.end());
static std::mt19937_64 rng(seed);
static std::uniform_int_distribution<uint64_t> dist(std::llround(std::pow(2, 56)), std::llround(std::pow(2, 62)));
static bool required_data_initialized = false; // this must be set to false at the beginning
static std::vector<std::vector<uint64_t> > zobrist_pieces;
static std::vector<uint64_t> zobrist_castling;
static std::vector<uint64_t> zobrist_enpassant;
static std::vector<uint64_t> zobrist_side;

std::vector<std::vector<uint64_t> > pawn_attacks;
std::vector<uint64_t> knight_attacks;
std::vector<uint64_t> king_attacks;
std::vector<uint64_t> castling_mask;

static uint64_t get_random_64() {
	return dist(rng);
}

// initializes zobrist hashes and bitboard tables
static void init_data() {
    if(required_data_initialized) {
        return;
	}

	initmagicmoves();

    zobrist_pieces.resize(12);
    for(int piece = WHITE_PAWN; piece <= BLACK_KING; piece++) {
        zobrist_pieces[piece].resize(64);
        for(int sq = 0; sq < 64; sq++)
            zobrist_pieces[piece][sq] = get_random_64();
    }
    
    zobrist_castling.resize(4);
    for(int castling_flag = 0; castling_flag < 4; castling_flag++)
        zobrist_castling[castling_flag] = get_random_64();

    zobrist_enpassant.resize(9);
    for(int col = 0; col < 8; col++)
        zobrist_enpassant[col] = get_random_64();
    /* when there is no enpassant column */
    zobrist_enpassant[NO_ENPASSANT] = 0;

    zobrist_side.resize(2);
    for(int _side = WHITE; _side <= BLACK; _side++)
        zobrist_side[_side] = get_random_64();

    /* pawn tables */
    pawn_attacks.resize(2);
    pawn_attacks[WHITE].assign(64, 0);
    pawn_attacks[BLACK].assign(64, 0);
    for(int sq = 0; sq < 64; sq++) {
        int _row = row(sq), _col = col(sq);
		if(_row > 0 && _col > 0)
			pawn_attacks[WHITE][sq] |= mask_sq(sq - 9);
		if(_row > 0 && _col < 7)
			pawn_attacks[WHITE][sq] |= mask_sq(sq - 7);
		if(_row < 7 && _col > 0)
			pawn_attacks[BLACK][sq] |= mask_sq(sq + 7);
		if(_row < 7 && _col < 7)
			pawn_attacks[BLACK][sq] |= mask_sq(sq + 9);
    }

    /* knight tables */
    knight_attacks.clear();
    knight_attacks.resize(64);
    for(int sq = 0; sq < 64; sq++) {
        int _row = row(sq), _col = col(sq);
		knight_attacks[sq] = 0;
        /* one up, two left */
        if(_col > 1 && _row < 7)
            knight_attacks[sq] |= mask_sq(sq + 8 - 2);
        /* two up, one left */
        if(_col > 0 && _row < 6)
            knight_attacks[sq] |= mask_sq(sq + 16 - 1);
        /* two up, one right */
        if(_col < 7 && _row < 6)
            knight_attacks[sq] |= mask_sq(sq + 16 + 1);
        /* one up, two right */
        if(_col < 6 && _row < 7)
            knight_attacks[sq] |= mask_sq(sq + 8 + 2);
        /* one down, two right */
        if(_col < 6 && _row > 0)
            knight_attacks[sq] |= mask_sq(sq - 8 + 2);
        /* two down, one right */
        if(_col < 7 && _row > 1)
            knight_attacks[sq] |= mask_sq(sq - 16 + 1);
        /* two down, one left */
        if(_col > 0 && _row > 1)
            knight_attacks[sq] |= mask_sq(sq - 16 - 1);
        /* one down, two left */
        if(_col > 1 && _row > 0)
            knight_attacks[sq] |= mask_sq(sq - 8 - 2);
    }

    /* king tables */
    king_attacks.clear();
    king_attacks.resize(64);
    for(int sq = 0; sq < 64; sq++) {
        int _row = row(sq), _col = col(sq);
        /* one left */
        if(_col > 0)
            king_attacks[sq] |= mask_sq(sq - 1);
        /* one left, one up */
        if(_col > 0 && _row < 7)
            king_attacks[sq] |= mask_sq(sq + 7);
        /* one up */
        if(_row < 7)
            king_attacks[sq] |= mask_sq(sq + 8);
        /* one right, one up */
        if(_col < 7 && _row < 7)
            king_attacks[sq] |= mask_sq(sq + 9);
        /* one right */
        if(_col < 7)
            king_attacks[sq] |= mask_sq(sq + 1);
        /* one right, one down */
        if(_col < 7 && _row > 0)
            king_attacks[sq] |= mask_sq(sq - 8 + 1);
        /* one down */
        if(_row > 0)
            king_attacks[sq] |= mask_sq(sq - 8);
        /* one left, one down */
        if(_col > 0 && _row > 0)
            king_attacks[sq] |= mask_sq(sq - 8 - 1);
    }

	castling_mask.resize(4);
	castling_mask[WHITE_QUEEN_SIDE] = mask_sq(B1) | mask_sq(C1) | mask_sq(D1);
	castling_mask[WHITE_KING_SIDE] = mask_sq(F1) | mask_sq(G1);
	castling_mask[BLACK_QUEEN_SIDE] = mask_sq(B8) | mask_sq(C8) | mask_sq(D8);
	castling_mask[BLACK_KING_SIDE] = mask_sq(F8) | mask_sq(G8);

    required_data_initialized = true;
}

Board::Board() {
	castling_rights.assign(4, true);
	init_data();

	set_from_fen("1k6/8/2Q5/8/3K4/8/8/8 w - - 0 1"); // useless king move, delaying the checkmate
	// set_from_fen("5k2/2r5/8/8/4B3/2Q5/2K5/8 b - - 0 1"); // don't do the stupid thing with the rook 
	// set_from_fen("5k2/4r3/8/8/4B3/2Q5/8/1K6 b - - 0 1"); // don't eat with the rook
	// set_from_fen("5k2/8/8/8/4r3/2Q5/8/1K6 w - - 0 1"); // do the queen trick
	// set_from_fen("8/1q6/8/R7/2k5/K7/8/8 w - - 0 1");
	// set_from_fen("8/1q6/8/8/2k5/K7/8/8 w - - 0 1");
	// set_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	// set_from_fen("5n2/8/5k2/8/K7/2p5/8/Q7 w - - 0 1");

	key = calculate_key();
	keys.push_back(key);
}

Board::Board(const std::string& str) { // bool read_from_file = false) {
	castling_rights.assign(4, true);
	init_data();
	set_from_fen(str);
	key = calculate_key();
	keys.push_back(key);
}

void Board::set_from_fen(const std::string& fen) {
	// https://www.chessprogramming.org/Forsyth-Edwards_Notation
	clear_board();
	int index = 0, _row = 7;
	while(_row >= 0) {
		int _col = 0;
		while(_col < 8) {
			assert(index < (int)fen.size());
			char _char = fen[index++];
			if(_char == '/') {

			} else if(_char >= '1' && _char <= '8') {
				_col += _char - '0';
			} else {
				const int sq = _row * 8 + _col;
				switch(_char) {
				case 'P':
					set_square(sq, WHITE_PAWN);
					break;
				case 'N':
					set_square(sq, WHITE_KNIGHT);
					break;
				case 'B':
					set_square(sq, WHITE_BISHOP);
					break;
				case 'R':
					set_square(sq, WHITE_ROOK);
					break;
				case 'Q':
					set_square(sq, WHITE_QUEEN);
					break;
				case 'K':
					set_square(sq, WHITE_KING);
					break;
				case 'p':
					set_square(sq, BLACK_PAWN);
					break;
				case 'n':
					set_square(sq, BLACK_KNIGHT);
					break;
				case 'b':
					set_square(sq, BLACK_BISHOP);
					break;
				case 'r':
					set_square(sq, BLACK_ROOK);
					break;
				case 'q':
					set_square(sq, BLACK_QUEEN);
					break;
				case 'k':
					set_square(sq, BLACK_KING);
					break;
				default:
					std::cout << "Invalid fen string (1)" << endl;
					clear_board();
					return;
				}
				_col++;
			}
		}
		_row--;
		if(!(fen[index] == '/' || (_row == -1 && fen[index] == ' '))) {
			std::cout << "Invalid fen string (2)" << endl;
		}
		index++;
	}
	/* side to move */
	if(fen[index] == 'w') {
		side = WHITE;
		xside = BLACK;
	} else if(fen[index] == 'b') {
		side = BLACK;
		xside = WHITE;
	} else {
		std::cout << "Invalid fen string (3)" << endl;
		clear_board();
		return;
	}
	/* space */
	if(fen[index] == ' ') {
		std::cout << "Invalid fen string (4)" << endl;
		clear_board();
		return;
	}
	index++;
	if(fen[index] != ' ') {
		std::cout << "Invalid fen string (5)" << endl;
		clear_board();
		return;
	}
	index++;
	/* castling */
	if(fen[index] == '-') {
		castling_rights[WHITE_QUEEN_SIDE] = false;
		castling_rights[WHITE_KING_SIDE] = false;
		castling_rights[BLACK_QUEEN_SIDE] = false;
		castling_rights[BLACK_KING_SIDE] = false;
		index++;
	} else {
		while(fen[index] != ' ') {
			switch(fen[index++]) {
				case 'K':
					castling_rights[BLACK_KING_SIDE] = true;
					break;
				case 'Q':
					castling_rights[BLACK_QUEEN_SIDE] = true;
					break;
				case 'k':
					castling_rights[WHITE_KING_SIDE] = true;
					break;
				case 'q':
					castling_rights[WHITE_QUEEN_SIDE] = true;
					break;
				default:
					std::cerr << "Invalid fen string (5)" << endl;
					clear_board();
					return;
			}
		}
	}
	/* space */
	if(fen[index] != ' ') {
		std::cout << "Invalid fen string (6)" << endl;
		clear_board();
		return;
	}
	index++;
	/* enpassant square */
	if(fen[index] == '-') {
		set_enpassant(NO_ENPASSANT);
		index++;
	} else {
		char char_col = fen[index++];
		char char_row = fen[index++];
		if(!(char_col >= 'a' && char_col <= 'h')) {
			std::cout << "Invalid fen string (7)" << endl;
			clear_board();
			return;
		}
		int _col = char_col - 'a';
		set_enpassant(_col);
	}
	/* space */
	if(fen[index] != ' ') {
		std::cout << "Invalid fen string (8)" << endl;
		clear_board();
		return;
	}
	index++;
	std::string tmp_str = "";
	/* fifty move counter */
	while(fen[index] != ' ') {
		if(!(fen[index] >= '0' && fen[index] <= '9')) {
			std::cout << "Invalid fen string (9)" << endl;
			clear_board();
			return;
		}
		tmp_str += fen[index++];
	}
	try {
		fifty_move_ply = stoi(tmp_str);
	} catch(char* exception_name) {
		std::cout << "Invalid fen string (10)" << endl;
		clear_board();
		return;
	}
	/* space */
	if(fen[index] != ' ') {
		std::cout << "Invalid fen string (11)" << endl;
		clear_board();
		return;
	}
	index++;
	tmp_str.clear();
	/* move counter */
	while(index < (int)fen.size() && fen[index] != ' ') {
		if(!(fen[index] >= '0' && fen[index] <= '9')) {
			std::cout << "Invalid fen string (12)" << endl;
			clear_board();
			return;
		}
		tmp_str += fen[index++];
	}
	try {
		move_count = stoi(tmp_str);
	} catch(char* exception_name) {
		std::cout << "Invalid fen string (13)" << endl;
		clear_board();
		return;
	}
	// key = calculate_key();
}

void Board::clear_board() {
	for(int piece = WHITE_PAWN; piece <= BLACK_KING; piece++) {
		bits[piece] &= 0;
	}
	for(int sq = 0; sq < 64; sq++) {
		color_at[sq] = piece_at[sq] = EMPTY;
	}
	occ_mask = 0;
	king_attackers = 0;
	enpassant = NO_ENPASSANT;
}

void Board::set_enpassant(int col) {
	enpassant = uint8_t(col);
}

// the piece in this case ranges from 0 to 5, so we need the _side parameter
void Board::set_square(int sq, int non_side_piece, bool _side) {
	assert(non_side_piece >= PAWN && non_side_piece <= KING);
	assert(piece_at[sq] == EMPTY && color_at[sq] == EMPTY);
	for(int _piece = WHITE_PAWN; _piece <= BLACK_KING; _piece++) if(non_side_piece + (_side == BLACK ? 6 : 0) != _piece) {
		assert((bits[_piece] & mask_sq(sq)) == 0);
	}

    bits[non_side_piece + (_side == BLACK ? 6 : 0)] |= mask_sq(sq);
	color_at[sq] = _side;
	piece_at[sq] = non_side_piece;
	occ_mask |= mask_sq(sq);

	assert(occ_mask & mask_sq(sq));
}

// the piece in this case ranges from 0 to 11 
void Board::set_square(int sq, int piece) {
	assert(piece >= WHITE_PAWN && piece <= BLACK_KING);
	assert(piece_at[sq] == EMPTY && color_at[sq] == EMPTY);
	for(int _piece = WHITE_PAWN; _piece <= BLACK_KING; _piece++) if(piece != _piece) {
		assert((bits[_piece] & mask_sq(sq)) == 0);
	}

    bits[piece] |= mask_sq(sq);
	color_at[sq] = (piece >= BLACK_PAWN ? BLACK : WHITE);
	piece_at[sq] = (piece >= BLACK_PAWN ? piece - 6 : piece);
	occ_mask |= mask_sq(sq);

	assert((piece >= BLACK_PAWN ? piece - 6 : piece) == piece_at[sq]);
	assert(bits[piece] & mask_sq(sq));
	assert(occ_mask & mask_sq(sq));
	assert(color_at[sq] != EMPTY && piece_at[sq] == (piece >= BLACK_PAWN ? piece - 6 : piece));
} 

// the piece ranges from 0 to 5
// void Board::clear_square(int sq, int non_side_piece, bool _side) {
    // assert(bits[non_side_piece + (_side == BLACK ? 6 : 0)] & mask_sq(sq));
	// assert(color_at[sq] != EMPTY && piece_at[sq] == non_side_piece);
	// assert(color_at[sq] == _side);
	// assert(occ_mask & mask_sq(sq));
// 
//     bits[non_side_piece + (_side == BLACK ? 6 : 0)] ^= mask_sq(sq);
// 	color_at[sq] = piece_at[sq] = EMPTY;
// 	occ_mask ^= mask_sq(sq);

// 	assert((occ_mask & mask_sq(sq)) == 0);
//     assert((bits[non_side_piece + (_side == BLACK ? 6 : 0)] & mask_sq(sq)) == 0);
// }

// // the piece ranges from 0 to 11
// void Board::clear_square(int sq, int piece) {
//     assert(bits[piece] & mask_sq(sq));
// 	assert(color_at[sq] != EMPTY && piece_at[sq] == (piece >= BLACK_PAWN ? piece - 6 : piece));
// 	assert((piece <= WHITE_KING && color_at[sq] == WHITE) || (piece >= BLACK_PAWN && color_at[sq] == BLACK));
// 	assert(occ_mask & mask_sq(sq));

//     bits[piece] ^= mask_sq(sq);
// 	color_at[sq] = piece_at[sq] = EMPTY;
// 	occ_mask ^= mask_sq(sq);

// 	assert((occ_mask & mask_sq(sq)) == 0);
// 	assert((bits[piece] & mask_sq(sq)) == 0);
// }

bool Board::is_empty(int sq) {
	bool ans_1 = (color_at[sq] == EMPTY);
	const uint64_t mask = mask_sq(sq);
	for(int piece = WHITE_PAWN; piece <= BLACK_KING; piece++) {
		if(mask & bits[piece]) {
			assert(ans_1 == false);
			assert(occ_mask);
			return false;
		}
	}
	assert(ans_1 == true);
	assert(!occ_mask);
	return true;
}

uint64_t Board::get_all_mask() const {
	uint64_t mask = 0;
	for(int piece = WHITE_PAWN; piece <= BLACK_KING; piece++) {
		mask |= bits[piece];
	}
	assert(occ_mask == mask);
	return occ_mask;
}

uint64_t Board::get_piece_mask(int piece) const {
	return bits[piece];
}

/* returns the mask of all white/black pieces */
uint64_t Board::get_side_mask(bool _side) const {
	uint64_t mask = 0;
	if(_side == WHITE) {
		for(int piece = WHITE_PAWN; piece <= WHITE_KING; piece++) {
			mask |= bits[piece];
		}
	} else {
		for(int piece = BLACK_PAWN; piece <= BLACK_KING; piece++) {
			mask |= bits[piece];
		}
	}
	return mask;
}

uint64_t Board::get_pawn_mask(bool _side) const {
	return _side == WHITE ? bits[WHITE_PAWN] : bits[BLACK_PAWN];
}

uint64_t Board::get_knight_mask(bool _side) const {
	return _side == WHITE ? bits[WHITE_KNIGHT] : bits[BLACK_KNIGHT];
}

uint64_t Board::get_bishop_mask(bool _side) const {
	return _side == WHITE ? bits[WHITE_BISHOP] : bits[BLACK_BISHOP];
}

uint64_t Board::get_rook_mask(bool _side) const {
	return _side == WHITE ? bits[WHITE_ROOK] : bits[BLACK_ROOK];
}

uint64_t Board::get_queen_mask(bool _side) const {
	return _side == WHITE ? bits[WHITE_QUEEN] : bits[BLACK_QUEEN];
}

uint64_t Board::get_king_mask(bool _side) const {
	return _side == WHITE ? bits[WHITE_KING] : bits[BLACK_KING];
}

int Board::get_piece(int sq) const {
	// slow check, just to make sure that bitmasks aren't overlapping
	for(int piece = WHITE_PAWN; piece <= BLACK_KING; piece++) {
		for(int _piece = WHITE_PAWN; _piece <= BLACK_KING; _piece++) if(piece != _piece) {
			assert(!(bits[piece] & bits[_piece]));
		}
	}
    for(int piece = WHITE_PAWN; piece <= BLACK_KING; piece++) {
        if((mask_sq(sq) & bits[piece])) {
			assert(mask_sq(sq) & bits[piece]);
			assert(piece_at[sq] != EMPTY);
			if((int(piece_at[sq]) + (piece >= BLACK_PAWN ? 6 : 0)) != piece) {
				cerr << "Classic piece doesn't match with bitboards" << endl;
				cerr << piece_char[piece] << " " << piece_char[piece_at[sq]] << endl;
				cerr << "Sq is " << pos_to_str(sq) << endl;
				cerr << "Board is" << endl;
				print_board();
			}
			assert((int(piece_at[sq]) + (piece >= BLACK_PAWN ? 6 : 0)) == piece);
            return piece;
		}
	}
    return EMPTY;
}

// returns whether a piece at square is WHITE, BLACK or EMPTY
int Board::get_color(int sq) const {
    uint64_t mask = mask_sq(sq);
    for(int piece = WHITE_PAWN; piece <= BLACK_KING; piece++) {
        if(mask & bits[piece]) {
			assert(color_at[sq] == (piece >= BLACK_PAWN ? BLACK : WHITE));
            return ((piece >= BLACK_PAWN) ? BLACK : WHITE); 
		}
	}
	assert(color_at[sq] == EMPTY);
    return EMPTY;
}

void Board::update_castling_rights(const Move move) {
    const int piece = get_piece(get_to(move));

	if(piece == WHITE_KING) {
		castling_rights[WHITE_QUEEN_SIDE] = false;
		castling_rights[WHITE_KING_SIDE] = false;
    } else if(piece == BLACK_KING) {
		castling_rights[BLACK_QUEEN_SIDE] = false;
		castling_rights[BLACK_KING_SIDE] = false;
	}

	if(castling_rights[WHITE_QUEEN_SIDE] && get_piece(A1) != WHITE_ROOK)
		castling_rights[WHITE_QUEEN_SIDE] = false;
	else if(castling_rights[WHITE_KING_SIDE] && get_piece(H1) != WHITE_ROOK)
		castling_rights[WHITE_KING_SIDE] = false;

	if(castling_rights[BLACK_QUEEN_SIDE] && get_piece(A8) != BLACK_ROOK)
		castling_rights[BLACK_QUEEN_SIDE] = false;
	else if(castling_rights[BLACK_KING_SIDE] && get_piece(H8) != BLACK_ROOK)
		castling_rights[BLACK_KING_SIDE] = false;
}

uint64_t Board::calculate_key() const {
	uint64_t new_key = 0;

	new_key ^= zobrist_enpassant[enpassant];

    for(int castling_type = 0; castling_type < 4; castling_type++) {
		if(castling_rights[castling_type]) {
			new_key ^= zobrist_castling[castling_type];
		}
	}

	int piece;
	for(int sq = 0; sq < 64; sq++) {
		piece = get_piece(sq);
		if(piece != EMPTY) {
			new_key ^= zobrist_pieces[piece][sq];   
		}
	}

	new_key ^= zobrist_side[side];

	return new_key;
}

void Board::update_key(const UndoData& undo_data) {
	const Move move = undo_data.move;

    if(undo_data.enpassant != enpassant) {
        key ^= zobrist_enpassant[undo_data.enpassant];
        key ^= zobrist_enpassant[enpassant];
    }

    if(is_null(move)) {
		key ^= zobrist_side[side];
		key ^= zobrist_side[xside];
        return;
	}

    for(int i = 0; i < 4; i++) {
        if(undo_data.castling_rights[i] != castling_rights[i]) {
            key ^= zobrist_castling[i];
		}
	}
        
    const int from_sq = get_from(move);
    const int to_sq = get_to(move);
    const int flag = get_flag(move);
	const int piece = undo_data.moved_piece;

    if(flag == QUIET_MOVE) {
		key ^= zobrist_pieces[piece][from_sq]; 
		key ^= zobrist_pieces[piece][to_sq];
    } else if(flag == CASTLING_MOVE) {
        int rook_from = -1, rook_to = -1;
        if(from_sq == E1 && to_sq == C1) { rook_from = A1; rook_to = D1; } 
        else if(from_sq == E1 && to_sq == G1) { rook_from = H1; rook_to = F1; } 
        else if(from_sq == E8 && to_sq == C8) { rook_from = A8; rook_to = D8; }
        else if(from_sq == E8 && to_sq == G8) { rook_from = H8; rook_to = F8; }
		else assert(false);
        key ^= zobrist_pieces[undo_data.captured_piece][rook_from];
        key ^= zobrist_pieces[undo_data.captured_piece][rook_to];
		key ^= zobrist_pieces[piece][from_sq]; 
		key ^= zobrist_pieces[piece][to_sq];
    } else if(flag == ENPASSANT_MOVE) {
        const int adjacent = row(from_sq) * 8 + col(to_sq);
        key ^= zobrist_pieces[undo_data.captured_piece][adjacent];
		key ^= zobrist_pieces[piece][from_sq]; 
		key ^= zobrist_pieces[piece][to_sq];
    } else if(flag == CAPTURE_MOVE) {
        key ^= zobrist_pieces[undo_data.captured_piece][to_sq];
		key ^= zobrist_pieces[piece][from_sq]; 
		key ^= zobrist_pieces[piece][to_sq];
    } else {
        int promotion_piece;
		switch(flag) {
			case KNIGHT_PROMOTION:
				promotion_piece = KNIGHT;
				break;
			case BISHOP_PROMOTION:
				promotion_piece = BISHOP;
				break;
			case ROOK_PROMOTION:
				promotion_piece = ROOK;
				break;
			case QUEEN_PROMOTION:
				promotion_piece = QUEEN;
				break;
			default:
				assert(false);
		}
		const int captured_piece = undo_data.captured_piece; // remember we can capture and promote at the same time
		if(captured_piece != EMPTY) {
			key ^= zobrist_pieces[captured_piece][to_sq];
		}
        key ^= zobrist_pieces[(side == WHITE ? BLACK_PAWN : WHITE_PAWN)][from_sq];
        key ^= zobrist_pieces[promotion_piece + (side == BLACK ? 0 : 6)][to_sq];
    }       

	key ^= zobrist_side[side];
    key ^= zobrist_side[xside];
}

bool Board::is_attacked(const int sq) const {
	// Version 1
	bool ans_1 = false;
    if(knight_attacks[sq] & get_knight_mask(xside)) {
		ans_1 = true; 
	}

    if(king_attacks[sq] & get_king_mask(xside)) {
		ans_1 = true;
	}
	
    if(pawn_attacks[xside][sq] & get_pawn_mask(xside)) {
		ans_1 = true;
	}

    const uint64_t occupation = get_all_mask();

    const uint64_t bishop_moves = Bmagic(sq, occupation);
    if((bishop_moves & get_bishop_mask(xside)) || (bishop_moves & get_queen_mask(xside))) {
		ans_1 = true;
    }

    const uint64_t rook_moves = Rmagic(sq, occupation);
    if((rook_moves & get_rook_mask(xside)) || (rook_moves & get_queen_mask(xside))) {
		ans_1 = true;
	}

    // Version 2 (not tested, but it might be faster)
	bool ans_2 = 
		   (knight_attacks[sq] & get_knight_mask(xside))
		|| (king_attacks[sq] & get_king_mask(xside))
    	|| (pawn_attacks[xside][sq] & get_pawn_mask(xside))
		|| (Bmagic(sq, get_all_mask()) & (get_bishop_mask(xside) | get_queen_mask(xside))) 
		|| (Rmagic(sq, get_all_mask()) & (get_rook_mask(xside) | get_queen_mask(xside)));
	
	assert(ans_1 == ans_2);
	return ans_2;
}

bool Board::checkmate() {
	if(!in_check()) {
		return false;
	}
	std::vector<Move> possible_moves;
	generate_moves(possible_moves, this);
	return possible_moves.empty();
}

bool Board::stalemate() {
	if(in_check()) {
		return false;
	}
	std::vector<Move> possible_moves;
	generate_moves(possible_moves, this);
	return possible_moves.empty();
}

bool Board::is_draw() const {
	if(fifty_move_ply >= 50) {
		cerr << "FMP" << endl;
		return true;
	}
     
    // Insufficient material:
    //    (no pawns && no rooks && no queens)
    // && (at least one side with only one piece)
    // && (   (no more than 1 bishop or knight in both sides)
    //     || (no bishops and no more than 2 knights))
    
	if(
	   !(get_piece_mask(WHITE_PAWN) | get_piece_mask(BLACK_PAWN) | get_piece_mask(WHITE_ROOK) | get_piece_mask(BLACK_ROOK) | get_piece_mask(WHITE_QUEEN) | get_piece_mask(BLACK_QUEEN))
	&& (!(get_side_mask(WHITE) ^ get_king_mask(WHITE)) || !(get_side_mask(BLACK) ^ get_king_mask(BLACK)))
    && (   (popcnt(get_side_mask(WHITE) | get_side_mask(BLACK)) <= 2)
        || (!(get_bishop_mask(WHITE) | get_bishop_mask(BLACK)) && popcnt(get_knight_mask(WHITE) | get_knight_mask(BLACK)) <= 2))
	) {
		return true;
	}	

    // draw by repetition
	int n_repetitions = 0;

	for(int i = keys.size() - 2; i >= 0; i--) {
		if(keys[i] == keys[keys.size() - 1]) {
			if(++n_repetitions == 3) {
				return false;
			}
		}
	}

    return false;
}

bool Board::in_check() const {
	assert(is_attacked(lsb(get_king_mask(side))) == bool(king_attackers));
	return is_attacked(lsb(get_king_mask(side)));
}

bool Board::castling_valid(const Move move) const {
    int from_sq = get_from(move);
    int to_sq   = get_to(move);

	int castling_type;
	uint64_t all_mask = get_all_mask();
	
	if(side == WHITE
	&& (from_sq == E1 && to_sq == C1)
	&& castling_rights[WHITE_QUEEN_SIDE]
	&& ((all_mask & castling_mask[WHITE_QUEEN_SIDE]) == 0)) {
		castling_type = WHITE_QUEEN_SIDE;
	} else if(side == WHITE
	&& (from_sq == E1 && to_sq == G1)
	&& (castling_rights[WHITE_KING_SIDE])
	&& ((all_mask & castling_mask[WHITE_KING_SIDE]) == 0)) {
		castling_type = WHITE_KING_SIDE;
	} else if(side == BLACK
	&& (from_sq == E8 && to_sq == C8)
	&& castling_rights[BLACK_QUEEN_SIDE]
	&& ((all_mask & castling_mask[BLACK_QUEEN_SIDE]) == 0)) {
		castling_type = BLACK_QUEEN_SIDE;
	} else if(side == BLACK
	&& (from_sq == E8 && to_sq == G8)
	&& castling_rights[BLACK_KING_SIDE]
	&& ((all_mask & castling_mask[BLACK_KING_SIDE]) == 0)) {
		castling_type = BLACK_KING_SIDE;
	} else {
		return false;
	}

    if(is_attacked(from_sq) || is_attacked(to_sq)) {
        return false;
	}

	if(castling_type == WHITE_QUEEN_SIDE || castling_type == BLACK_QUEEN_SIDE) { 
		return !is_attacked(from_sq - 1);
    } else if(castling_type == WHITE_KING_SIDE || castling_type == BLACK_KING_SIDE) {
    	return !is_attacked(from_sq + 1);
    } else {
		assert(false); // we should not get here
		return false;
	}
}

// it only works for pawns
bool Board::move_diagonal(const Move move) const {
	int from_sq = get_from(move);
	int to_sq   = get_to(move);
	if(side == WHITE) {
		if(to_sq != from_sq + 7 && to_sq != from_sq + 9)
			return false;
		/* wrong offset */
		if(col(to_sq) == 0 && to_sq != from_sq + 7)
			return false;
		/* wrong offset */
		if(col(to_sq) == 7 && to_sq != from_sq + 9)
			return false;
	} else {
		if(to_sq != from_sq - 7 && to_sq != from_sq - 9)
			return false;
		/* wrong offset */
		if(col(to_sq) == 0 && to_sq != from_sq - 9)
			return false;
		/* wrong offset */
		if(col(to_sq) == 7 && to_sq != from_sq - 7)
			return false;
	}
    return true;
}


void Board::print_board() const {
	std::cout << endl;
	int i;
	for(i = 56; i >= 0;) {
		if(i % 8 == 0) std::cout << (i / 8) + 1 << "  ";
		std::cout << " ";
		if(get_piece(i) == EMPTY) {
			std::cout << '.';
		} else {
			std::cout << piece_char[get_piece(i)];
		}
		if((i + 1) % 8 == 0) {
			std::cout << '\n';
			i -= 15;
		} else {
			i++;
		}
	}
	std::cout << endl << endl << "   ";
	for(i = 0; i < 8; i++)
		std::cout << " " << char('a' + i);
	std::cout << endl;
	/*
	std::cout << "Move count: " << move_count << endl;
	std::cout << "Fifty move count: " << int(fifty_move_ply) << endl;
	if(enpassant == 8) {
		std::cout << "Enpassant: None" << endl;
	} else {
		std::cout << "Enpassant: " << int(enpassant) << endl;
	}
	// cout << "Key is:" << endl; 
	// cout << key << endl;
	std::cout << "Castling (WQ, WK, BQ, BK): ";
	for(i = 0; i < 4; i++)
		std::cout << (castling_rights[i] ? "Y " : "N ");
	std::cout << endl;
	*/
	std::cout << "Side: " << (side == WHITE ? "WHITE" : "BLACK") << endl;
	assert(side != xside);
}

void Board::print_bitboard(uint64_t bb) const {
	std::cout << endl;
	int i;
	for(i = 56; i >= 0;) {
		if(i % 8 == 0) std::cout << (i / 8) + 1 << "  ";
		std::cout << " ";
		if(bb & (uint64_t(1) << i)) {
			if(get_piece(i) == EMPTY) {
				std::cout << 'x';
			} else {
				std::cout << piece_char[get_piece(i)];
			}
		} else {
			std::cout << '.';
		}
		// std::cout << RESET_COLOR;
		if((i + 1) % 8 == 0) {
			std::cout << '\n';
			i -= 15;
		} else {
			i++;
		}
	}
	std::cout << endl << endl << "   ";
	for(i = 0; i < 8; i++) {
		std::cout << " " << char('a' + i);
	}
	std::cout << endl;
}

void Board::check_classic() {
	int piece;
	for(int sq = 0; sq < 64; sq++) {
		int piece = get_piece(sq);
		if(piece == EMPTY && (color_at[sq] != EMPTY || piece_at[sq] != EMPTY)) {
			assert(color_at[sq] == piece_at[sq]);
			assert(!(piece == EMPTY && (color_at[sq] != EMPTY || piece_at[sq] != EMPTY)));
		} else if(piece == EMPTY) {
			continue;
		}
		int classic_piece = piece_at[sq] + (color_at[sq] == WHITE ? 0 : 6);
		assert(bits[classic_piece] & mask_sq(sq));
		get_piece(sq); 
	}
}

void Board::error_check() const {
	uint64_t occ_mask_now = 0;
	for(int i = WHITE_PAWN; i <= BLACK_KING; i++) {
		occ_mask_now |= bits[i];
		for(int j = WHITE_PAWN; j <= BLACK_KING; j++) if(i != j) {
			if((bits[i] & bits[j]) != 0) {
				cerr << "Two bitmasks are overlapped" << endl;
				cerr << i << " " << j << endl;
				cerr << "Board is:" << endl;
				print_board();
			}
			assert(!(bits[i] & bits[j]));
		}
	}
	if(occ_mask != occ_mask_now) {
		cerr << "Occ mask is different than bits mask" << endl;
		print_board();
	}
	assert(occ_mask == occ_mask_now);
	for(int sq = 0; sq < 64; sq++) {
		int piece_classic = piece_at[sq];
		if(color_at[sq] == EMPTY && piece_classic != EMPTY) {
			cerr << "color_at and piece_at don't match at sq = " << sq << endl;
			print_board();
		} else if(color_at[sq] != EMPTY && piece_classic == EMPTY) {
			cerr << "color_at and piece_at don't match at sq = " << sq << endl;
			print_board();
		}
		assert((piece_at[sq] != EMPTY && color_at[sq] != EMPTY) || (piece_at[sq] == EMPTY && color_at[sq] == EMPTY));
		if(color_at[sq] == EMPTY) {
			continue;
		}
		if(color_at[sq] != EMPTY && color_at[sq] == BLACK) {
			piece_classic += 6; 
		}
		if(!(bits[piece_classic] & mask_sq(sq))) {
			cerr << "Classic and bitboard don't match for sq = " << sq << endl;
			cerr << "Board is:" << endl;
			print_board();
		}
		assert(bits[piece_classic] & mask_sq(sq));
	}
}

bool Board::same(const Board& other) const {
	if(other.castling_rights[WHITE_QUEEN_SIDE] != castling_rights[WHITE_QUEEN_SIDE])
		return false;
	if(other.castling_rights[WHITE_KING_SIDE] != castling_rights[WHITE_KING_SIDE])
		return false;
	if(other.castling_rights[BLACK_QUEEN_SIDE] != castling_rights[BLACK_QUEEN_SIDE])
		return false;
	if(other.castling_rights[BLACK_KING_SIDE] != castling_rights[BLACK_KING_SIDE])
		return false;

	for(int piece = WHITE_PAWN; piece <= BLACK_KING; piece++) {
		if(bits[piece] != other.bits[piece]) {
			return false;
		}
	}

	for(int i = 0; i < 64; i++) {
		if(other.get_piece(i) != get_piece(i)
		|| color_at[i] != other.color_at[i]
		|| piece_at[i] != piece_at[i]
		) {
			return false;
		}
	}

	return true;
}

void Board::print_board_data() const {
	cout << endl << endl;
	cout << '"';
	for(int piece = 0; piece < 12; piece++) {
		cerr << bits[piece] << " ";
	}
	for(int sq = 0; sq < 64; sq++) {
		cerr << int(color_at[sq]) << " ";
	}
	for(int sq = 0; sq < 64; sq++) {
		cerr << int(piece_at[sq]) << " ";
	}
	cerr << int(enpassant) << " ";
	cerr << int(side) << " ";
	cerr << int(xside);
	cout << int(castling_rights[0]) << " "; 
	cout << int(castling_rights[1]) << " "; 
	cout << int(castling_rights[2]) << " "; 
	cout << int(castling_rights[3]) << " "; 
	cout << int(move_count) << " ";
	cout << int(fifty_move_ply) << " ";
	cout << key;
	cout << '"' << endl << endl;
}

std::string Board::get_data() const {
	std::string data = "";
	for(int pos = 0; pos < 64; pos++) {
		if(pos > 0) cout << ' ';
		data += std::to_string(int(get_piece(pos)));
		// cout << (int)get_piece(pos);
	}
	data += ' ' + std::to_string(key) + ' ';
	data += std::to_string(move_count) + ' ';
	data += std::to_string(int(fifty_move_ply)) + ' ';
	data += std::to_string(int(enpassant)) + ' ';
	data += std::to_string(int(castling_rights[0])) + ' ';
	data += std::to_string(int(castling_rights[1])) + ' ';
	data += std::to_string(int(castling_rights[2])) + ' ';
	data += std::to_string(int(castling_rights[3])) + ' ';
	data += std::to_string(int(side)) + ' ';
	data += std::to_string(int(xside));
	return data;
}

void Board::set_from_data() {
	// freopen(file_name.c_str(), "r", stdin);	
	// std::string data = "3 1 2 4 12 2 1 3 0 12 0 12 5 0 0 0 12 12 12 12 12 12 12 12 12 0 12 0 0 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 6 7 12 12 6 6 6 6 12 6 6 6 9 7 8 10 11 12 12 9 1106155485056437330 4 2 8 0 0 1 1 1 0";
	std::string data = "3 1 2 4 12 2 1 3 0 12 0 12 5 0 0 0 12 12 12 12 12 12 12 12 12 0 12 0 0 12 12 12 12 12 12 12 12 12 12 12 12 12 12 12 6 7 12 12 6 6 6 6 12 6 6 6 9 7 8 10 11 12 12 9 1106155485056437330 4 2 8 0 0 1 1 1 0"; 
	// std::string data = "436266240 66 274877906976 129 8 4096 49275987988316160 144150372447944704 288230376151711744 9295429630892703744 576460752303423488 1152921504606846976 0 0 12 0 12 0 0 0 0 12 0 12 0 0 0 0 12 12 12 12 12 12 12 12 12 0 12 0 0 12 12 12 12 12 12 12 12 12 0 12 12 12 12 12 1 1 12 12 1 1 1 1 12 1 12 1 1 1 1 1 1 12 12 1 3 1 12 4 12 2 1 3 0 12 0 12 5 0 0 0 12 12 12 12 12 12 12 12 12 0 12 0 0 12 12 12 12 12 12 12 12 12 2 12 12 12 12 12 0 1 12 12 0 0 0 0 12 0 12 0 3 1 2 4 5 12 12 3 8 1 00 0 1 1 3 1 2549602398997328124";

	// reset bitboards
	for(int i = 0; i <= BLACK_KING; i++) {
		bits[i] = 0;
	}

	int i = 0;
	for(int pos = 0; pos < 64; pos++) {
		std::string this_number = "";
		while(i < (int)data.size() && data[i] != ' ') {
			this_number += data[i++];
		}
		i++;
		int piece = stoi(this_number);
		bits[piece] |= mask_sq(pos);		
		cerr << piece << " ";
	}
	side = BLACK;
	xside = WHITE;
	enpassant = NO_ENPASSANT;
	fifty_move_ply = 2;
	move_count = 2;
	castling_rights[0] = false;
	castling_rights[1] = false;
	castling_rights[2] = true;
	castling_rights[3] = true;
}
