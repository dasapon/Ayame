#include "search.hpp"
#include "learn.hpp"

namespace shogi{
	static void selfplay_one_record(SingleThreadSearcher& searcher, Record& record, int depth, int value_limit){
		State state(record.sp);
		PV pv;
		record.result = 0;
		for(int ply=0;ply<300;ply++){
			int v = searcher.search(state, pv, depth);
			if(v <= -value_limit || pv[0] == Move::null_move()){
				record.result = -player2sign(state.turn());
				break;
			}
			else if(v >= value_limit){
				record.result = player2sign(state.turn());
				break;
			}
			record.push_back(pv[0] , v * player2sign(state.turn()));
			state.make_move(pv[0]);
			CycleFlag flg = state.cycle_flag();
			if(flg == DrawRep)break;
		}
	}
	void selfplay(int depth, int nthreads, int ngames){
		std::vector<SingleThreadSearcher> searchers(nthreads);
		std::vector<Record> records;
		records.resize(ngames);
		std::random_device rd;
		std::mt19937 mt(rd());
		//初期局面の設定
		{
			std::vector<SimplePosition> start_positions;
			std::vector<std::string> lines;
			bool ok = sheena::read_file("pos.txt", lines);
			if(!ok){
				return;
			}
			start_positions.resize(lines.size());
			for(int i=0; i < lines.size(); i++){
				std::vector<std::string> s = sheena::split_string(lines[i], ' ');
				Sfen sfen;
				for(int j=0;j<Sfen::size();j++){
					sfen[j] = s[j];
				}
				start_positions[i] = sfen;
			}
			if(ngames > start_positions.size()){
				std::cout << "too many games" << std::endl;
			}
			std::shuffle(start_positions.begin(), start_positions.end(), mt);
			for(int i=0;i<ngames;i++)records[i].sp = start_positions[i];
		}
		omp_set_num_threads(nthreads);
		std::cout << "selfplay start" << std::endl;
		sheena::Stopwatch stopwatch;
		std::mutex mtx;
#pragma omp parallel for schedule(dynamic, 100)
		for(int i=0;i<records.size();i++){
			int thread_id = omp_get_thread_num();
			if(thread_id == 0 && i % 100 == 0){
				std::cout << stopwatch.sec() << "[sec] cur_game = " << i << std::endl;
			}
			int value_limit = 512 * 5;
			{
				std::lock_guard<std::mutex> lk(mtx);
				if(mt() % 32 == 0)value_limit = SupValue;
			}
			Record& record = records[i];
			selfplay_one_record(searchers[thread_id], record, depth, value_limit);
		}
		store_records(records, "selfplay.txt");
	}
	void selfplay_startpos(){
		Position pos;
		std::unordered_map<std::string, int> positions;
		//平手初期局面
		positions[pos.regularized_sfen()] = 0;
		MoveArray moves;
		int ply_limit = 7;
		std::mt19937 mt(0);
		for(int ply=0; ply < ply_limit; ply++){
			std::cout << "ply = " << ply << std::endl;
			std::unordered_map<std::string, int> next_positions;
			int cnt = 0;
			for(const auto& p : positions){
				if(p.second != ply)continue;
				const SimplePosition sp(p.first);
				pos.set_up(sp);
				int n_moves = pos.GenerateMoves(moves);
				for(int i=0;i<n_moves;i++){
					//飛車角歩の不成は無視
					if(!moves[i].is_drop()){
						PieceType pt = moves[i].pt_promoted();
						if(can_promote(First, pt, moves[i].from(), moves[i].to())){
							if(pt == Pawn || pt == Bishop || pt == Rook)continue;
						}
					}
					pos.MakeMove(moves[i]);
					std::string str = pos.regularized_sfen();
					auto itr = positions.find(str);
					//既に展開済みの局面でないかチェック
					if(itr == positions.end()){
						next_positions[str] = ply + 1;
					}
					pos.UnmakeMove(moves[i]);
				}
				if(++cnt % 10000 == 0){
					std::cout << cnt << " " << next_positions.size() << std::endl;
				}
			}
			std::cout << "leaf positions = " << next_positions.size() << std::endl;
			for(const auto& p : next_positions){
				//positionsに追加
				positions[p.first] = ply + 1;
			}
		}
		std::ofstream out("pos.txt");
		for(const auto& p : positions){
			if(p.second != ply_limit)continue;
			//保存
			std::vector<std::string> v = sheena::split_string(p.first, ' ');
			out << v[0] << " b " << v[1] << " 1" << std::endl;
		}
		out.close();
	}
}