#pragma once
#include "state.hpp"
#include "probability.hpp"
namespace shogi{

	class KillerMove : public sheena::Array<Move, 2>{
	public:
		void clear(){
			(*this)[0] = Move::null_move();
			(*this)[1] = Move::null_move();
		}
		KillerMove(){}
	};
	//指し手オーダリングをするクラス
	class MoveOrdering{
		enum Status{
			Hash,
			Capture,
			Killer,
			All,
		};
		State& state;
		move_probability::Evaluator move_probability;
		MoveArray moves;
		sheena::Array<float, MaxLegalMove> scores;
		const KillerMove killer_move;
		Move hash_move;
		int move_count, n_moves;
		Status status;
		const bool do_fp;
		void generate_next();
		void generate_rps();
		void generate_evasion();
		void insertion_sort();
		void insertion_sort_v2();
	public:
		//通常探索用
		MoveOrdering(State& state, Move hash_move, const KillerMove& killer_move, bool rps, bool do_fp);
		Move next_move(float* pscore);
	};
	class MoveOrderingQSearch{
		MoveArray moves;
		sheena::Array<int, MaxLegalMove> scores;
		int move_count, n_moves;
	public:
		//静止探索
		MoveOrderingQSearch(State& state, Move hash_move);
		Move next_move();
	};
}