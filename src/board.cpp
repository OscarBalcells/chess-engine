#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include "board.h"
#include "defs.h"
#include "gen.h"
#include "data.h"
#include "hash.h"

// we reset every single game variable
void Position::init_board() {
	for(int i = 0; i < 64; i++) {
		piece[i] = initial_piece[i];
		color[i] = initial_color[i];
	}
	side = WHITE;
	xside = BLACK;
	castling = 63;
	enpassant = 0;
	while(!move_stack.empty()) move_stack.pop_back();
	while(!taken_moves.empty()) taken_moves.pop_back();
	while(!unordered_move_stack.empty()) unordered_move_stack.pop_back();
	init_zobrist();
	book_deactivated = false;
}

bool Position::is_attacked(char pos, bool attacker_side) {
	// pawns
	if(attacker_side == BLACK) {
		if(valid_distance(pos, pos + 7) && piece[pos + 7] == PAWN && color[pos + 7] == attacker_side) return true;
		if(valid_distance(pos, pos + 9) && piece[pos + 9] == PAWN && color[pos + 9] == attacker_side) return true;
	} else {
		if(valid_distance(pos, pos - 7) && piece[pos - 7] == PAWN && color[pos - 7] == attacker_side) return true;
		if(valid_distance(pos, pos - 9) && piece[pos - 9] == PAWN && color[pos - 9] == attacker_side) return true;
	}
	int i;
	char new_pos = pos;
	// knights
	for(i = 0; i < 8; i++) {
		if(offset[KNIGHT][i] == 0) break; // done
		if(!valid_distance(pos, pos + offset[KNIGHT][i])) continue;
		if(piece[pos + offset[KNIGHT][i]] == KNIGHT && color[pos + offset[KNIGHT][i]] == attacker_side) return true;
	}
	// bishops and queen
	for(i = 0; i < 8; i++) {
    if(offset[BISHOP][i] == 0) break;
		new_pos = pos;
		while(valid_distance(new_pos, new_pos + offset[BISHOP][i]) && color[new_pos + offset[BISHOP][i]] != (attacker_side ^ 1)) {
			new_pos += offset[BISHOP][i];
			if((piece[new_pos] == BISHOP || piece[new_pos] == QUEEN) && color[new_pos] == attacker_side) return true;
			else if(color[new_pos] == attacker_side) break;
 	   }
 	}
	// rook and queen
	for(i = 0; i < 8; i++) {
		if(offset[ROOK][i] == 0) break;
		new_pos = pos;
		while(valid_distance(new_pos, new_pos + offset[ROOK][i]) && color[new_pos + offset[ROOK][i]] != (attacker_side ^ 1)) {
			new_pos += offset[ROOK][i];
			if((piece[new_pos] == ROOK || piece[new_pos] == QUEEN) && color[new_pos] == attacker_side) return true;
			else if(color[new_pos] == attacker_side) break;
		}
	}
	//	king
	char delta_moves[8] = { 8, 9, 1, -7, -8, -9, -1, 7 }; // for some reason, my compiler wants me to declare this
	for(i = 0; i < 8; i++) {
		if(valid_pos(pos + delta_moves[i])
		&& piece[pos + delta_moves[i]] == KING
		&& color[pos + delta_moves[i]] == attacker_side)
			return true;
	}
	return false;
}

bool Position::in_check(bool defender_side) {
	for(char pos = 0; pos < 64; pos++) {
		if(piece[pos] == KING && color[pos] == defender_side) {
		  return is_attacked(pos, defender_side ^ 1);
		}
	}
	return false;
}

void Position::print_board() {
	std::cout << endl;
	int i;
	for(i = 56; i >= 0;) {
		if(i % 8 == 0) std::cout << (i / 8) + 1 << "  ";
		std::cout << " ";
		if(color[i] == WHITE) {
			std::cout << WHITE_COLOR;
			if(piece[i] == PAWN) std::cout << 'P';
			else if(piece[i] == KNIGHT) std::cout << 'N';
			else if(piece[i] == BISHOP) std::cout << 'B';
			else if(piece[i] == ROOK) std::cout << 'R';
			else if(piece[i] == QUEEN) std::cout << 'Q';
			else if(piece[i] == KING) std::cout << 'K';
		} else if(color[i] == BLACK) {
			std::cout << WHITE_COLOR;
			if(piece[i] == PAWN) std::cout << 'p';
			else if(piece[i] == KNIGHT) std::cout << 'n';
			else if(piece[i] == BISHOP) std::cout << 'b';
			else if(piece[i] == ROOK) std::cout << 'r';
			else if(piece[i] == QUEEN) std::cout << 'q';
			else if(piece[i] == KING) std::cout << 'k';
		} else if(color[i] == EMPTY) {
			std::cout << EMPTY_COLOR << '.';
		}
		// std::cout << RESET_COLOR;
		if((i + 1) % 8 == 0) {
			std::cout << '\n';
			i -= 15;
		} else {
			i++;
		}
	}
 	std::cout << '\n' << "   ";
	for(i = 0; i < 8; i++) std::cout << " " << char('a' + i);
  	std::cout << "\n\n";
}

// There should be some more checks here
bool Position::is_draw() {
	for(char i = 0; i < 64; i++) {
		if(piece[i] != KING) return false;
	}
	return true;
}

int Position::game_over() {
	if(is_draw()) return EMPTY;
	int n_moves_prev = (int)move_stack.size();
	generate_moves(*this);
	int n_moves_now = (int)move_stack.size();
	if(n_moves_prev != n_moves_now) {
		return -1; // not over
	} else {
		bool white_in_check = in_check(WHITE);
		bool black_in_check = in_check(BLACK);
		assert(!(white_in_check && black_in_check)); // both can't be true at the same time
		if(white_in_check) return BLACK; // BLACK WON
		else if(black_in_check) return WHITE; // WHITE WON
		else return EMPTY; // DRAW
	}
}

void Position::save_snapshot(std::string snapshot_name) {
  freopen(("./snapshots/" + snapshot_name + ".snapshot").c_str(), "w", stdout);
  std::cout << side << endl;
  std::cout << castling << endl;
  std::cout << enpassant << endl;
  for(int i = 0; i < 64; i++) {
    std::cout << color[i] << " " << piece[i] << endl;
  }
  freopen("/dev/tty", "w", stdout);
  std::cout << "Snapshot made!" << endl; // this should be printed in console
}

void Position::load_snapshot(std::string snapshot_name) {
  freopen(("./snapshots/" + snapshot_name + ".snapshot").c_str(), "r", stdin);
  std::cin >> side;
  xside = side ^ 1;
  std::cin >> castling;
  std::cin >> enpassant;
  for(int i = 0; i < 64; i++) {
    std::cin >> color[i];
    std::cin >> piece[i];
  }
  freopen("/dev/tty", "r", stdin);
  std::cout << "Snapshot loaded!" << endl;
}