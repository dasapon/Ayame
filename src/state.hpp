#pragma once

#include "position.hpp"
#include "evaluate.hpp"

namespace shogi{
	enum CycleFlag{
		NoRep,
		CheckRepWin,
		SupRep,
		CheckRepLose,
		LesserRep,
		DrawRep,
	};
	class State : Position{
		struct History{
		public:
			EvalComponent eval;
			uint64_t board_key;
			uint64_t hand_counts;
			Move lastmove;
			bool check;
			History(){}
			History(const State& state);
			History(const History& h){
				operator=(h);
			}
			History(const State& state, Move lastmove, const EvalComponent& eval);
			void operator=(const History& rhs){
				eval = rhs.eval;
				board_key = rhs.board_key;
				hand_counts = rhs.hand_counts;
				lastmove = rhs.lastmove;
				check = rhs.check;
			}
		};
		std::vector<History> history;
		void eval_diff(EvalComponent& eval, Move lastmove, const EvalListDiff& elist_diff)const;
	public:
		State();
		State(const SimplePosition& sp);
		State (const State& state){
			operator=(state);
		}
		void make_move(Move move);
		void unmake_move();
		void set_up(const SimplePosition& sp);
		CycleFlag cycle_flag()const;
		int evaluate()const;
		const EvalComponent& eval_component()const{
			return history[ply()].eval;
		}
		Move previous_move(int i)const{
			if(i >= history.size())return Move::null_move();
			return history[ply() - i].lastmove;
		}
		int ply()const{
			return history.size() - 1;
		}
		void operator=(const State& rhs){
			Position::operator=(rhs);
			history = rhs.history;
		}
		int dynamic_exchange_evaluation(Move move);
		using Position::check;
		using Position::turn;
		using Position::key;
		using Position::next_position_key;
		using Position::GenerateMoves;
		using Position::generate_moves;
		using Position::generate_capture;
		using Position::generate_nocapture;
		using Position::generate_drop;
		using Position::generate_drop_check;
		using Position::generate_evasion;
		using Position::generate_nocapture_promotion;
		using Position::str2move;
		using Position::is_move_valid;
		using Position::piece_on;
		using Position::is_move_check;
		using Position::get_eval_list;
		using Position::see;
		using Position::sfen;
		using Position::regularized_sfen;
		using Position::mate1ply;
		using Position::Mate1Ply;
		using Position::get_hand;
		using Position::has_control;
		using Position::control_count;
		using Position::occupied;
		using Position::nyugyoku_declaration;
		using Position::piece_id;
		using Position::king_sq;
	};
}