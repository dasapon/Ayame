#pragma once

#include "shogi.hpp"

namespace shogi{
	class Timer{
		sheena::Stopwatch thinking_start, ponderhit_detect;
		static constexpr uint64_t minimum_thinking_time = 500;
		uint64_t byoyomi_margin;
		uint64_t time, byoyomi, inc;
		bool ponder, infinite;
		bool stop_recieved;
		bool search_is_terminated;
		std::mutex mtx;
		uint64_t max_thinking_time;
	public:
		Timer(){
			byoyomi_margin = 0;
		}
		bool terminate(float difficulty);
		void set_terminated(){
			std::lock_guard<std::mutex> lk(mtx);
			search_is_terminated = true;
		}
		void stop();
		void ponderhit();
		void set_byoyomi_margin(uint64_t b){
			byoyomi_margin = b;
		}
		void timer_start(uint64_t time, uint64_t byoyomi, uint64_t inc, bool ponder, bool infinite);
	};
}