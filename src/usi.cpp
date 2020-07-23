#include "state.hpp"
#include "search.hpp"

namespace shogi{
	void usi_loop(){
		Searcher searcher;
		State state;
		std::vector<std::pair<std::string, std::function<void(int)>>> options;

		std::map<std::string, std::function<void(const std::vector<std::string>&)>> commands;
		commands["usi"]=[&](const std::vector<std::string>&){
			std::cout << "id name Ayame20200724" << std::endl;
			std::cout << "id author Watanabe Keisuke" << std::endl;
			std::cout << "option name Level type spin default " << searcher.level << " min " << searcher.LevelZero <<  " max " << searcher.LevelMax << std::endl;
			std::cout << "option name Threads type spin default 1 min 1 max 16" << std::endl;
			std::cout << "usiok" << std::endl;
			std::cout.flush();
		};
		commands["isready"]=[&](const std::vector<std::string>&){
			searcher.ready();
			std::cout << "readyok" << std::endl;
		};
		commands["setoption"]=[&](const std::vector<std::string>& args){
			if(args[2] == "USI_Hash"){
				searcher.set_hash_size(std::stoi(args[4]));
			}
			else if(args[2] == "Level"){
				searcher.level = std::stoi(args[4]);
			}
			else if(args[2] == "Threads"){
				searcher.set_threads(std::stoi(args[4]));
			}
		};
		commands["usinewgame"]=[](const std::vector<std::string>&){
		};
		commands["position"]=[&](const std::vector<std::string>& args){
			int idx = 0;
			if(args[1] == "startpos"){
				state.set_up(startpos);
				idx = 2;
			}
			else{
				state.set_up(Sfen({args[2], args[3], args[4], args[5]}));
				idx = 6;
			}
			if(args.size() > idx && args[idx] == "moves"){
				for(idx += 1; idx < args.size(); idx++){
					state.make_move(state.str2move(args[idx]));
				}
			}
		};
		commands["go"]=[&](const std::vector<std::string>& args){
			uint64_t time = 0, inc = 0, byoyomi = 0;
			bool ponder = false, infinite = false;
			for(int i=1;i<args.size(); i++){
				if(args[i] == "byoyomi"){
					byoyomi = std::stoi(args[i + 1]);
				}
				else if(args[i] == "ponder"){
					ponder = true;
				}
				else if(args[i] == "infinite"){
					infinite = true;
				}
				else if(args[i] == "btime"){
					if(state.turn() == First)time = std::stoi(args[i + 1]);
				}
				else if(args[i] == "wtime"){
					if(state.turn() == Second)time = std::stoi(args[i + 1]);
				}
				else if(args[i] == "binc"){
					if(state.turn() == First)inc = std::stoi(args[i + 1]);
				}
				else if(args[i] == "winc"){
					if(state.turn() == Second)inc = std::stoi(args[i + 1]);
				}
			}
			searcher.timer_start(time, byoyomi, inc, ponder, infinite);
			searcher.go(state);
		};
		commands["stop"]=[&](const std::vector<std::string>&){
			searcher.stop();
		};
		commands["ponderhit"]=[&](const std::vector<std::string>&){
			searcher.ponderhit();
		};
		commands["quit"]=[&](const std::vector<std::string>&){
			searcher.stop();
			std::exit(0);
		};
		commands["gameover"]=[&](const std::vector<std::string>&){
			searcher.stop();
		};
		//デバッグ用コマンド
		commands["eval"]=[&](const std::vector<std::string>&){
			std::cout << "eval:" << state.evaluate() << std::endl;
			std::cout << "eval_list";
			const EvalList& lst = state.get_eval_list();
			for(int i=0;i<NumPiece; i++){
				
				std::cout << " (" << lst[i][0] << ", " << lst[i][1] << "),";
			}
			std::cout << std::endl;
		};
		commands["genmove"]=[&](const std::vector<std::string>&){
			MoveArray moves;
			int n;
			if(state.turn() == First)n = state.generate_moves<First>(moves);
			else n = state.generate_moves<Second>(moves);
			std::cout << n << " moves =";
			for(int i=0;i<n;i++)std::cout << " " << moves[i].string();
			std::cout << std::endl;
		};
		commands["sfen"]=[&](const std::vector<std::string>&){
			Sfen sfen = state.sfen();
			std::cout << sfen[0] << " " << sfen[1] << " " << sfen[2] << " " << sfen[3] << std::endl;
		};

		std::string line;
		while(std::getline(std::cin, line)){
			std::vector<std::string> cmd = sheena::split_string(line, ' ');
			if(cmd.size() > 0){
				if(commands.find(cmd[0]) != commands.end()){
					commands[cmd[0]](cmd);
				}
			}
		}
	}
}