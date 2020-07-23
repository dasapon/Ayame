#include "search.hpp"

namespace shogi{
	bool Timer::terminate(float difficulty){
		std::lock_guard<std::mutex> lk(mtx);
		if(ponder || infinite)return false;
		if(stop_recieved || search_is_terminated){
			search_is_terminated = true;
			return true;
		}
		//消費時間を見て探索終了判定
		uint64_t consumed = ponderhit_detect.msec();
		if(consumed + byoyomi_margin >= time + inc + byoyomi)return true;
		else if(consumed >= time + inc)return false;
		//思考時間を見て探索終了判定
		uint64_t desired_thinking_time = std::max<double>(minimum_thinking_time, max_thinking_time * difficulty);
		uint64_t thinking_time = thinking_start.msec();
		//適切な思考時間が経過したか否かで判定
		search_is_terminated = desired_thinking_time < thinking_time;
		return search_is_terminated;
	}
	void Timer::stop(){
		std::lock_guard<std::mutex> lk(mtx);
		infinite = ponder = false;
		stop_recieved = true;
	}
	void Timer::ponderhit(){
		std::lock_guard<std::mutex> lk(mtx);
		ponder = false;
		ponderhit_detect.restart();
	}
	void Timer::timer_start(uint64_t time, uint64_t byoyomi, uint64_t inc, bool ponder, bool infinite){
		std::lock_guard<std::mutex> lk(mtx);
		this->time = time;
		this->byoyomi = byoyomi;
		this->ponder = ponder;
		this->inc = inc;
		this->ponder = ponder;
		this->infinite = infinite;
		stop_recieved = false;
		search_is_terminated = false;
		thinking_start.restart();
		ponderhit_detect.restart();
		//思考時間の設定
		max_thinking_time = time / 18 + byoyomi + inc;
	}
}