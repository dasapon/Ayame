#include "state.hpp"
#include "learn.hpp"
#include "probability.hpp"
#include <typeinfo>

namespace shogi::move_probability{
	sheena::Array<Weight<Feature>, Feature::Dim> Feature::table;
	sheena::Array<Weight<SimpleFeature>, SimpleFeature::Dim> SimpleFeature::table;
	static sheena::Array2d<int, BoardSize, BoardSize> from_to_index_table;
	static sheena::Array2d<int, Sentinel, 2386> move_class_index_table;
	static sheena::Array<int, Sentinel> ptype10({
		0,	//空
		0, 1, 2, 3, 4, 5, 6, 9, 
		6, 6, 6, 6, 7, 8,
	});
	//2地点の相対位置
	static sheena::Array2d<int, BoardSize, BoardSize> sq_rel;
	static void init_move_class_index(){
		for(Square from = 0; from < BoardSize; from++){
			for(Square to = 0; to < BoardSize; to++){
				from_to_index_table[from][to] = -1;
			}
		}
		int idx = 0;
		for(Square from = 0; from < BoardSize; from++){
			if(sentinel_bb[from])continue;
			//8方向+桂馬の効き
			BitBoard bb = file_mask[from] | rank_mask[from];
			bb |= diag_mask[from] | diag2_mask[from];
			bb |= knight_attack[First][from];
			bb.foreach([&](Square to){
				from_to_index_table[from][to] = idx++;
			});
		}
		//駒打ち
		for(Square to = 0; to < BoardSize; to++){
			if(sentinel_bb[to])continue;
			from_to_index_table[SquareHand][to] = idx;
			from_to_index_table[flip(SquareHand)][to] = idx++;
		}
		if(idx != move_class_index_table[0].size()){
			std::cout << "error : move_classification index" << std::endl;
		}
		assert(idx == move_class_index_table[0].size());
		int cnt = 0;
		//駒打ち以外の着手
		BitBoard nopiece_bb = sentinel_bb;
		for(Square from = 0; from < BoardSize; from++){
			if(sentinel_bb[from])continue;
			for(PieceType pt = Pawn; pt < Sentinel; pt++){
				BitBoard attack = attack_bb(First, pt, from, nopiece_bb);
				attack.foreach([&](Square to){
					const int ft_index = from_to_index_table[from][to];
					if(ft_index < 0){
						std::cout << "Invalid" << std::endl;
						std::exit(0);
					}
					//成銀には金と同じ番号を割り当てる
					if(pt == ProSilver){
						move_class_index_table[pt][ft_index] = move_class_index_table[Gold][ft_index];
					}
					//成桂には成香と同じ番号を割り当てる
					else if(pt == ProKnight){
						move_class_index_table[pt][ft_index] = move_class_index_table[ProLance][ft_index];
					}
					//5段目以降のと金、成香には、金と同じ番号を割り当てる
					else if(ProPawn <= pt && pt <= ProLance
					&& squareX(from) >= (xy_start + xy_end) / 2){
						move_class_index_table[pt][ft_index] = move_class_index_table[Gold][ft_index];
					}
					else if(must_promote(First, pt, to)){
						move_class_index_table[pt][ft_index] = cnt - 1;
						cnt++;
					}
					else{
						move_class_index_table[pt][ft_index] = cnt++;
						if(can_promote(First, pt, from, to)){
							cnt++;
						}
					}
				});
			}
		}
		//駒打ち
		for(Square to = 0; to < BoardSize; to++){
			for(PieceType pt = Pawn; pt < King; pt++){
				const int ft_index = from_to_index_table[SquareHand][to];
				if(ft_index < 0)continue;
				if(!must_promote(First, pt, to)){
					move_class_index_table[pt][ft_index] = cnt++;
				}
			}
		}
	}
	static int move_class_index(Player pl, Move move){
		Square from = move.from(), to = move.to();
		if(pl == Second){
			from = flip(from);
			to = flip(to);
		}
		int ft_index = from_to_index_table[from][to];
		int ret = move_class_index_table[move.piece_type()][ft_index];
		if(move.is_promotion())ret++;
		return ret;
	}
	void load(){
		init_move_class_index();
		//sq_relの初期化
		for(Square sq1 = 0; sq1 < BoardSize; sq1++){
			if(sentinel_bb[sq1])continue;
			for(Square sq2 = 0; sq2 < BoardSize; sq2++){
				if(sentinel_bb[sq2])continue;
				int dx = squareX(sq1) - squareX(sq2);
				int dy = squareY(sq1) - squareY(sq2);
				sq_rel[sq1][sq2] = (dx + 8) * 17 + dy + 8;
			}
		}
		FILE* fp = fopen("probability.bin", "rb");
		bool ok = fp != nullptr;
		for(int i=0;i<Feature::table.size();i++){
			ok = ok && fread(&Feature::table[i], sizeof(int16_t), Weight<Feature>::size(), fp) == Weight<Feature>::size();
		}
		for(int i=0;i<SimpleFeature::table.size();i++){
			ok = ok && fread(&SimpleFeature::table[i], sizeof(int16_t), Weight<SimpleFeature>::size(), fp) == Weight<SimpleFeature>::size();
		}
		if(!ok){
			std::cout << "probability.bin can't be loaded" << std::endl;
		}
		if(fp != nullptr)fclose(fp);
	}
	void save(){
		FILE* fp = fopen("probability.bin", "wb");
		for(int i=0;i<Feature::table.size();i++){
			fwrite(&Feature::table[i], sizeof(int16_t), Weight<Feature>::size(), fp);
		}
		for(int i=0;i<SimpleFeature::table.size();i++){
			fwrite(&SimpleFeature::table[i], sizeof(int16_t), Weight<SimpleFeature>::size(), fp);
		}
		if(fp != nullptr)fclose(fp);
	}
	template<typename F>
	void Feature::proce_common(const State& state, const F& func){
		const EvalList& list = state.get_eval_list();
		if(state.check())func(Evasion);
		Player turn = state.turn();
		Player enemy = opponent(turn);
		Move lastmove = state.previous_move(0), secondlastmove = state.previous_move(1);
		if(lastmove){
			int idx = EvalIndex::on_board(enemy, lastmove.pt_promoted(), lastmove.to())[turn];
			func(PreviousMove + idx);
		}
		if(secondlastmove){
			int idx = EvalIndex::on_board(turn, secondlastmove.pt_promoted(), secondlastmove.to())[turn];
			func(PreviousMove + idx);
		}
		//
		sheena::Array<int, NumPiece> pieces;
		for(int i=0; i<NumPiece;i++){
			pieces[i] = list[i][turn];
		}
		std::sort(pieces.begin(), pieces.end(), std::greater<int>());
		for(int i=0;i<NumPiece;i++){
			int idx_base = pieces[i] * (pieces[i] - 1) / 2;
			for(int j=i+1;j<NumPiece;j++){
				func(PiecePair + idx_base + pieces[j]);
			}
		}
		//
		const int king_defence_base = KingDefence + (list[king_id(turn)][turn] - EvalKing) * 81 * 16;
		const int king_attack_base = KingDefence + (list[king_id(enemy)][turn] - EvalEnemyKing) * 81 * 16;
		for(int x=xy_start;x<xy_end;x++){
			for(int y=xy_start;y<xy_end;y++){
				Square sq = make_square(y, x);
				if(turn == Second)sq = flip(sq);
				int sq81 = (x-xy_start) * 9 + y - xy_start;
				int defence = std::min(3, state.control_count(turn, sq));
				int attack = std::min(3, state.control_count(enemy, sq));
				int control_idx = sq81 * 16 + defence * 4 + attack;
				func(king_defence_base + control_idx);
				func(king_attack_base + control_idx);
			}
		}
	}
	template<typename F>
	void SimpleFeature::proce_common(const State& state, const F& func){
		if(state.check())func(Evasion);
		Player turn = state.turn();
		Player enemy = opponent(turn);
		Move lastmove = state.previous_move(0), secondlastmove = state.previous_move(1);
		if(lastmove){
			int idx = EvalIndex::on_board(enemy, lastmove.pt_promoted(), lastmove.to())[turn];
			func(PreviousMove + idx);
		}
		if(secondlastmove){
			int idx = EvalIndex::on_board(turn, secondlastmove.pt_promoted(), secondlastmove.to())[turn];
			func(PreviousMove + idx);
		}
		const EvalList& list = state.get_eval_list();
		for(int i=0;i<NumPiece;i++){
			func(PieceSquare + list[i][turn]);
		}
	}
	template<typename Ty>
	void Evaluator::init_internal(const State& state){
		if(typeid(Ty) == typeid(Feature)){
			v_common.clear();
			Feature::proce_common(state, [&](int idx){
				v_common += Feature::table[idx];
			});
		}
		else{
			v_common_simple.clear();
			SimpleFeature::proce_common(state, [&](int idx){
				v_common_simple += SimpleFeature::table[idx];
			});
		}
	}
	template<bool fast>
	void Evaluator::init(const State& state){
		if(fast){
			init_internal<SimpleFeature>(state);
		}
		else{
			init_internal<Feature>(state);
		}
	}
	template void Evaluator::init<false>(const State&);
	template void Evaluator::init<true>(const State&);
	static int see_index(int see){
		int ret = (see + 64 * SeeFeatureDim) / 128;
		if(ret < 0)return 0;
		if(ret >= SeeFeatureDim)return SeeFeatureDim - 1;
		return ret;
	}
	const sheena::Array<int, Sentinel> Feature::attack_index({
		0, 
		Attack,//歩
		Attack + EvalIndexWithKingDim * 4,//香車
		Attack + 2 * EvalIndexWithKingDim * 4,//桂馬
		Attack + 3 * EvalIndexWithKingDim * 4,//銀
		Attack + 4 * EvalIndexWithKingDim * 4,//角
		Attack + 5 * EvalIndexWithKingDim * 4,//飛車
		Attack + 6 * EvalIndexWithKingDim * 4,//金
		Attack + 9 * EvalIndexWithKingDim * 4,//龍
		Attack + 6 * EvalIndexWithKingDim * 4,//と金
		Attack + 6 * EvalIndexWithKingDim * 4,//成香
		Attack + 6 * EvalIndexWithKingDim * 4,//成桂
		Attack + 6 * EvalIndexWithKingDim * 4,//成銀
		Attack + 7 * EvalIndexWithKingDim * 4,//馬
		Attack + 8 * EvalIndexWithKingDim * 4,//龍
	});
	template<typename F>
	void Feature::proce(State& state, Move move, const F& func){
		const Player pl = state.turn();
		const PieceType pt = move.piece_type();
		const PieceType pt_promoted = move.pt_promoted();
		const PieceType cap = move.capture();
		const Square from = move.from(), to = move.to();
		const bool check = state.is_move_check(move);
		//SEEの値
		int see = state.see(move);
		func(See + see_index(see));
		if(see >= 0){
			int dee = state.dynamic_exchange_evaluation(move);
			func(Dee + see_index(dee));
		}
		//取る駒の種類 x 王手
		{
			int idx = CheckAndCapture + cap;
			if(check)idx += Sentinel;
			if(state.previous_move(0).to() == to){
				idx += 2 * Sentinel;
			}
			func(idx);
		}
		//直前の手との位置関係
		Move lastmove = state.previous_move(0);
		if(lastmove){
			Square last_to = lastmove.to();
			int last_pt = ptype10[lastmove.pt_promoted()];
			int rel;
			if(pl == First){
				rel = sq_rel[last_to][to];
			}
			else{
				rel = sq_rel[to][last_to];
			}
			func(RelationLast + (last_pt * 10 + ptype10[pt_promoted]) * 289 + rel);
			if(!move.is_drop()){
				if(pl == First){
					rel = sq_rel[last_to][from];
				}
				else{
					rel = sq_rel[from][last_to];
				}
				func(RelationLastF + (last_pt * 10 + ptype10[pt]) * 289 + rel);
			}
		}
		//移動先と玉の位置関係
		{
			sheena::Array<int, PlayerDim> king_rel_to;
			king_rel_to[First] = sq_rel[to][state.king_sq(First)];
			king_rel_to[Second] = sq_rel[state.king_sq(Second)][to];
			func(KPFriendTo + pt_promoted * 289 + king_rel_to[pl]);
			func(KPEnemyTo + pt_promoted * 289 + king_rel_to[opponent(pl)]);
		}
		//移動先周辺8マスの駒
		for(Dir dir = NW; dir <= SE; dir++){
			Square sq = to + dir_diff[dir] * player2sign(pl);
			Piece p = state.piece_on(sq);
			if(!p.empty() && p.type() != Sentinel){
				int idx = ptype10[pt_promoted] * 8 * 2 * 10 + dir * 20 + 2 * ptype10[p.type()] + (p.owner() ^ pl);
				func(Neighbor8To + idx);
			}
		}
		BitBoard attack = attack_bb(pl, pt_promoted, to, state.occupied());
		attack &= state.occupied();
		if(!move.is_drop()){
			attack.remove(square_bb[from]);
			attack.remove(attack_bb(pl, pt, from, state.occupied()));
			if(state.has_control(opponent(pl), from)){
				int idx = EvalIndex::on_board(pl, pt, from)[pl] * 2;
				if(state.has_control(pl, from))idx++;
				func(Escape + idx);
			}
			//移動元と玉の位置関係
			{
				sheena::Array<int, PlayerDim> king_rel_from;
				king_rel_from[First] = sq_rel[from][state.king_sq(First)];
				king_rel_from[Second] = sq_rel[state.king_sq(Second)][from];
				func(KPFriendFrom + pt * 289 + king_rel_from[pl]);
				func(KPEnemyFrom + pt * 289 + king_rel_from[opponent(pl)]);
			}
			//移動元周辺8マスの駒
			for(Dir dir = NW; dir <= SE; dir++){
				Square sq = from + dir_diff[dir] * player2sign(pl);
				Piece p = state.piece_on(sq);
				if(!p.empty() && p.type() != Sentinel){
					int idx = ptype10[pt] * 8 * 2 * 10 + dir * 20 + 2 * ptype10[p.type()] + (p.owner() ^ pl);
					func(Neighbor8From + idx);
				}
			}
		}
		//駒の効き
		const EvalList& elist = state.get_eval_list();
		int attack_idx = attack_index[pt_promoted];
		attack.foreach([&](Square sq){
			int id = state.piece_on(sq).id();
			int control = 0;
			if(state.has_control(pl, sq))control += 1;
			if(state.has_control(opponent(pl), sq))control += 2;
			func(attack_idx + elist[id][pl] * 4 + control);
		});
		//移動元, 移動先等
		func(MoveClass + move_class_index(state.turn(), move));
	}
	template<typename F>
	void SimpleFeature::proce(State& state, Move move, const F& func){
		const PieceType cap = move.capture();
		//SEEの値
		if(move.is_capture()){
			int see = state.see(move);
			func(See + see_index(see));
		}
		//取る駒の種類
		{
			int idx = Capture + cap;
			if(state.previous_move(0).to() == move.to()){
				idx += Sentinel;
			}
			func(idx);
		}
		func(MoveClass + move_class_index(state.turn(), move));	
	}
	template<typename Ty>
	int Evaluator::forward(State& state, Move move, Weight<Ty>& v)const{
		Ty::proce(state, move, [&](int idx){
			v += Ty::table[idx];
		});
		int ret = v[0];
		ret *= factorization_machine::Scale;
		v[0] = 0;
		ret += v.inner_product(v);
		return ret;
	}
	template<bool fast>
	int Evaluator::score(State& state, Move move)const{
		if(fast){
			Weight<SimpleFeature> v = v_common_simple;
			return forward<SimpleFeature>(state, move, v);
		}
		else {
			Weight<Feature> v = v_common;
			return forward<Feature>(state, move, v);
		}
	}
	template int Evaluator::score<false>(State&, Move)const;
	template int Evaluator::score<true>(State&, Move)const;
#ifdef LEARN
	template<typename Ty>
	void Evaluator::backward(RawTable<Ty>& grad, const RawTable<Ty>& raw_table, UpdateFlags<Ty>& update_flags, 
	State& state, Move move, const RawWeight<Ty>& v, float g)const{
		Ty::proce(state, move, [&](int idx){
			factorization_machine::update<Ty::InteractionDim>(grad[idx], raw_table[idx], v, g);
			update_flags.set(idx);
		});
	}
	template<typename Ty>
	void Evaluator::learn_one_pos(State& state, RawTable<Ty>& grad, const RawTable<Ty>& raw_table,
	UpdateFlags<Ty>& update_flags, Move bestmove, ClassificationStatistics& stats, double importance)const{
		MoveArray moves;
		sheena::Array<float, MaxLegalMove> scores;
		sheena::Array<RawWeight<Ty>, MaxLegalMove> v;
		int n_moves;
		if(state.turn() == First)n_moves = state.generate_moves<First>(moves);
		else n_moves = state.generate_moves<Second>(moves);
		float bestmove_score = 0;
		float othermove_score = -FLT_MAX;
		Weight<Ty> vi16_common;
		for(int i=0;i<vi16_common.size();i++){
			if(typeid(Ty) == typeid(Feature)){
				vi16_common[i] = v_common[i];
			}else{
				vi16_common[i] = v_common_simple[i];
			}
		}
		for(int i=0;i<n_moves;i++){
			Weight<Ty> vi16 = vi16_common;
			scores[i] = forward<Ty>(state, moves[i], vi16) * score_scale;
			v[i] = factorization_machine::convert_interactions<Ty::InteractionDim>(vi16);
			if(moves[i] == bestmove)bestmove_score = scores[i];
			else othermove_score = std::max(othermove_score, scores[i]);
		}
		float max_score = std::max(bestmove_score, othermove_score);
		//損失計算
		double denom = 0;
		for(int i=0;i<n_moves;i++){
			denom += std::exp(scores[i] - max_score);
		}
		double loss = -(bestmove_score - max_score - std::log(denom));
		stats.update(loss, bestmove_score > othermove_score);
		sheena::softmax<MaxLegalMove>(scores, n_moves);
		RawWeight<Ty> v_sum;
		v_sum.clear();
		for(int i=0;i<n_moves;i++){
			float g;
			if(moves[i] == bestmove)g = scores[i] - 1.0;
			else g = scores[i];
			backward<Ty>(grad, raw_table, update_flags, state, moves[i], v[i], g * importance);
			v_sum.add_product(v[i], g * importance);
		}
		//局面共通の特徴の勾配を更新
		v_sum[0] = 0;
		Ty::proce_common(state, [&](int idx){
			grad[idx] += v_sum;
			update_flags.set(idx);
		});
	}
	template<typename Ty>
	static void init_weight(RawTable<Ty>& raw_table, std::mt19937& mt){
		std::normal_distribution<double> dist(0, 0.01);
		for(int i=0;i<raw_table.size();i++){
			for(int j=0;j<RawWeight<Ty>::size();j++)raw_table[i][j] = dist(mt);
			Ty::table[i] = factorization_machine::convert<Ty::InteractionDim>(raw_table[i]);
		}
		for(int i=0;i<Ty::common_dim;i++){
			Ty::table[i][0] = 0;
		}
	}
	template<typename Ty>
	static void optimize(const std::string& log_file_name, DataSet& dataset, DataSet& dataset2){
		std::ofstream log_file(log_file_name);
		if(!log_file.is_open()){
			return;
		}
		constexpr int batch_size = 4096;
		constexpr int threads = 8;
		const double penalty = 0.01;
		std::mt19937 mt(0);
		sheena::ArrayAlloc<UpdateFlags<Ty>> updated(threads);
		sheena::ArrayAlloc<RawTable<Ty>> grads(threads), w_table(2);
		RawTable<Ty>& raw_table = w_table[0];
		RawTable<Ty>& global_grad = w_table[1];
		int n_epoch = 5;
		//各データセットのバッチサイズ決定
		const int64_t total_dataset_size = dataset.size() + dataset2.size() / (10 * n_epoch);
		const int batch1_size = dataset.size() * batch_size / total_dataset_size;
		const int batch2_size = dataset2.size() * batch_size / total_dataset_size / n_epoch;
		if(batch1_size + batch2_size == 0){
			std::cout << "no records" << std::endl;
			return;
		}
		size_t n_step = INT_MAX;
		if(batch1_size){
			n_step = std::min(dataset.size() / batch1_size, n_step);
		}
		if(batch2_size){
			n_step = std::min(dataset2.size() / n_epoch / batch2_size, n_step);
		}
		std::cout << "batch1_size = " << batch1_size << std::endl;
		std::cout << "batch2_size = " << batch2_size << std::endl;
		std::cout << "step/epoch = " << n_step << std::endl; 
		//重みの初期化
		init_weight<Ty>(raw_table, mt);
		sheena::Array<State, threads> states;
		for(int i=0;i<threads; i++){
			memset(&grads[i], 0, sizeof(RawTable<Ty>));
		}
		memset(&global_grad, 0, sizeof(RawTable<Ty>));
		sheena::Stopwatch stopwatch;
		ExpAverage loss_exp_ave(0.001), accuracy_exp_ave(0.001);
		ExpAverage loss_exp_ave2(0.001), accuracy_exp_ave2(0.001);
		int64_t total_step = 0;
		dataset2.shuffle(mt);
		std::cout << " learning start"  << std::endl;
		for(int epoch=0;epoch< n_epoch;epoch++){
			double learning_rate = Ty::learning_rate_base / batch_size;
			if(epoch > 1)learning_rate /= 10;
			if(epoch > 3)learning_rate /= 10;
			dataset.shuffle(mt);
			for(int step = 0;step < n_step; step++, total_step++){
				bool write_log = (total_step + 1) % 1000 == 0;
				sheena::Array<ClassificationStatistics, threads> stats, stats2;
				omp_set_num_threads(threads);
#pragma omp parallel
				{
					//重要度の高い棋譜
#pragma omp for schedule(guided) nowait
					for(int i=step * batch1_size; i<(step + 1) * batch1_size; i++){
						int thread_id = omp_get_thread_num();
						//局面のセット
						State& state = states[thread_id];
						Label label = dataset.get(state, i);
						//勾配計算
						Evaluator move_probability;
						move_probability.init_internal<Ty>(state);
						move_probability.learn_one_pos<Ty>(state, grads[thread_id], 
						raw_table, updated[thread_id], label.move, stats[thread_id], 1.0);
					};
					//重要度の低い棋譜
#pragma omp for schedule(guided)
					for(int64_t i=total_step * batch2_size; i<(total_step + 1) * batch2_size; i++){
						int thread_id = omp_get_thread_num();
						//局面のセット
						State& state = states[thread_id];
						Label label = dataset2.get(state, i);
						//勾配計算
						Evaluator move_probability;
						move_probability.init_internal<Ty>(state);
						move_probability.learn_one_pos<Ty>(state, grads[thread_id], 
						raw_table, updated[thread_id], label.move, stats2[thread_id], 0.1);
					};
#pragma omp for
					for(int i=0;i<raw_table.size();i++){
						//各スレッドで計算したパラメータの統合
						for(int th =1; th < threads; th++){
							if(updated[th].test(i)){
								grads[0][i] += grads[th][i];
								grads[th][i].clear();
							}
						}
						//正則化
						grads[0][i].add_product(raw_table[i], penalty);
						//パラメータ更新
						raw_table[i].sub_product(grads[0][i], learning_rate);
						Ty::table[i] = factorization_machine::convert<Ty::InteractionDim>(raw_table[i]);
						grads[0][i] *= 0.9;
					}
#pragma omp for
					for(int i=0;i<threads;i++)updated[i].reset();
#pragma omp for
					for(int i=0;i<Ty::common_dim;i++){
						Ty::table[i][0] = 0;
					}
				}
				for(int i=1;i<threads;i++){
					stats[0] += stats[i];
					stats2[0] += stats2[i];
				}
				double loss = stats[0].average_loss();
				double accuracy = stats[0].accuracy();
				loss_exp_ave.update(loss);
				accuracy_exp_ave.update(accuracy);
				double loss2 = stats2[0].average_loss();
				double accuracy2 = stats2[0].accuracy();
				loss_exp_ave2.update(loss2);
				accuracy_exp_ave2.update(accuracy2);
				if(write_log){
					log_file << (total_step + 1) << "," << loss << "," << loss_exp_ave.score() << "," << accuracy << "," << accuracy_exp_ave.score();
					log_file << "," << loss2 << "," << loss_exp_ave2.score() << "," << accuracy2 << "," << accuracy_exp_ave2.score();
					log_file << "," << stopwatch.sec() << std::endl;
				}
			}
		}
		log_file.close();
	}
	void optimize(DataSet& dataset, DataSet& dataset2){
		optimize<Feature>("probability_log.csv", dataset, dataset2);
		optimize<SimpleFeature>("probability_log2.csv", dataset, dataset2);
		//パラメータ保存
		save();
	}
#endif
}