#include "search.hpp"
#include "move_ordering.hpp"

namespace shogi{
	static constexpr int delta_margin = 300;
	sheena::Array<int, 2> rfp_margin({500, 0}), fp_margin_pv({300, 128}), fp_margin_nonpv({200, 128});
	constexpr int rfp_depth = 3 * DepthScale, rps_depth = 4 * DepthScale;
	constexpr int fp_depth_pv = 3 * DepthScale, fp_depth_nonpv = 6 * DepthScale;
	sheena::Array<int, 5> aspiration_window({
		PawnValue, PawnValue, PawnValue * 2, PawnValue * 3, 2 * MateValue
	});
	static int futility_margin(bool pv, int depth){
		if(pv){
			if(depth < fp_depth_pv)return fp_margin_pv[0] + fp_margin_pv[1] * depth / DepthScale;
		}
		else{
			if(depth < fp_depth_nonpv)return fp_margin_nonpv[0] + fp_margin_nonpv[1] * depth / DepthScale;
		}
		return 2 * MateValue;
	}
	static int reverse_futility_margin(int depth){
		return rfp_margin[0] + rfp_margin[1] * depth / DepthScale;
	}
	template<size_t Threads>
	int SearchCore<Threads>::qsearch(State& state, int alpha, int beta, int depth, int ply, int thread_id){
		const int old_alpha = alpha;
		int ret = INT_MIN;
		Move hash_move = Move::null_move();
		Move bestmove = Move::null_move();
		pv_table[thread_id][ply][ply] = bestmove;
		bool check = state.check();
		//深さ下限の検出
		if(ply >= MaxPly){
			return evaluate(state);
		}
		if(depth < LowerLimitDepth){
			if(!check)return evaluate(state);
			else depth += DepthScale;
		}
		//ループ検出
		if(ply > 0){
			CycleFlag cycle_flag = state.cycle_flag();
			switch(cycle_flag){
			case NoRep:
				break;
			case CheckRepWin:
				return MateValue;
			case SupRep:
				alpha = std::max(alpha, +SupValue);
				ret = std::max(ret, +SupValue);
				if(ret >= beta)return ret;
				break;
			case CheckRepLose:
			case LesserRep:
				if(alpha >= -SupValue){
					return -SupValue;
				}
				break;
			case DrawRep:
				return 0;
			}
		}
		//置換表の参照
		HashEntry hash_entry;
		if(hash_table.probe(state, &hash_entry)){
#ifndef LEARN
			int hash_value = hash_entry.value();
			ValueType vt = hash_entry.value_type();
			int hash_depth = hash_entry.depth();
			bool hash_cut = false;
			if(hash_value > SupValue)hash_value -= ply;
			if(hash_value < -SupValue)hash_value += ply;
			if(hash_depth >= depth || std::abs(hash_value) >= MateValue){
				if(hash_value >= beta)hash_cut = vt != UpperBound;
				else if(hash_value <= alpha)hash_cut = vt != LowerBound;
				else hash_cut = vt == Exact;
			}
			if(hash_cut){
				hash_table.store(state, hash_entry);
				return hash_value;
			}
#endif
			hash_move = hash_entry.move(state);
		}
		else if(!check){
			//1手詰め
			if(state.Mate1Ply(&bestmove)){
				pv_table[thread_id][ply][ply] = bestmove;
				pv_table[thread_id][ply][ply + 1] = Move::null_move();
				return value_mate_in(ply + 1);
			}
			//宣言勝ち
			if(state.nyugyoku_declaration()){
				pv_table[thread_id][ply][ply] = Move::null_move();
				return value_mate_in(ply + 1);
			}
		}
		int stand_pat = evaluate(state);
		if(!check){
			ret = stand_pat;
			if(stand_pat >= alpha){
				alpha = stand_pat;
				if(stand_pat >= beta)return stand_pat;
			}
		}
		MoveOrderingQSearch move_ordering(state, hash_move);
		while(true){
			Move move = move_ordering.next_move();
			if(!move)break;
			//枝刈り
			if(!check){
				//delta pruning
				int upper = delta_margin + move.estimate();
				if(stand_pat + upper <= alpha){
					//王手は枝刈りしない
					if(!state.is_move_check(move)){
						ret = std::max(ret, stand_pat + upper);
						continue;
					}
				}
				//SEEによる枝刈り
				else if(exchange_value[move.capture()] < exchange_value[move.piece_type()]){
					if(!state.is_move_check(move) && state.see(move) < 0)continue;
				}
			}
			nodes[thread_id]++;
			hash_table.prefetch(state, move);
			state.make_move(move);
			int v = -qsearch(state, -beta, -alpha, depth - DepthScale, ply + 1, thread_id);
			state.unmake_move();
			if(v > ret){
				ret = v;
				alpha = std::max(alpha, v);
				bestmove = move;
				pv_table[thread_id][ply] = pv_table[thread_id][ply + 1];
				pv_table[thread_id][ply][ply] = move;
				if(alpha >= beta)break;
			}
		}
		ret = std::max(-value_mate_in(ply), ret);
		//置換表への記録
		int hash_value = ret;
		if(hash_value > SupValue)hash_value += ply;
		if(hash_value < -SupValue)hash_value -= ply;
		hash_table.store(state, old_alpha, beta, hash_value, bestmove, depth);
		return ret;
	}
	template<size_t Threads>
	int SearchCore<Threads>::search(State& state, int alpha, int beta, int depth, int ply, int thread_id){
		if(depth < DepthScale){
			return qsearch(state, alpha, beta, 0, ply, thread_id);
		}
		const int old_alpha = alpha;
		int ret = INT_MIN;
		Move hash_move = Move::null_move();
		Move bestmove = Move::null_move();
		if(ply != 0)pv_table[thread_id][ply][ply] = bestmove;
		killer_move[thread_id][ply + 2].clear();
		bool check = state.check();
		bool is_pv = beta - alpha > 1;
		//ループ検出
		if(ply > 0){
			CycleFlag cycle_flag = state.cycle_flag();
			switch(cycle_flag){
			case NoRep:
				break;
			case CheckRepWin:
				return MateValue;
			case SupRep:
				alpha = std::max(alpha, +SupValue);
				ret = std::max(ret, +SupValue);
				if(ret >= beta)return ret;
				break;
			case CheckRepLose:
			case LesserRep:
				if(alpha >= -SupValue){
					return -SupValue;
				}
				break;
			case DrawRep:
				return 0;
			}
		}
		//置換表の参照
		HashEntry hash_entry;
		if(hash_table.probe(state, &hash_entry)){
			ValueType vt = hash_entry.value_type();
#ifndef LEARN
			int hash_depth = hash_entry.depth();
			bool hash_cut = false;
			if(ply > 0){
				int hash_value = hash_entry.value();
				if(hash_value > SupValue)hash_value -= ply;
				if(hash_value < -SupValue)hash_value += ply;
				if(hash_depth >= depth || std::abs(hash_value) >= MateValue){
					if(hash_value >= beta)hash_cut = vt != UpperBound;
					else if(hash_value <= alpha)hash_cut = vt != LowerBound;
					else hash_cut = vt == Exact;
				}
				if(hash_cut){
					pv_table[thread_id][ply][ply] = hash_entry.move(state);
					pv_table[thread_id][ply][ply + 1] = Move::null_move();
					hash_table.store(state, hash_entry);
					return hash_value;
				}
			}
#endif
			if(vt != UpperBound){
				hash_move = hash_entry.move(state);
			}
		}
		else if(!check){
			//1手詰め
			if(state.Mate1Ply(&bestmove)){
				pv_table[thread_id][ply][ply] = bestmove;
				pv_table[thread_id][ply][ply + 1] = Move::null_move();
				return value_mate_in(ply + 1);
			}
			//宣言勝ち
			if(state.nyugyoku_declaration()){
				pv_table[thread_id][ply][ply] = Move::null_move();
				return value_mate_in(ply + 1);
			}
		}
		//評価値の取得
		int stand_pat = evaluate(state);
		//reverse futility pruning
		if(depth < rfp_depth
		&& !check
		&& !is_pv){
			int v = stand_pat - reverse_futility_margin(depth);
			if(v >= beta)return v;
		}
		//null move pruning
		if(depth >= 2 * DepthScale
		&& !check
		&& stand_pat >= beta
		&& !is_pv
		&& state.previous_move(0)){
			nodes[thread_id]++;
			state.make_move(Move::null_move());
			int v = -search(state, -beta, 1-beta, depth - 5 * DepthScale, ply + 1, thread_id);
			state.unmake_move();
			if(v >= beta){
				hash_table.store(state, old_alpha, beta, v, Move::null_move(), depth);
				return v;
			}
		}
		//再帰的反復深化
		if(depth >= 5 * DepthScale
		&& !hash_move){
			int r = 3 * DepthScale;
			if(!is_pv)r += DepthScale;
			//int x = nodes[thread_id];
			int v = search(state, alpha, beta, depth - r, ply, thread_id);
			//nodes[thread_id] = x;
			if(std::abs(v) >= MateValue)return v;
			hash_move = pv_table[thread_id][ply][ply];
		}
		int move_count = 0;
		int fut_margin = futility_margin(is_pv, depth);
		bool do_fp = !check && stand_pat + futility_margin(is_pv, depth) <= alpha;
		if(do_fp){
			ret = std::max(fut_margin + stand_pat, ret);
		}
		const bool rps = depth >= rps_depth;
		MoveOrdering move_ordering(state, hash_move, killer_move[thread_id][ply], rps, do_fp);
		if(tle.load() || abort[thread_id].load())return ret;
		while(true){
			float score;
			Move move = move_ordering.next_move(&score);
			if(!move)break;
			bool important_move = check || is_pv || move.is_capture() || move.is_promotion() || state.is_move_check(move);
			//late move pruning
			if( !important_move
				&& move_count >= 24 + depth * 4 / DepthScale
				&& ret > -MateValue){
				continue;
			}
			int consume = DepthScale;
			if(rps){
				consume = -std::log2(score) * DepthScale;
				if(move_count == 0){
					consume = std::min(consume, +DepthScale);
				}
				else{
					consume = std::min(consume, depth - DepthScale);
				}
			}
			else{
				if(move_count >= 7
				&& depth >= 2 * DepthScale
				&& !move.is_capture()
				&& !move.is_promotion()){
					//late move reduction
					consume = 2 * DepthScale;
				}
				else if(state.is_move_check(move)){
					consume = 0;
					if(state.see(move) < 0)consume = DepthScale / 2;
				}
			}
			hash_table.prefetch(state, move);
			state.make_move(move);
			nodes[thread_id]++;
			int v;
			if(move_count == 0){
				v = -search(state, -beta, -alpha, depth - consume, ply + 1, thread_id);
			}
			else{
				v = -search(state, -alpha - 1, -alpha, depth - consume, ply + 1, thread_id);
				//再探索
				if(alpha < v && consume > DepthScale){
					consume = DepthScale;
					v = -search(state, -alpha - 1, -alpha, depth - consume, ply + 1, thread_id);
				}
				if(alpha < v && v < beta){
					v = -search(state, -beta, -alpha, depth - consume, ply + 1, thread_id);
				}
			}
			move_count++;
			state.unmake_move();
			//探索終了フラグのチェック
			if(tle.load() || abort[thread_id].load())return ret;
			if(v > ret){
				ret = v;
				alpha = std::max(alpha, v);
				bestmove = move;
				pv_table[thread_id][ply] = pv_table[thread_id][ply + 1];
				pv_table[thread_id][ply][ply] = move;
				if(alpha >= beta){
					break;
				}
			}
		}
		ret = std::max(-value_mate_in(ply), ret);
		//置換表への記録
		int hash_value = ret;
		if(hash_value > SupValue)hash_value += ply;
		if(hash_value < -SupValue)hash_value -= ply;
		hash_table.store(state, old_alpha, beta, hash_value, bestmove, depth);
		//killerの更新
		if(!check && ret > old_alpha && !bestmove.is_capture()){
			if(killer_move[thread_id][ply][0] != bestmove){
				killer_move[thread_id][ply][1] = killer_move[thread_id][ply][0];
				killer_move[thread_id][ply][0] = bestmove;
			}
		}
		return ret;
	}
	int Searcher::search(State& state, int alpha, int beta, int depth, std::vector<MTMove>& moves, int thread_id){
		const int old_alpha = alpha;
		int ret = INT_MIN;
		Move bestmove = Move::null_move();
		killer_move[thread_id][2].clear();
		for(int move_count=0; move_count < moves.size(); move_count++){
			float score = moves[move_count].probability;
			Move move = moves[move_count].move;
			if(!move)break;
			//探索深さ決定
			int consume = -std::log2(score) * DepthScale;
			if(move_count == 0){
				consume = std::min(consume, +DepthScale);
			}
			else{
				consume = std::min(consume, depth - DepthScale);
			}
			hash_table.prefetch(state, move);
			state.make_move(move);
			nodes[thread_id]++;
			int v;
			if(move_count == 0){
				v = -SearchCore::search(state, -beta, -alpha, depth - consume, 1, thread_id);
			}
			else{
				v = -SearchCore::search(state, -alpha - 1, -alpha, depth - consume, 1, thread_id);
				//再探索
				if(alpha < v && consume > DepthScale){
					consume = DepthScale;
					v = -SearchCore::search(state, -alpha - 1, -alpha, depth - consume, 1, thread_id);
				}
				if(alpha < v && v < beta){
					v = -SearchCore::search(state, -beta, -alpha, depth - consume, 1, thread_id);
				}
			}
			state.unmake_move();
			//探索終了フラグのチェック
			if(tle.load() || abort[thread_id].load())return ret;
			moves[move_count].value = v;
			//時間制御
			if(thread_id == 0 && v > ret){
				float d = 1 - score;
				if(v <= alpha || v >= beta){
					d = std::min(1.0f, d * 1.5f);
				}
				difficulty.store(d);
			}
			if(v <= alpha && v > -SupValue){
				moves[move_count].value = 1-SupValue;
			}
			if(v > ret){
				ret = v;
				alpha = std::max(alpha, v);
				bestmove = move;
				pv_table[thread_id][0] = pv_table[thread_id][1];
				pv_table[thread_id][0][0] = move;
				if(alpha >= beta){
					break;
				}
			}
		}
		ret = std::max(-MateValue, ret);
		//置換表への記録
		hash_table.store(state, old_alpha, beta, ret, bestmove, depth);
		return ret;
	}
	template<size_t Threads>
	int SearchCore<Threads>::qsearch(State& state, PV& pv){
		new_search();
		int ret = qsearch(state, -MateValue, MateValue, 0, 0, 0);
		pv = pv_table[0][0];
		return ret;	
	}
	template<size_t Threads>
	int SearchCore<Threads>::search(State& state, PV& pv, int depth_limit){
		assert(Threads == 1);
		new_search();
		int ret = 0;
		//反復深化
		for(int d = 1; d <= depth_limit; d++){
			int v = search(state, -MateValue, MateValue, d * DepthScale, 0, 0);
			if(v != INT_MIN)ret = v;
			//詰みの値になっている場合は終了
			if(std::abs(ret) >= MateValue)break;
		}
		pv = pv_table[0][0];
		return ret;
	}
	int Searcher::iterative_deepning(State& state, int depth_limit, PV& pv){
		int ret = 0;
		sheena::Stopwatch stopwatch;
		difficulty.store(1.0f);
		//rootmoveの作成
		std::vector<std::vector<MTMove>> root_moves(nthreads);
		std::cout << "info string";
		{
			KillerMove dummy;
			dummy.clear();
			MoveOrdering ordering(state, Move::null_move(), dummy, true, false);
			while(true){
				float score;
				Move mv = ordering.next_move(&score);
				if(!mv)break;
				if(root_moves[0].size() < 8){
					std::cout << " (" + mv.string() << ", " << score << "),";
				}
				root_moves[0].push_back(MTMove(mv, score, 0));
			}
		}
		std::cout << std::endl;
		//ヘルパー用に局面初期化
		for(int i=0;i<nthreads;i++){
			states[i] = state;
		}
		//メインスレッドの探索
		threads[0] = std::thread([&](){
			int v = 0;
			//制限時間まで探索
			for(int d=1; d<32 && !tle.load();d++){
				int alpha = -MateValue, beta = MateValue;
				int fail_low = 0, fail_high = 0;
				if(d > 1){
					alpha = std::max(-MateValue, v - aspiration_window[fail_low]);
					beta = std::min(+MateValue, v + aspiration_window[fail_high]);
				}
				while(true){
					//探索のスレッド数
					int nthreads = std::min(d, this->nthreads);
					//ヘルパーの探索開始
					for(int i=1;i<nthreads;i++){
						abort[i].store(false);
						root_moves[i] = root_moves[0];
						threads[i] = std::thread([&](int thread_id){
							search(states[thread_id], alpha, beta, (d + thread_id) * DepthScale, root_moves[thread_id], thread_id);
						}, i);
					}
					//メインスレッドの探索
					abort[0].store(false);
					v = search(state, alpha, beta, d * DepthScale, root_moves[0], 0);
					//ヘルパースレッドの探索終了
					for(int i=1;i<nthreads;i++){
						abort[i].store(true);
					}
					for(int i=1;i<threads.size();i++){
						if(threads[i].joinable())threads[i].join();
					}
					if(v >= MateValue || v <= -MateValue)break;
					if(alpha >= v){
						alpha = std::max(-MateValue, v - aspiration_window[++fail_low]);
					}
					else if(v >= beta){
						beta = std::min(+MateValue, v + aspiration_window[++fail_high]);
					}
					else{
						break;
					}
				}
				if(v < -MateValue)break;
				std::stable_sort(root_moves[0].begin(), root_moves[0].end(), [](const MTMove& lhs, const MTMove& rhs){
					return lhs.value > rhs.value;
				});
				float difficulty_temp = difficulty * 0.6;
				//強制手の判定
				if(d > 10 && (root_moves[0].size() < 2 || root_moves[0][1].value <= -SupValue)){
					difficulty_temp = 0;
				}
				if(terminate(difficulty_temp))time_up();
				pv = pv_table[0][0];
				//infoの出力
				ret = v;
				int64_t search_time = stopwatch.msec();
				uint64_t nodes = nodes_searched();
				std::cout << "info depth "<< d;
				std::cout << " time " << search_time;
				std::cout << " hashfull "<< hash_table.hashfull();
				std::cout << " nodes " << nodes;
				if(search_time > 0)std::cout << " nps " << nodes * 1000 / search_time;
				std::cout << " score cp " << ret;
				if(pv[0]){
					std::cout << " pv";
					for(int ply=0;ply<MaxPly && pv[ply]; ply++){
						std::cout << " " << pv[ply].string();
					}
				}
				std::cout << std::endl;
			}
			difficulty.store(0);
		});
		//時間制御
		bool pseudo_book_move = state.ply() <= 20;
		while(true){
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			float d = difficulty.load();
			if(pseudo_book_move)d *= 0.1;
			if(terminate(d))break;
		}
		time_up();
		//メイン終了待ち
		threads[0].join();
		return ret;
	}
	template<size_t Threads>
	void SearchCore<Threads>::new_search(){
		for(int th=0;th < Threads; th++){
			nodes[th] = 0;
			killer_move[th][0].clear();
			killer_move[th][1].clear();
			pv_table[th][0][0] = Move::null_move();
		}
		tle.store(false);
		hash_table.new_gen();
	}

