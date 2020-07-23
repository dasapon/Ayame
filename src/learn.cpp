#include "learn.hpp"
#include "search.hpp"
namespace shogi{
	void Record::set_up(State& state, int ply)const{
		state.set_up(sp);
		for(int i=0;i<ply;i++){
			Move move = Move::unpack_15bit(operator[](i).first, state);
			state.make_move(move);
		}
	}
	static Square csa_square(char x, char y){
		if(x == '0' && y == '0'){
			return SquareHand;
		}
		return make_square(y - '1' + xy_start, xy_end - (x - '1' + xy_start));
	}
	static void load_csa1(const std::string& filename, std::vector<Record>& records){
		std::vector<std::string> records_str;
		sheena::read_file(filename, records_str);
		sheena::Array<std::string, Sentinel> csa_piece_string({
			"",
			"FU", "KY", "KE", "GI", "KA", "HI", "KI", "OU",
			"TO", "NY", "NK", "NG", "UM", "RY",
		});
		SimplePosition startpos_sp(startpos);
		for(int record_id=0; record_id<records_str.size() / 2; record_id++){
			Record record;
			record.sp = startpos_sp;
			Position pos(record.sp);
			bool skip = false;
			std::vector<std::string> tags = sheena::split_string(records_str[record_id * 2], ' ');
			int r = std::stoi(tags[4]);
			switch(r){
			case 0://引き分け
				record.result = 0;
				break;
			case 1://先手の勝ち
				record.result = 1;
				break;
			case 2://後手の勝ち
				record.result = -1;
				break;
			default:
				std::cout << "invalid result " << records_str[record_id * 2];
				skip = true;
			}
			if(skip)continue;
			const std::string& csa1_moves = records_str[record_id * 2 + 1];
			for(int i=0;i+5<csa1_moves.size() && !skip;i+=6){
				Square from = csa_square(csa1_moves[i], csa1_moves[i + 1]);
				Square to = csa_square(csa1_moves[i + 2], csa1_moves[i + 3]);
				std::string pt_str = csa1_moves.substr(i + 4, 2);
				PieceType pt_promoted = Empty;
				for(pt_promoted = Empty; pt_promoted < Sentinel; pt_promoted++){
					if(pt_str == csa_piece_string[pt_promoted])break;
				}
				MoveArray moves;
				Move played = Move::null_move();
				int n_moves = pos.GenerateMoves(moves);
				for(int j=0;j<n_moves;j++){
					if(moves[j].from() == from
					&& moves[j].to() == to
					&& moves[j].pt_promoted() == pt_promoted){
						played = moves[j];
						break;
					}
				}
				if(played == Move::null_move()){
					skip = true;
					break;
				}
				pos.MakeMove(played);
				//深い探索の評価値は0としておく
				record.push_back(played, 0);
			}
			if(!skip){
				records.push_back(record);
			}
		}
	}
	void store_records(std::vector<Record>& records, const std::string& filename){
		std::ofstream out(filename);
		State state;
		for(const Record& record : records){
			state.set_up(record.sp);
			Sfen sfen = state.sfen();
			out << record.result;
			for(int i=0;i<Sfen::size();i++){
				out << " " << sfen[i];
			}
			for(auto m : record){
				Move move = Move::unpack_15bit(m.first, state);
				state.make_move(move);
				out << " " << move.string() << " " << m.second;
			}
			out << std::endl;
		}
		out.close();
	}
	DataSet load_records(const std::vector<std::string>& filenames){
		DataSet ret;
		for(const std::string& filename : filenames){
			if(filename.size() >= 5 && filename.substr(filename.size() - 5) == ".csa1"){
				std::cout << "load csa1 file " << filename << std::endl;
				std::vector<Record> records;
				//csa1形式のファイルは全てrecordsに格納してからDataSetに変換
				load_csa1(filename, records);
				for(const Record& record : records){
					ret.add(record);
				}
			}
			else{
				std::cout << "load text file " << filename << std::endl;
				//txt形式の棋譜は数が多いため、一局ずつDataSetに変換
				//stringとして各行を読み込む
				std::vector<std::string> records_str;
				sheena::read_file(filename, records_str);
				//1局ずつRecordに変換してから、DataSetに格納(todo : 直接DataSetに入れる)
				Record record;
				for(int record_id = 0; record_id < records_str.size();record_id++){
					record.clear();
					std::vector<std::string> r = sheena::split_string(records_str[record_id], ' ');
					record.result = std::stoi(r[0]);
					if(std::abs(record.result) > 1){
						std::cout << "alert " << record.result << std::endl;
						continue;
					}
					Sfen sfen;
					for(int i=0;i < Sfen::size(); i++){
						sfen[i] = r[i + 1];
					}
					record.sp = sfen;
					Position pos(record.sp);
					for(int i=1 + Sfen::size(); i<r.size(); i+=2){
						Move move = pos.str2move(r[i]);
						int v = std::stoi(r[i + 1]);
						pos.MakeMove(move);
						record.push_back(move, v);
					}
					ret.add(record);
					if((record_id + 1) % 500000 == 0){
						std::cout << record_id + 1 << std::endl;
					}
				}
			}
		}
		return ret;
	}
	int DataSet::get_record_id(uint64_t idx, int s, int e)const{
		if(e == s + 1)return s;
		int mid = (s + e) / 2;
		if(idx < pos_count[mid])return get_record_id(idx, s, mid);
		return get_record_id(idx, mid, e);
	}
	Label DataSet::get(State& state, uint64_t idx)const{
		uint64_t x = rand[idx];
		int record_id = get_record_id(x, 0, sp.size());
		state.set_up(sp[record_id]);
		for(uint64_t i=pos_count[record_id];i<x;i++){
			Move move = Move::unpack_15bit(moves[i], state);
			state.make_move(move);
		}
		Label label;
		label.result = result[record_id];
		label.move = Move::unpack_15bit(moves[x], state);
		label.eval = values[x];
		return label;
	}
#ifdef LEARN
	class EvalLearner{
		using Stats = ClassificationStatistics;
		sheena::Array<SingleThreadSearcher, learning_threads> searcher;
		sheena::Array<State, learning_threads> states;
		sheena::Array<std::mt19937, learning_threads> rand;
		sheena::ArrayAlloc<EvalWeight> local_grad;
		EvalWeight *raw_weight_ptr, *global_grad_ptr;
		static constexpr size_t batch_size = 10000;
		float learning_rate;
		static constexpr float penalty = 0.1;
		static constexpr float momentum = 0.9;
		static constexpr float sl_brother_loss_scale = 2.0;
		static constexpr int move_select_count = 16;
		static constexpr float rl_scale = 3.0;
		float deep_eval_scale;
		void init_weights();
		void update_weights();
		void update_gradients(State& state, const PV& pv, float g, int thread_id);
		sheena::Array<Stats, 2> supervised_learning(const DataSet& dataset, int start, int end);
		sheena::Array<Stats, 2> reinforcement_learning(const DataSet& dataset, uint64_t start, uint64_t end);
	public:
		EvalLearner();
		~EvalLearner();
		void optimize(DataSet& selfplay_records, DataSet& expert_records);
	};
	EvalLearner::EvalLearner():local_grad(learning_threads), raw_weight_ptr(new EvalWeight), global_grad_ptr(new EvalWeight), deep_eval_scale(512){
	}
	EvalLearner::~EvalLearner(){
		delete raw_weight_ptr;
		delete global_grad_ptr;
	}
	
