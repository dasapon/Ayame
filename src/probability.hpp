#pragma once
#include "state.hpp"
#include "learn.hpp"
#include "factorization_machine.hpp"

namespace shogi::move_probability{
	constexpr double score_scale = 1.0 / (factorization_machine::Scale * factorization_machine::Scale);
	template<typename Ty>
	using RawWeight = factorization_machine::RawWeight<Ty::InteractionDim>;
	template<typename Ty>
	using Weight = factorization_machine::Weight<Ty::InteractionDim>;
	enum{
		SeeFeatureDim = 21,
	};
	class Feature{
		enum {
			Evasion,
			PreviousMove = Evasion + 1 - EvalHandDim,
			PiecePair = PreviousMove + EvalIndexWithKingDim,
			KingDefence = PiecePair + EvalIndexWithKingDim * (EvalIndexWithKingDim - 1) / 2,
			KingAttack = KingDefence + 81 * 81 * 16,
			CommonDim = KingAttack + 81 * 81 * 16,
			See = CommonDim,
			Dee = See + SeeFeatureDim,
			CheckAndCapture = Dee + SeeFeatureDim,
			RelationLast = CheckAndCapture + 4 * Sentinel,
			RelationLastF = RelationLast + 100 * 289,
			Neighbor8To = RelationLastF + 100 * 289,
			Neighbor8From = Neighbor8To + 10 * 8 * 2 * 10,
			KPFriendTo = Neighbor8From + Sentinel * 8 * 2 * 10 - Pawn * 289,
			KPEnemyTo = KPFriendTo + Sentinel * 289 - Pawn * 289,
			KPFriendFrom = KPEnemyTo + Sentinel * 289 - Pawn * 289,
			KPEnemyFrom = KPFriendFrom + Sentinel * 289 - Pawn * 289,
			Escape = KPEnemyFrom + Sentinel * 289 - 2 * EvalHandDim,
			Attack = Escape + 2 * EvalIndexWithKingDim - EvalHandDim * 4,
			MoveClass = Attack + 10 * EvalIndexWithKingDim * 4,
			Dim = MoveClass + 8745,
		};
		static const sheena::Array<int, Sentinel> attack_index;
	public:
		enum{
			InteractionDim = 63,
		};
		static sheena::Array<Weight<Feature>, Dim> table;
		static constexpr int common_dim = CommonDim;
		static constexpr float learning_rate_base = 0.1;
		template<typename F>
		static void proce(State& state, Move move, const F& func);
		template<typename F>
		static void proce_common(const State& state, const F& func);
	};
	class SimpleFeature{
		enum {
			Evasion,
			PreviousMove = Evasion + 1 - EvalHandDim,
			PieceSquare = PreviousMove + EvalIndexWithKingDim,
			CommonDim = PieceSquare + EvalIndexWithKingDim,
			See = CommonDim,
			Capture = See + SeeFeatureDim,
			MoveClass = Capture + 2 * Sentinel,
			Dim = MoveClass + 8745,
		};
	public:
		enum{
			InteractionDim = 63,
		};
		static sheena::Array<Weight<SimpleFeature>, Dim> table;
		static constexpr int common_dim = CommonDim;
		static constexpr float learning_rate_base = 0.25;
		template<typename F>
		static void proce(State& state, Move move, const F& func);
		template<typename F>
		static void proce_common(const State& state, const F& func);
	};
	template<class Cl>
	using RawTable = sheena::Array<RawWeight<Cl>, Cl::table.size()>;
	template<class Cl>
	using UpdateFlags = std::bitset<Cl::table.size()>;
	class Evaluator{
		Weight<Feature> v_common;
		Weight<SimpleFeature> v_common_simple;
		template<typename Ty>
		int forward(State& state, Move move, Weight<Ty>& v)const;
#ifdef LEARN
		template<typename Ty>
		void backward(RawTable<Ty>& grad, const RawTable<Ty>& raw_table, UpdateFlags<Ty>& update_flag, 
		State& state, Move move, const RawWeight<Ty>& v, float g)const;
#endif
	public:
		template<typename Ty>
		void init_internal(const State& state);
		template<bool fast>
		void init(const State& state);
		template<bool fast>
		int score(State& state, Move move)const;
#ifdef LEARN
		template<typename Ty>
		void learn_one_pos(State& state, RawTable<Ty>& grad, const RawTable<Ty>& raw_table,
		UpdateFlags<Ty>& update_flag, Move bestmove, ClassificationStatistics& stats, double importance)const;
#endif
	};
	extern void load();
#ifdef LEARN
	extern void optimize(DataSet& dataset, DataSet& dataset2);
#endif
}