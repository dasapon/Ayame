#include "move_ordering.hpp"

namespace shogi{
	MoveOrdering::MoveOrdering(State& state, Move hash_move, const KillerMove& killer_move, bool rps, bool do_fp)
	:state(state),  killer_move(killer_move), hash_move(hash_move), move_count(0), n_moves(0), do_fp(do_fp){
		if(rps){
			generate_rps();
		}
		else if(state.check()){
			generate_evasion();
		}
		else{
			status = Hash;
			if(state.is_move_valid(hash_move)){
				moves[0] = hash_move;
				n_moves = 1;
			}
		}
	}
	MoveOrderingQSearch::MoveOrderingQSearch(State& state, Move hash_move):move_count(1){
		scores[0] = INT_MAX;
		if(state.check()){
			n_moves = state.generate_evasion(moves, 1);
			//SEEでのオーダリング
			for(int i=1;i<n_moves;i++){
				if(moves[i] == hash_move)scores[i] = 4096 * 256;
				else scores[i] = state.see(moves[i]);
			}
		}
		else{
			n_moves = state.generate_capture(moves, 1);
			//駒を取らない歩成を生成
			n_moves = state.generate_nocapture_promotion<Pawn>(moves, n_moves);
			//オーダリング
			for(int i=1;i<n_moves;i++){
				if(moves[i] == hash_move)scores[i] = 4096 * 256;
				else scores[i] = moves[i].mvv_lva();
			}
		}
		//ソート
		for(int i= 2; i < n_moves;i++){
			float s = scores[i];
			if(scores[i - 1] < s){
				int j = i;
				Move m = moves[i];
				do{
					scores[j] = scores[j - 1];
					moves[j] = moves[j - 1];
					j--;
				}while(scores[j - 1] < s);
				scores[j] = s;
				moves[j] = m;
			}
		}
		moves[n_moves] = Move::null_move();
	}
	Move MoveOrdering::next_move(float* pscore){
		while(true){
			if(move_count < n_moves){
				*pscore = scores[move_count];
				return moves[move_count++];
			}
			if(status == All)return Move::null_move();
			generate_next();
		}
	}
	Move MoveOrderingQSearch::next_move(){
		return moves[move_count++];
	}
	void MoveOrdering::generate_next(){
		assert(!state.check());
		//番兵のセット
		n_moves = move_count = 1;
		scores[0] = FLT_MAX;
		switch(status){
		case Hash://取る手及びgood killerを生成する
			status = Capture;
			move_probability.init<true>(state);
			n_moves = state.generate_capture(moves, n_moves);
			if(state.is_move_valid(killer_move[0]) && killer_move[0] != hash_move){
				moves[n_moves++] = killer_move[0];
			}
			for(int i=move_count; i < n_moves; i++){
				if(moves[i] == hash_move){
					//ハッシュの手の除去
					moves[i--] = moves[--n_moves];
				}
				else{
					//着手確率によるオーダリング
					scores[i] = move_probability.score<true>(state, moves[i]);
				}
			}
			insertion_sort_v2();
			break;
		case Capture:
			status = Killer;
			if(state.is_move_valid(killer_move[1]) && killer_move[1] != hash_move){
				moves[n_moves++] = killer_move[1];
			}
			break;
		case Killer://残りの手をすべて生成する
			//駒打ちは, futility pruningを行う時王手のみ生成、futility pruningを行わない時は全てを生成
			//駒移動は,一旦全ての手を生成する.futility_pruningを行う場合は, その後不要な手を削除する
			if(do_fp){
				int nocapture_start = state.generate_drop_check(moves, n_moves);
				n_moves = state.generate_nocapture(moves, nocapture_start);
				for(int i=nocapture_start; i<n_moves;i++){
					if(!moves[i].is_promotion() && !state.is_move_check(moves[i])){
						moves[i--] = moves[--n_moves];
					}
				}
			}
			else{
				n_moves = state.generate_nocapture(moves, n_moves);
				if(state.turn() == First)n_moves = state.generate_drop<First>(moves, n_moves);
				else n_moves = state.generate_drop<Second>(moves, n_moves);
			}
			//ハッシュの手、キラー手の除去,スコア付け
			for(int i=move_count; i < n_moves; i++){
				if(moves[i] == hash_move || moves[i] == killer_move[0] || moves[i] == killer_move[1]){
					moves[i--] = moves[--n_moves];
				}
				else{
					scores[i] = move_probability.score<true>(state, moves[i]);
				}
			}
			//オーダリング
			insertion_sort_v2();
			status = All;
			break;
		default:
			assert(false);
		}
	}
	void MoveOrdering::generate_rps(){
		status = All;
		move_probability.init<false>(state);
		if(state.turn() == First) n_moves = state.generate_moves<First>(moves);
		else n_moves = state.generate_moves<Second>(moves);
		//着手確率計算
		int hash_move_idx = -1;
		for(int i=0;i<n_moves;i++){
			if(moves[i] == hash_move)hash_move_idx = i;
			scores[i] = move_probability.score<false>(state, moves[i]) * move_probability::score_scale;
		}
		sheena::softmax<MaxLegalMove>(scores, n_moves);
		if(hash_move_idx > 0){
			float hash_move_score = scores[hash_move_idx];
			moves[hash_move_idx] = moves[0];
			scores[hash_move_idx] = scores[0];
			moves[0] = hash_move;
			scores[0] = hash_move_score;
		}
		if(do_fp){
			for(int i=0;i<n_moves;i++){
				if(!moves[i].is_capture()
				&& !moves[i].is_promotion()
				&& moves[i] != hash_move
				&& moves[i] != killer_move[0]
				&& moves[i] != killer_move[1]
				&& !state.is_move_check(moves[i])){
					scores[i] = scores[--n_moves];
					moves[i--] = moves[n_moves];
				}
			}
		}
		if(hash_move_idx >= 0){
			move_count = 1;
			insertion_sort();
			move_count = 0;
		}
		else{
			insertion_sort();
		}
	}
	void MoveOrdering::generate_evasion(){
		status = All;
		//番兵を置く
		n_moves = move_count = 1;
		scores[0] = FLT_MAX;
		n_moves = state.generate_evasion(moves, 1);
		if(n_moves <= 2)return;
		//オーダリング
		move_probability.init<true>(state);
		for(int i=1;i<n_moves;i++){
			if(moves[i] == hash_move)scores[i] = FLT_MAX;
			else scores[i] = move_probability.score<true>(state, moves[i]);
		}
		insertion_sort_v2();
	}
	void MoveOrdering::insertion_sort(){
		for(int i= move_count + 1; i < n_moves;i++){
			float s = scores[i];
			if(scores[i - 1] < s){
				int j = i;
				Move m = moves[i];
				do{
					scores[j] = scores[j - 1];
					moves[j] = moves[j - 1];
				}while(--j > move_count && scores[j - 1] < s);
				scores[j] = s;
				moves[j] = m;
			}
		}
	}
	void MoveOrdering::insertion_sort_v2(){
		for(int i= 2; i < n_moves;i++){
			float s = scores[i];
			if(scores[i - 1] < s){
				int j = i;
				Move m = moves[i];
				do{
					scores[j] = scores[j - 1];
					moves[j] = moves[j - 1];
					--j;
				}while(scores[j - 1] < s);
				scores[j] = s;
				moves[j] = m;
			}
		}
	}
}