	//重みの初期化
	void EvalLearner::init_weights(){
		EvalWeight& raw_weight = *raw_weight_ptr;
		//勾配のゼロクリア
		for(int i=0;i<local_grad.size();i++){
			memset(&local_grad[i], 0, sizeof(EvalWeight));
		}
		memset(global_grad_ptr, 0, sizeof(EvalWeight));
		//raw_weightを乱数を用いて初期化
		std::mt19937 mt(0);
		std::normal_distribution<double> dist(0, 0.01);
		factorization_machine::initialize_table<KPRawDim, KPInteractionDim>(raw_weight.kp, dist, mt);
		for(int ksq = 0; ksq < 81; ksq++){
			for(int i=0;i<EvalIndexDim;i++){
				KPRawWeight kp;
				kp.clear();
				foreach_kp_raw(ksq, i, [&](int raw_idx){
					kp += raw_weight.kp[raw_idx];
				});
				kp_table[ksq][i] = factorization_machine::convert<KPInteractionDim>(kp);
			}
		}
		factorization_machine::initialize_table<KingSafetyDim, KPInteractionDim>(raw_weight.ks, dist, mt);
		for(int i=0;i<KingSafetyDim;i++){
			ks_table[i] = factorization_machine::convert<KPInteractionDim>(raw_weight.ks[i]);
		}
		for(int i=0;i<learning_threads;i++)rand[i].seed(mt());
	}
	void EvalLearner::update_weights(){
		auto update = [=](KPRawWeight& g, KPRawWeight& w){
			g += w * penalty;
			w -= g * learning_rate;
			g *= momentum;
		};
#pragma omp parallel
		{
			//各スレッドで計算した勾配を一つにまとめてから更新を行う
#pragma omp for nowait
			for(int i=0;i<KPRawDim;i++){
				for(int th = 0; th < learning_threads; th++){
					if(local_grad[th].kp_updated.test(i)){
						global_grad_ptr->kp[i] += local_grad[th].kp[i];
						local_grad[th].kp[i].clear();
					}
				}
				update(global_grad_ptr->kp[i], raw_weight_ptr->kp[i]);
			}
#pragma omp for
			for(int i=0;i<KingSafetyDim;i++){
				for(int th = 0; th < learning_threads; th++){
					global_grad_ptr->ks[i] += local_grad[th].ks[i];
					local_grad[th].ks[i].clear();
				}
				update(global_grad_ptr->ks[i], raw_weight_ptr->ks[i]);
				//ks_tableの更新
				ks_table[i] = factorization_machine::convert<KPInteractionDim>(raw_weight_ptr->ks[i]);
			}
#pragma omp for
			for(int th=0;th < learning_threads; th++){
				local_grad[th].kp_updated.reset();
			}
#pragma omp for
			for(int ksq = 0; ksq < 81; ksq++){
				for(int i=0;i<EvalIndexDim;i++){
					KPRawWeight kp;
					kp.clear();
					foreach_kp_raw(ksq, i, [&](int raw_idx){
						kp += raw_weight_ptr->kp[raw_idx];
					});
					kp_table[ksq][i] = factorization_machine::convert<KPInteractionDim>(kp);
				}
			}
		}//omp parallel
	}
	void EvalLearner::update_gradients(State& state, const PV& pv, float g, int thread_id){
		//PVをたどる
		int ply;
		for(ply = 0;pv[ply];ply++){
			state.make_move(pv[ply]);
		}
		if(state.cycle_flag() == NoRep){
			raw_weight_ptr->update(state, local_grad[thread_id], g);
		}
		for(int i=0;i<ply;i++){
			state.unmake_move();
		}
	}
	sheena::Array<ClassificationStatistics, 2> EvalLearner::reinforcement_learning(const DataSet& dataset, uint64_t start, uint64_t end){
		sheena::Array<Stats, learning_threads> stats, stats_deep;
#pragma omp parallel for schedule(guided)
		for(int64_t i=start; i<end;i++){
			int thread_id = omp_get_thread_num();
			State& state = states[thread_id];
			Label label = dataset.get(state, i);
			//静止探索をして評価値を得る
			PV pv;
			int v = searcher[thread_id].qsearch(state, pv) * player2sign(state.turn());
			double wr_deep = eval2wr(label.eval, deep_eval_scale);
			double wr = eval2wr(v);
			double loss = cross_entropy_loss((1.0 + label.result) / 2, v);
			double loss_deep = cross_entropy_loss(wr_deep, v);
			stats[thread_id].update(loss, v * label.result > 0);
			stats_deep[thread_id].update(loss_deep, v * (wr_deep - 0.5) > 0);
			if(std::abs(v) >= SupValue)continue;
			//勾配を更新
			double g = 0.5 * (wr - (label.result + 1.0) / 2) + 0.5 * (wr - wr_deep);
			update_gradients(state, pv, g * rl_scale, thread_id);
		}
		//各スレッドで計算した統計値を統合
		sheena::Array<Stats, 2> ret;
		for(int i=0;i<learning_threads;i++){
			ret[0] += stats[i];
			ret[1] += stats_deep[i];
		}
		return ret;
	}
	sheena::Array<ClassificationStatistics, 2> EvalLearner::supervised_learning(const DataSet& dataset, int start, int end){
		sheena::Array<Stats, learning_threads> stats, stats_wr;
#pragma omp parallel for schedule(guided)
		for(int i=start; i<end;i++){
			int thread_id = omp_get_thread_num();
			State& state = states[thread_id];
			Label label = dataset.get(state, i);
			int margin = 10 + std::min(118, state.ply()) * 2;
			//合法手生成
			MoveArray moves;
			int n_moves = state.GenerateMoves(moves);
			PV pv_good, pv;
			sheena::Array<PV, move_select_count> pv_bad;
			const int sign = player2sign(state.turn());
			//expert playerが実際に指した手で局面を進めて静止探索
			state.make_move(label.move);
			int v_good = -searcher[thread_id].qsearch(state, pv_good);
			state.unmake_move();
			//v_goodの値が詰みや駒得千日手のスコアであれば、この局面を学習に使う意味はない
			if(std::abs(v_good) >= SupValue)continue;
			//この局面の先手勝率
			double wr = eval2wr(v_good * sign);
			double loss_wr = cross_entropy_loss((1.0 + label.result) / 2, v_good * sign);
			stats_wr[thread_id].update(loss_wr, v_good * sign * label.result > 0);
			//指されなかった手で局面を進めて静止探索
			float loss = 0;
			bool accurate = true;
			bool skip = false;
			int n = 0;
			if(n_moves > 1){
				int selected = 0;
				for(int j=0;j<n_moves;j++){
					if(moves[j] == label.move){
						moves[j] = moves[--n_moves];
						break;
					}
				}
				for(int j=0;j<n_moves;j++){
					if(rand[thread_id]() % (n_moves - j) >= move_select_count - selected)continue;
					state.make_move(moves[j]);
					int v = -searcher[thread_id].qsearch(state, pv);
					state.unmake_move();
					if(v >= v_good)accurate = false;
					if(std::abs(v) >= SupValue){
						skip = true;
						break;
					}
					//PVを登録
					if(v >= v_good - margin && n < move_select_count){
						loss += v - v_good + margin;
						pv_bad[n][0] = moves[j];
						int k;
						for(k=0;pv[k];k++){
							pv_bad[n][k + 1] = pv[k];
						}
						pv_bad[n][k + 1] = Move::null_move();
						n++;
					}
					selected++;
				}
			}
			if(skip)continue;
			//兄弟手との比較から算出されるloss
			stats[thread_id].update(loss, accurate);
			//勾配を更新(実戦の手)
			double g = (wr - (label.result + 1.0) / 2);//勝率項
			if(n != 0)g -= sign * sl_brother_loss_scale;//兄弟手との比較
			state.make_move(label.move);
			update_gradients(state, pv_good, g, thread_id);
			state.unmake_move();
			//勾配更新(指されなかった手)
			for(int i=0;i<n;i++){
				update_gradients(state, pv_bad[i], sign * sl_brother_loss_scale / n, thread_id);
			}
		}
		//各スレッドで計算した統計値を統合
		sheena::Array<Stats, 2> ret;
		for(int i=0;i<learning_threads;i++){
			ret[0] += stats[i];
			ret[1] += stats_wr[i];
		}
		return ret;
	}
	void EvalLearner::optimize(DataSet& rl_data, DataSet& sl_data){
		init_weights();
		omp_set_num_threads(learning_threads);
		std::ofstream log_file("eval_log.csv");
		if(!log_file.is_open()){
			return;
		}
		sheena::Stopwatch stopwatch;
		std::mt19937 mt(0);
		double sl_size = sl_data.size(), rl_size = rl_data.size();
		int sl_batch = sl_size / (sl_size + rl_size) * batch_size;
		int rl_batch = rl_size / (sl_size + rl_size) * batch_size;
		std::cout << "batch size\n";
		std::cout << "reinforcement_learning : " << rl_batch << std::endl;
		std::cout << "supervised_learning : " << sl_batch << std::endl;
		if(rl_batch + sl_batch == 0){
			std::cout << "no records" << std::endl;
			return;
		}
		sheena::Array<ExpAverage, 8> average_stats;
		size_t n_step = INT_MAX;
		if(rl_batch){
			n_step = std::min(rl_data.size() / rl_batch, n_step);
		}
		if(sl_batch){
			n_step = std::min(sl_data.size() / sl_batch, n_step);
		}
		std::cout << "step/epoch = " << n_step << std::endl; 
		int total_step = 0;
		learning_rate = 1.0 / batch_size;
		for(int epoch = 0; epoch < 5; epoch++){
			learning_rate /= 10;
			rl_data.shuffle(mt);
			sl_data.shuffle(mt);
			for(int step = 0; step < n_step; step++){
				//強化学習による勾配と損失の計算
				sheena::Array<Stats, 2> rl_stats = reinforcement_learning(rl_data, step * rl_batch, (step + 1) * rl_batch);
				//教師あり学習による勾配と損失の計算
				sheena::Array<Stats, 2> sl_stats = supervised_learning(sl_data, step * sl_batch, (step + 1) * sl_batch);
				sheena::Array<Stats, 4> stats;
				for(int i=0;i<2;i++){
					stats[i] = sl_stats[i];
					stats[i + 2] = rl_stats[i];
				}
				//重み更新
				update_weights();
				total_step++;
				//指数平均の計算
				for(int i=0;i<stats.size();i++){
					average_stats[i * 2].update(stats[i].average_loss());
					average_stats[i * 2 + 1].update(stats[i].accuracy());
				}
				//ログの出力
				if(total_step % 100 == 0){
					uint64_t sec = stopwatch.sec();
					log_file << total_step << ",";
					//損失
					for(int i=0;i<stats.size(); i++){
						log_file << stats[i].average_loss() << "," << average_stats[i * 2].score() << ",";
					}
					//accuracy
					for(int i=0;i<stats.size(); i++){
						log_file << stats[i].accuracy() << "," << average_stats[i * 2 + 1].score() << ",";
					}
					states[0].set_up(startpos);
					log_file << states[0].evaluate() << "," << sec << std::endl;
				}
			}
		}
		log_file.close();
		save_evaluate();
	}
	void optimize_eval(const std::vector<std::string>& sl_files, const std::vector<std::string>& rl_files){
		DataSet sl_records = load_records(sl_files);
		DataSet rl_records = load_records(rl_files);
		std::cout << "supervised_learning : " << sl_records.size() << " games" << std::endl;
		std::cout << "reinforcement_learning : " << rl_records.size() << " games" << std::endl;
		EvalLearner learner;
		learner.optimize(rl_records, sl_records);
	}
#endif
}