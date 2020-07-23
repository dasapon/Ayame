#pragma once

#include "bitboard.hpp"
#include "move.hpp"
#include "hand.hpp"
#include "evaluate.hpp"

namespace shogi{
	using Sfen = sheena::Array<std::string, 4>;
	using MoveArray = sheena::Array<Move, MaxLegalMove>;
	extern const Sfen startpos;
	extern sheena::Array3d<uint64_t, Sentinel, BoardSize, PlayerDim> hash_seed;
	extern sheena::Array3d<uint64_t, 18, King, PlayerDim> hand_hash_seed;
	extern void init_hash_seed();
	class Position;
	//単純な盤面表現.主に学習時等に大量に局面を保持する場合にSfenよりメモリ使用量が少ない(ハズ)
	class SimplePosition{
		friend class Position;
		sheena::Array<Piece, NumPiece> pieces;
		Player turn;
		void set_up(const std::string& board, Player turn, const std::string& hand);
	public:
		SimplePosition(){};
		SimplePosition(const Sfen& sfen);
		SimplePosition(const std::string& regularized_sfen);
		void operator=(const SimplePosition& rhs){
			pieces = rhs.pieces;
			turn = rhs.turn;
		}
	};
	//価値の小さい順に駒を取り出すためのQueue
	class SeeQueue;
	class Position{
		sheena::Array<sheena::VInt<BoardSize>, PlayerDim> pin;
		sheena::VInt<BoardSize> sliding_control;
		sheena::Array2d<ShortControl, PlayerDim, BoardSize> short_control;
		sheena::Array<Piece, BoardSize> board;
		sheena::Array<Piece, NumPiece> pieces;
		BitBoard all_bb;
		sheena::Array<SlidingControl, PlayerDim> own_slider;
		sheena::Array<PieceSet, PlayerDim> pieces_on_board;
		sheena::Array<Hand, PlayerDim> hand;
		PieceSet promoted_pieces;
		uint64_t board_key, hand_key;
		sheena::Array<BitBoard, PlayerDim> pawn_exist_file_mask;
		EvalList eval_list;
		Player turn_player;
		//ピン判定
		bool is_pinned(Player pl, Square sq)const{
			return pin[pl][sq] & sliding_control[sq] & own_slider[opponent(pl)];
		}
		//空き王手判定
		bool discovered(Player pl, Square from, Square to)const{
			if(is_pinned(pl, from)){
				const Square ksq = king_sq(pl);
				return (square_relation[ksq][to] & square_relation[ksq][from]) == 0;
			}
			return false;
		}
		bool pawn_exist(Player pl, Square sq)const{
			return pawn_exist_file_mask[pl][sq];
		}
		template<PieceType pt>
		int generate_nocapture_sub(Player pl, MoveArray& moves, int i, PieceSet piece_set)const;
		template<Player pl, int N>
		int generate_drop_sub2(MoveArray& moves, int n, sheena::Array<PieceType, 6>& pieces)const;
		template<PieceType pt>
		int generate_drop_check_sub(MoveArray& moves, int n, Square ksq)const;
		template<Player pl, int N>
		int generate_drop_sub(MoveArray& moves, int n, sheena::Array<PieceType, 6>& pieces)const;
		template<int N>
		int generate_drop_y(MoveArray& moves, int y, int n, sheena::Array<PieceType, 6>& pieces)const;
		int generate_evasion_sub(MoveArray& moves, int n, Square to, Square drop_pawn_mate_square)const;
		void put_piece(Player owner, PieceType pt, Square sq, int id){
			all_bb ^= square_bb[sq];
			board[sq].set(pt, sq, owner, id);
			eval_list[id] = EvalIndex::on_board(owner, pt, sq);
			pieces[id] = board[sq];
			board_key ^= hash_seed[pt][sq][owner];
		}
		template<Player owner>
		void remove_piece(Square sq){
			assert(board[sq].owner() == owner);
			all_bb ^= square_bb[sq];
			board_key ^= hash_seed[board[sq].type()][sq][owner];
			board[sq].clear();
		}
		int dec_hand(Player pl, PieceType pt){
			int ret = hand[pl].pop(pt);
			hand_key ^= hand_hash_seed[hand[pl].count(pt)][pt][pl];
			return ret;
		}
		void inc_hand(Player pl, PieceType pt, int id){
			hand_key ^= hand_hash_seed[hand[pl].count(pt)][pt][pl];
			eval_list[id] = EvalIndex::in_hand(pl, pt, hand[pl].count(pt));
			hand[pl].push(pt, id);
			pieces[id].set(pt, SquareHand, pl, id);
		}
		template<Player pl>
		void xor_control(PieceType pt, Square sq, int id);
		void set_pin(Player pl, Square sq);
		void open_or_close_pin_slidings(Square sq);
		//sqに駒が置かれるとplの効きが消えるマス
		BitBoard control_invalidation(Player pl, Square sq)const;
		BitBoard attack_neighbor8(Player pl, Square sq)const;
		template<Player pl>
		bool mate1move(Move* pmove, Square from, Square to, const BitBoard& evasion_canditate);
		//影効きを作っている駒のIDを得る.影効きを作る駒がなければ-1を返す seeで使用する
		int find_hidden_attacker(Square target_sq, Square forward_attacker_sq)const;
		int see_sub(Square target_square, int hanged, Player pl, sheena::Array<SeeQueue, PlayerDim>& queue)const;
		Square drop_pawn_mate_square()const;
		std::string board_sfen(bool flip)const;
		std::string hand_sfen(bool flip)const;
		template<Player pl>
		bool nyugyoku()const;
	public:
		Position();
		Position(const SimplePosition& sp);
		Position(const Position& pos);
		void set_up(const SimplePosition& sp);
		int generate_capture(MoveArray& moves, int n)const;
		int generate_nocapture(MoveArray& moves, int n)const;
		template<PieceType pt>
		int generate_nocapture_promotion(MoveArray& moves, int n)const;
		template<Player pl>
		int generate_drop(MoveArray& moves, int n)const;
		int generate_drop_check(MoveArray& moves, int n)const;
		int generate_evasion(MoveArray& moves, int n = 0)const;
		template<Player pl>
		int generate_moves(MoveArray& moves)const;
		int GenerateMoves(MoveArray& moves)const;

