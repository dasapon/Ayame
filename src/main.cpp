#include "position.hpp"
#include "bench_and_test.hpp"
#include "usi.hpp"
#include "learn.hpp"
#include "probability.hpp"
#include "search.hpp"

using namespace shogi;

int main(int argc, char* args_cstyle_str[]){
	shogi::init_tables();
	shogi::init_hash_seed();
	shogi::load_evaluate();
	shogi::move_probability::load();
	std::vector<std::string> args(argc);
	for(int i=0;i<argc;i++)args[i] = std::string(args_cstyle_str[i]);
	if(argc > 1){
		if(argc == 3 && args[1] == "bench"){
			search_bench(std::stoi(args[2]));
			//genmove_bench();
		}
		else if(args[1] == "test"){
			test_mate1ply();
		}
		if(args[1] == "perft"){
			perft();
		}
		if(argc == 2 && args[1] == "selfplay_startpos"){
			selfplay_startpos();
		}
		if(argc >= 4 && args[1] == "selfplay"){
			selfplay(std::stoi(args[2]), std::stoi(args[3]), std::stoi(args[4]));
		}
#ifdef LEARN
		if(argc < 3){
			std::cout << "few args" << std::endl;
			return 0;
		}
		if(args[1] == "eval" || args[1] == "probability"){
			std::vector<std::string> sl_files;
			std::vector<std::string> rl_files;
			for(int i=2;i + 1<args.size();i+=2){
				if(args[i] == "sl"){
					sl_files.push_back(args[i + 1]);
				}
				else if(args[i] == "rl"){
					rl_files.push_back(args[i + 1]);
				}
				else{
					throw std::invalid_argument(args[i]);
				}
			}
			if(args[1] == "eval"){
				optimize_eval(sl_files, rl_files);
			}
			else{
				DataSet dataset = load_records(sl_files);
				DataSet dataset2 = load_records(rl_files);
				move_probability::optimize(dataset, dataset2);
			}
		}
#endif
	}
	else usi_loop();
	return 0;
}