	template<size_t Threads>
	SearchCore<Threads>::SearchCore():pv_table(Threads){
		nthreads = 1;
	}
	void Searcher::go(State& state){
		if(search_thread.joinable())search_thread.join();
		new_search();
		search_thread = std::thread([&]{
			PV pv;
			bool no_think = false;
			if(!state.check()){
				//1手詰み
				if(state.Mate1Ply(&pv[0])){
					std::cout << "info score cp 32000 string checkmate" << std::endl;
					no_think = true;
				}
				else if(state.nyugyoku_declaration()){
					std::cout << "info score cp 32000 string declare win" << std::endl;
					no_think = true;
					pv[0] = Move::win();
				}
				pv[1] = Move::null_move();
			}
			//一定確率でランダムに着手する.
			//レベルが高いほどランダム着手をする確率が低い
			if(!no_think
				&& level < random(mt)){
				//先読みなしで着手決定
				KillerMove killer;
				killer.clear();
				MoveOrdering ordering(state, Move::null_move(), killer, true, false);
				std::uniform_real_distribution<double> dist(0, 1.0);
				float r = dist(mt);
				while(true){
					float score;
					Move move = ordering.next_move(&score);
					if(!move)break;
					r -= score;
					if(r <= 0){
						pv[0] = move;
						pv[1] = Move::null_move();
						no_think = true;
						break;
					}
				}
			}
			if(!no_think) iterative_deepning(state, 64, pv);
			//terminateがtrueになるのを待つ(ponderやinfinite中に値を返さないよう)
			set_terminated();
			while(!terminate(0)){
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}
			std::cout << "bestmove " << pv[0].string();
			if(pv[0] && pv[1])std::cout << " ponder " << pv[1].string();
			std::cout << std::endl;
		});
	}
	template class SearchCore<1>;
	template class SearchCore<64>;
}