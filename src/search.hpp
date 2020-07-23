#pragma once

#include "state.hpp"
#include "hash_table.hpp"
#include "timer.hpp"
#include "move_ordering.hpp"

namespace shogi{
	struct MTMove{
		Move move;
		float probability;
		int value;
		MTMove(Move m, float p, int v):move(m), probability(p), value(v){}
	};
	constexpr int MaxPly = 128;
	template<size_t Threads>
	class SearchCore;
	//探索部の核となる部分
	template<size_t Threads=1>
	class SearchCore{
	protected:
		sheena::Array2d<KillerMove, Threads, MaxPly> killer_move;
		sheena::ArrayAlloc<sheena::Array<PV, MaxPly>> pv_table;
		HashTable hash_table;
		sheena::Array<uint64_t, Threads> nodes;
		sheena::Array<std::atomic<bool>, Threads> abort;
		sheena::Array<std::thread, Threads> threads;
		sheena::Array<State, Threads> states;
		std::atomic<bool> tle;
		//USIで指定されたスレッド数(Threads以下である必要がある)
		int nthreads;
		int qsearch(State& state, int alpha, int beta, int depth, int ply, int thread_id);
		int search(State& state, int alpha, int beta, int depth, int ply, int thread_id);
		//
		int evaluate(const State& state)const{
			return state.evaluate();
		}
	public:
		SearchCore();
		int qsearch(State& state, PV& pv);
		//固定深さまで反復深化を行う関数.1スレッド限定
		int search(State& state, PV& pv, int depth);
		void set_hash_size(int mb){hash_table.resize(mb);}
		uint64_t nodes_searched()const{
			uint64_t ret = 0;
			for(int i=0;i<Threads;i++)ret += nodes[i];
			return ret;
		}
		void clear_hash(){
			hash_table.clear();
		}
		void set_threads(size_t th){
			nthreads = std::min(th, Threads);
		}
		void new_search();
		void time_up(){
			tle.store(true);
		}
	};
	using SingleThreadSearcher = SearchCore<1>;
	//時間制御を内蔵する探索部
	class Searcher : SearchCore<64>, Timer{
		std::thread search_thread;
		std::mt19937 mt;
		std::uniform_int_distribution<int> random;
		std::atomic<float> difficulty;
		int search(State& state, int alpha, int beta, int depth, std::vector<MTMove>& moves, int thread_id);
	public:
		enum{
			LevelZero = 0,
			LevelMax = 20,
		};
		int level;
		void go(State& state);
		void ready(){}
		Searcher():random(LevelZero, LevelMax), level(LevelMax){
			//シードの初期化
			mt.seed(std::chrono::system_clock::now().time_since_epoch().count());
		}
		~Searcher(){
			if(search_thread.joinable())search_thread.detach();
		}
		using Timer::timer_start;
		using Timer::set_byoyomi_margin;
		using Timer::ponderhit;
		using Timer::stop;
		using SearchCore::set_hash_size;
		using SearchCore::clear_hash;
		using SearchCore::set_threads;
	private:
		int iterative_deepning(State& state, int depth_limit, PV& pv);
	};
}