		template<Player pl>
		EvalListDiff make_move(Move move);
		EvalListDiff MakeMove(Move move){
			return turn() == First? make_move<First>(move) : make_move<Second>(move);
		}
		template<Player pl>
		void unmake_move(Move move);
		void UnmakeMove(Move move){
			turn() == First? unmake_move<Second>(move) : unmake_move<First>(move);
		}

		Player turn()const{
			return turn_player;
		}
		Square king_sq(Player pl)const{
			return pieces[FKingID + pl].square();
		}
		Piece piece_id(int id)const{
			return pieces[id];
		}
		Piece piece_on(Square sq)const{
			assert(sq >= 0);
			assert(sq < BoardSize);
			return board[sq];
		}
		const BitBoard& occupied()const{
			return all_bb;
		}
		uint64_t key()const{return board_key ^ hand_key;}
		uint64_t get_board_key()const{return board_key;}
		uint64_t next_position_key(Move move)const{
			uint64_t key = board_key ^ hand_key;
			const PieceType pt = move.piece_type();
			const Square from = move.from(), to = move.to();
			const Player pl = turn();
			if(move.is_drop()){
				key ^= hand_hash_seed[hand[pl].count(pt) - 1][pt][pl];
				key ^= hash_seed[pt][to][pl];
			}
			else{
				key ^= hash_seed[pt][from][pl];
				if(move.is_capture()){
					PieceType cap = move.capture();
					key ^= hash_seed[cap][to][opponent(pl)];
					key ^= hand_hash_seed[hand[pl].count(unpromote(cap))][unpromote(cap)][pl];
				}
				key ^= hash_seed[move.pt_promoted()][to][pl];
			}
			key ^= 1;
			return key;
		}
		bool has_control(Player pl, Square sq)const{
			return short_control[pl][sq] != 0 || (sliding_control[sq] & own_slider[pl]) != 0;
		}
		int control_count(Player pl, Square sq)const{
			return sheena::popcnt32(short_control[pl][sq]) + sheena::popcnt32(sliding_control[sq] & own_slider[pl]);
		}
		bool check()const{
			return has_control(opponent(turn_player), king_sq(turn_player));
		}
		bool nyugyoku_declaration()const;
		Move str2move(std::string str)const;

		bool is_move_check(Move move)const{
			Player king_side = opponent(turn());
			Square from = move.from(), to = move.to();
			//空き王手の判定
			if(discovered(king_side, from, to))return true;
			//直接王手の判定
			//all_bbが移動後の物になっていないが、気にしなくて良い
			return attack_bb(turn(), move.pt_promoted(), to, all_bb)[king_sq(king_side)];
		}
		bool is_move_valid(Move move)const{
			if(!move)return false;
			const Player pl = turn();
			Square from = move.from();
			Square to = move.to();
			PieceType pt = move.piece_type();
			if(move.is_drop()){
				if(hand[pl].count(pt) == 0 || !board[to].empty())return false;
				if(pt == Pawn){
					//2歩判定
					if(pawn_exist(pl, to))return false;
					//打ち歩判定
					if(drop_pawn_mate_square() == to)return false;
				}
				return true;
			}
			else{
				if(pt != board[from].type() || board[from].owner() != pl)return false;
				//captureが正しいか?
				if(move.capture() != board[to].type())return false;
				if(move.is_capture() && board[to].owner() == pl)return false;
				//効きの有無の判定
				if(pt == King){
					//玉の自殺手判定
					if(move.is_promotion())return false;
					if(has_control(opponent(pl), to))return false;
					return king_attack[from][to];
				}
				else{
					//自殺手判定,成りフラグの判定
					if(move.is_promotion()){
						if(!can_promote(pl, pt, from, to))return false;
					}
					else{
						if(must_promote(pl, pt, to))return false;
					}
					if(discovered(pl, from, to))return false;
					return attack_bb(pl, pt, from, all_bb)[to];
				}
			}
		}
		template<Player pl>
		bool mate1ply(Move* pmove);
		bool Mate1Ply(Move* pmove);
		int see(Move move)const;
		const Hand& get_hand(Player pl)const{
			return hand[pl];
		}
		const EvalList& get_eval_list()const{
			return eval_list;
		}
		Sfen sfen(bool flip = false)const;
		std::string regularized_sfen()const{
			bool flip = turn() != First;
			return board_sfen(flip) + " " + hand_sfen(flip);
		}
	};
}