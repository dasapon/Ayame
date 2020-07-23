#include "state.hpp"

namespace shogi{
	State::History::History(const State& state):eval(state), board_key(state.get_board_key()),
	hand_counts(state.get_hand(state.turn()).get_counts()), lastmove(Move::null_move()), check(state.check()){
	}
	State::History::History(const State& state, Move lastmove, const EvalComponent& eval):eval(eval),
	board_key(state.get_board_key()),hand_counts(state.get_hand(state.turn()).get_counts()), lastmove(lastmove), check(state.check()){}
	
	State::State(){
		history.reserve(2048);
		set_up(startpos);
	}
	State::State(const SimplePosition& sp){
		history.reserve(2048);
		set_up(sp);
	}
	void State::set_up(const SimplePosition& sp){
		Position::set_up(sp);
		history.resize(0);
		history.push_back(History(*this));
	}
	void State::make_move(Move move){
		EvalComponent eval = history[ply()].eval;
		EvalListDiff elist_diff = Position::MakeMove(move);
		eval_diff(eval, move, elist_diff);
		history.push_back(History(*this, move, eval));
	}
	void State::unmake_move(){
		Move move = history[history.size() - 1].lastmove;
		history.pop_back();
		Position::UnmakeMove(move);
		assert(history[history.size() - 1].board_key == get_board_key());
	}
	
	CycleFlag State::cycle_flag()const{
		//現局面のハッシュ値がhistory[cur_idx]に格納されている
		uint64_t cur_key = get_board_key();
		int cur_idx = history.size() - 1;
		for(int i=cur_idx - 4, e = std::max(cur_idx - 24, 0); i >= e; i-=2){
			if(history[i].board_key == cur_key){
				switch(get_hand(turn()).relation(history[i].hand_counts)){
				case HandRelEqual:
					//王手千日手の判定
					{
						bool check_rep = true;
						for(int j=cur_idx; j>= i && check_rep; j-=2){
							check_rep &= history[j].check;
						}
						if(check_rep)return CheckRepWin;
						check_rep = true;
						for(int j=cur_idx - 1; j >= i && check_rep; j-=2){
							check_rep &= history[j].check;
						}
						if(check_rep)return CheckRepLose;
						return DrawRep;
					}
				case HandRelSuperior:
					return SupRep;
				case HandRelLesser:
					return LesserRep;
				case HandRelNone:
					break;
				}
			}
		}
		return NoRep;
	}
	int State::dynamic_exchange_evaluation(Move move){
		int ret = move.estimate();
		Position::MakeMove(move);
		int n_moves;
		MoveArray moves;
		if(check()){
			n_moves = generate_evasion(moves);
		}
		else{
			n_moves = generate_capture(moves, 0);
		}
		int v = 0;
		for(int i=0;i<n_moves;i++){
			if(v < moves[i].estimate()){
				v = std::max(v, see(moves[i]));
			}
		}
		Position::UnmakeMove(move);
		return ret - v;
	}
}