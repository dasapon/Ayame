#pragma once

#include "shogi.hpp"
#include "factorization_machine.hpp"

namespace shogi{
	class State;
	class Move;
	class Record;
	enum{
		EvalHandPawn = 0,
		EvalHandLance = 18,
		EvalHandKnight = 22,
		EvalHandSilver = 26,
		EvalHandBishop = 30,
		EvalHandRook = 32,
		EvalHandGold = 34,
		EvalHandDim = 38,
		EvalPawn = EvalHandDim - 9,
		EvalLance = EvalPawn + 81 - 9,
		EvalKnight = EvalLance + 81 - 18,
		EvalSilver = EvalKnight + 81,
		EvalBishop = EvalSilver + 81,
		EvalRook = EvalBishop + 81,
		EvalGold = EvalRook + 81,
		EvalProBishop = EvalGold + 81,
		EvalProRook = EvalProBishop + 81,
		
		EvalEnemyHand = EvalProRook + 81,
		EvalEnemyPawn = EvalEnemyHand + 38,
		EvalEnemyLance = EvalEnemyPawn + 81 - 9,
		EvalEnemyKnight = EvalEnemyLance + 81 - 9,
		EvalEnemySilver = EvalEnemyKnight + 81 - 18,
		EvalEnemyBishop = EvalEnemySilver + 81,
		EvalEnemyRook = EvalEnemyBishop + 81,
		EvalEnemyGold = EvalEnemyRook + 81,
		EvalEnemyProBishop = EvalEnemyGold + 81,
		EvalEnemyProRook = EvalEnemyProBishop + 81,

		EvalNone = EvalEnemyProRook + 81,
		EvalIndexDim = EvalNone + 1,
		
		EvalKing = EvalIndexDim,
		EvalEnemyKing = EvalKing + 81,
		EvalIndexWithKingDim = EvalEnemyKing + 81,

		//低次元特徴
		KPRel = 81 * EvalIndexDim,
		KPRelHand = KPRel + 18 * 17 * 17,
		KPRelDim = KPRelHand + 38 * 2,
		EvalSquare = KPRelDim,
		KPRawDim = EvalSquare + EvalIndexDim,

		KingSafetyTurn = 25 * 16,
		KingSafetyDim = KingSafetyTurn + 2,
	};
	enum{
		EvalScale = 2,
	};
	enum{
		KPBase = factorization_machine::LinearTerm,
		KPInteractionDim = 63,
		KPWeightDim,
	};
	class EvalIndex{
		union{
			int32_t idx[2];
			int64_t idx64;
		};
		EvalIndex(int a, int b){
			idx[0] = a;
			idx[1] = b;
		}
		EvalIndex(int64_t a):idx64(a){}
		EvalIndex operator+(const EvalIndex& rhs){
			return idx64 + rhs.idx64;
		}
#ifdef LEARN
	public:
		static void psq(int idx, int* p_p, int* p_sq);
#endif
		static sheena::Array<EvalIndex, BoardSize> square_index;
		static sheena::Array2d<EvalIndex, Sentinel, 2> board_index;
		static sheena::Array2d<EvalIndex, King, 2> hand_index;
		static sheena::Array<EvalIndex, 18> hand_count_index;
	public:
		EvalIndex():idx64(0){}
		void operator=(const EvalIndex& rhs){
			idx64 = rhs.idx64;
		}
		static void init();
		static EvalIndex on_board(Player owner, PieceType pt, Square sq);
		static EvalIndex in_hand(Player owner, PieceType pt, int n);
		static EvalIndex none();
		int operator[](size_t i)const{
			assert(i < 2);
			return idx[i];
		}
		bool operator!=(const EvalIndex& rhs)const{
			return idx64 != rhs.idx64;
		}
	};
	struct EvalListDiff{
		std::pair<int, EvalIndex> moved, captured;
	};
	using KPWeight = factorization_machine::Weight<KPInteractionDim>;
	using KPRawWeight = factorization_machine::RawWeight<KPInteractionDim>;
	using EvalList = sheena::Array<EvalIndex, NumPiece>;
	extern sheena::Array2d<KPWeight, 81, EvalIndexDim> kp_table;
	extern sheena::Array<KPWeight, KingSafetyDim> ks_table;
#ifdef LEARN
	extern sheena::Array3d<int, 81, EvalIndexDim, 8> kp_raw_index;
	template<typename F>
	void foreach_kp_raw(int ksq, int idx, const F& func){
		const auto& a = kp_raw_index[ksq][idx];
		for(int i=1;i<=a[0];i++){
			func(a[i]);
		}
	}
#endif
	struct EvalWeight{
		sheena::Array<KPRawWeight, KPRawDim> kp;
		std::bitset<KPRawDim> kp_updated;
		sheena::Array<KPRawWeight, KingSafetyDim> ks;

		void update(const State& state, EvalWeight& grad, float g)const;
	};
	struct EvalComponent{
		sheena::Array<KPWeight, PlayerDim> kp;
		int material;
		EvalComponent(){}
		EvalComponent(const State& list);
		void operator=(const EvalComponent& rhs);
	};
	extern void init_evaluate(EvalComponent& eval, const EvalList& lst);
	extern void init_as_material();
	extern void load_evaluate();
	extern void save_evaluate();
}