#include "bench_and_test.hpp"
#include "search.hpp"

namespace shogi{
	static const Sfen genmove_fes({
		"l6nl/5+P1gk/2np1S3/p1p4Pp/3P2Sp1/1PPb2P1P/P5GS1/R8/LN4bKL", "w", "RGgsn5p", "1"
	});
	static const Sfen max_move_pos({"R8/2K1S1SSk/4B4/9/9/9/9/9/1L1L1L3", "b", "RBGSNLP3g3n17p", "1"});
	static void genmove_bench(const Position& pos){
		sheena::Stopwatch stopwatch;
		MoveArray moves;
		int n;
		for(int i=0;i<500 * 10000; i++){
			if(pos.turn() == First)n = pos.generate_moves<First>(moves);
			else n = pos.generate_moves<Second>(moves);
		}
		uint64_t msec = stopwatch.msec();
		std::cout << msec << "[msec] " << n << " moves =";
		for(int i=0;i<n;i++)std::cout << " " << moves[i].string() <<" "<< pos.see(moves[i]);
		std::cout << std::endl; 
	}
	struct PerftCount{
		int64_t capture, check, promotion;
		PerftCount():capture(0),check(0),promotion(0){}
	};
	static int64_t perft(State& state, int d, PerftCount& counts){
		if(d == 0){
			if(state.check())counts.check++;
			return 1;
		}
		MoveArray moves;
		int n;
		int64_t ret = 0;
		if(state.turn() == First)n = state.generate_moves<First>(moves);
		else n = state.generate_moves<Second>(moves);
		for(int i=0;i<n;i++){
			if(d == 1){
				if(moves[i].is_capture())counts.capture++;
				if(moves[i].is_promotion())counts.promotion++;
			}
			state.make_move(moves[i]);
			ret += perft(state, d - 1, counts);
			state.unmake_move();
		}
		return ret;
	}
	void genmove_bench(){
		Position pos(startpos);
		genmove_bench(pos);
		pos.set_up(genmove_fes);
		genmove_bench(pos);
		pos.set_up(max_move_pos);
		genmove_bench(pos);
	}
	void perft(){
		sheena::Stopwatch stopwatch;
		State state;
		state.set_up(max_move_pos);
		PerftCount count;
		int64_t cnt = perft(state, 3, count);
		uint64_t msec = stopwatch.msec();
		std::cout << "perft " << cnt  <<" " << msec << "[msec] " << cnt / (msec + 1) << "k / sec"<< std::endl;
		std::cout << "capture " << count.capture << std::endl;
		std::cout << "promotion " << count.promotion << std::endl;
		std::cout << "check " << count.check << std::endl;
	}
	void search_bench(int threads){
		const std::string bench_record = std::string("2g2f 8c8d 2f2e 8d8e 2e2d 2c2d 6i7h 8e8f 8g8f 8b8f P*2c P*8g 2c2b+ 3a2b B*7e 8f8b 7e5c+ ")
		+ "8g8h+ 7i8h 4a4b 5c6c 7a6b 6c4e 6b5c 2h2d P*2c 2d2f 5a4a 2f6f 6a5a 6f6c+ 5a6b 6c6f 4a3b 6f2f 8b8d P*8g 1c1d 3i3h 3c3d 4e5f " 
		+ "2a3c 9g9f 4b4a 5i6h 8d5d 5f6e 5d8d 4g4f 5c6d 6e5f 6b5b 3g3f B*4d 3f3e 4d3e 2f3e 3d3e P*3d 3c2e 5f6f 8d8e 8g8f 8e8f 7g7f 8f8e "
		+ "8i7g 8e8b P*8d 3e3f B*7a 8b7b 7a2f+ 3f3g+ 2i3g 2e3g+ 2f3g N*7d 6f2b 3b2b 8d8c+ 7b6b S*3c 2b3a N*3e P*3b 3e2c+ 3b3c 3d3c+ 4a4b "
		+ "3g2f 6d5c 2f5c 5b5c S*5a 4b3c 2c3c R*3b 5a6b 3b3c 6b5c P*5f R*5b 5f5g+ 6h5g P*5f 5g5f B*3d 5f5e P*3b G*2d 3a2b 2d3c 2b3c P*3e "
		+ "B*3f 3e3d 3c2d B*5d 3f5d 5e5d N*7a 3d3c+ S*6c 5d4c 6c5b 4c5b 3b3c R*2a P*2b 2a2b+ P*2c P*2e 2d3e G*4e 3e2f B*4d 2f3f 2b3c G*3d 3c3d B*3e G*2g";
		std::vector<std::string> move_str = sheena::split_string(bench_record, ' ');
		std::vector<Move> moves;
		State state;
		std::cout << move_str.size() << std::endl;
		for(int i=0;i<move_str.size();i++){
			Move move = state.str2move(move_str[i]);
			state.make_move(move);
			moves.push_back(move);
		}
		SearchCore<64> searcher;
		searcher.set_threads(threads);
		PV pv;
		state.set_up(startpos);
		uint64_t nodes = 0;
		sheena::Stopwatch stopwatch;
		for(int i=0;i<moves.size();i++){
			searcher.search(state, pv, 15);
			nodes += searcher.nodes_searched();
			state.make_move(moves[i]);
		}
		int msec = stopwatch.msec();
		std::cout << msec << "[msec], " << nodes << "[nodes], " << nodes * 1000 / std::max(1, msec) << "[nps]"<< std::endl;
	}

	void test_mate1ply(){
		sheena::Array<Sfen, 1> test_positions({
			Sfen({"S1+Rlg2nl/2+Bg2k2/4ppsp1/p2p2p1p/9/PPP1PP2P/1r7/s2L5/2K4NL", "w", "B2GS2N2P3p", "1"}),
			Sfen({"lnsg3nl/1pk3g2/p1pp+N2sp/4+RB3/9/6p2/P1NPPP2P/1SG1K4/L4+pS+rL", "b", "5Pbgp", "1"}),
		});
		sheena::Array<std::string, 1> answer_move({
			"8g8i+",
			"resign",
		});
		State state;
		std::cout << "test mate1ply" << std::endl;
		for(int i=0;i<test_positions.size();i++){
			state.set_up(test_positions[0]);
			Move move = Move::null_move();
			state.Mate1Ply(&move);
			if(move.string() != answer_move[i]){
				std::cout << "invalid problem:" << i << " answer:" << answer_move[i] << " wrong:" << move.string()<< std::endl;
			}
		}
		std::cout << "complete" << std::endl;
	}
}