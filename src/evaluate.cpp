#include "evaluate.hpp"
#include "state.hpp"

namespace shogi{
	sheena::Array2d<KPWeight, 81, EvalIndexDim> kp_table;
	sheena::Array<KPWeight, KingSafetyDim> ks_table;
	
	sheena::Array<EvalIndex, BoardSize> EvalIndex::square_index;
	sheena::Array2d<EvalIndex, Sentinel, 2> EvalIndex::board_index;
	sheena::Array2d<EvalIndex, King, 2> EvalIndex::hand_index;
	sheena::Array<EvalIndex, 18> EvalIndex::hand_count_index;
#ifdef LEARN
	sheena::Array3d<int, 81, EvalIndexDim, 8> kp_raw_index;
#endif
	EvalIndex EvalIndex::on_board(Player owner, PieceType pt, Square sq){
		return board_index[pt][owner] + square_index[sq];
	}
	EvalIndex EvalIndex::in_hand(Player owner, PieceType pt, int n){
		return hand_index[pt][owner] + hand_count_index[n];
	}
	EvalIndex EvalIndex::none(){
		return EvalIndex(EvalNone, EvalNone);
	}
#ifdef LEARN
	void EvalIndex::psq(int idx, int* p_p, int* p_sq){
		const sheena::Array<int, 20> eid_start({
			EvalPawn + 9, EvalLance + 9, EvalKnight + 18, EvalSilver, EvalBishop, EvalRook, EvalGold,
			EvalProBishop, EvalProRook, EvalEnemyPawn, EvalEnemyLance, EvalEnemyKnight, EvalEnemySilver, EvalEnemyBishop, EvalEnemyRook,
			EvalEnemyGold, EvalEnemyProBishop, EvalEnemyProRook, EvalKing, EvalEnemyKing,
		});
		if(idx < EvalHandDim 
		|| (idx >= EvalEnemyHand && idx < EvalEnemyHand + EvalHandDim)
		|| idx == EvalNone){
			*p_p = -1;
			return;
		}
		int p = 0;
		for(;p<20;p++){
			if(eid_start[p] > idx){
				break;
			}
		}
		p--;
		int sq = idx - eid_start[p];
		if(p == 2)sq += 18;
		else if(p <= 1)sq += 9;
		*p_p = p;
		*p_sq = sq;
	}
#endif
	void EvalIndex::init(){
		for(int x = xy_start; x < xy_end; x++){
			for(int y=xy_start;y<xy_end; y++){
				int sq81 = (x - xy_start) + (y - xy_start) * 9;
				square_index[make_square(y, x)] = EvalIndex(sq81, 80 - sq81);
			}
		}
		const sheena::Array<int, Sentinel> board_friend({
			0, EvalPawn, EvalLance, EvalKnight, EvalSilver, EvalBishop, EvalRook, EvalGold, EvalKing,
			EvalGold, EvalGold, EvalGold, EvalGold, EvalProBishop, EvalProRook,
		});
		const sheena::Array<int, Sentinel> board_enemy({
			0, EvalEnemyPawn, EvalEnemyLance, EvalEnemyKnight, EvalEnemySilver, EvalEnemyBishop, EvalEnemyRook, EvalEnemyGold, EvalEnemyKing,
			EvalEnemyGold, EvalEnemyGold, EvalEnemyGold, EvalEnemyGold, EvalEnemyProBishop, EvalEnemyProRook,
		});
		const sheena::Array<int, King> hand_friend({
			0, EvalHandPawn, EvalHandLance, EvalHandKnight, EvalHandSilver, EvalHandBishop, EvalHandRook, EvalHandGold,
		});
		for(PieceType pt = Pawn; pt < Sentinel; pt++){
			board_index[pt][First] = EvalIndex(board_friend[pt], board_enemy[pt]);
			board_index[pt][Second] = EvalIndex(board_enemy[pt], board_friend[pt]);
		}
		for(PieceType pt = Pawn; pt < King; pt++){
			hand_index[pt][First] = EvalIndex(hand_friend[pt], hand_friend[pt] + EvalEnemyHand);
			hand_index[pt][Second] = EvalIndex(hand_friend[pt] + EvalEnemyHand, hand_friend[pt]);
		}
		for(int i=0;i<18;i++){
			hand_count_index[i] = EvalIndex(i, i);
		}
#ifdef LEARN
		//kp_raw_indexの初期化
		//元々の特徴量
		for(int ksq=0; ksq<81; ksq++){
			for(int i=0;i<EvalIndexDim;i++){
				kp_raw_index[ksq][i][0] = 1;
				kp_raw_index[ksq][i][1] = ksq * EvalIndexDim + i;
			}
		}
		//相対位置
		for(int ksq = 0; ksq < 81; ksq++){
			for(int i=0;i<EvalIndexDim; i++){
				int p, sq;
				psq(i, &p, &sq);
				if(p < 0)continue;
				int dx = ksq % 9 - sq % 9 + 8;
				int dy = ksq / 9 - sq / 9 + 8;
				kp_raw_index[ksq][i][++kp_raw_index[ksq][i][0]] = KPRel + (p * 17 + dx) * 17 + dy;
			}
			for(int i=0;i<EvalHandDim;i++){
				int alt = i + EvalEnemyHand;
				kp_raw_index[ksq][i][++kp_raw_index[ksq][i][0]] = KPRelHand + i;
				kp_raw_index[ksq][alt][++kp_raw_index[ksq][alt][0]] = KPRelHand + 38 + i;
			}
		}
		//Pのみの位置
		for(int ksq=0; ksq < 81; ksq++){
			for(int i=0;i<EvalIndexDim;i++){
				kp_raw_index[ksq][i][++kp_raw_index[ksq][i][0]] = EvalSquare + i;
			}
		}
#endif
	}
	static KPWeight eval_kp(Player pl, const EvalList& list){
		int ksq = list[king_id(pl)][pl] - EvalKing;
		KPWeight ret(0);
		for(int i=0;i<FKingID;i++){
			ret += kp_table[ksq][list[i][pl]];
		}
		return ret;
	}
	static int ks_index(int attack, int defence, int dx, int dy){
		attack = std::min(3, attack);
		defence = std::min(3, defence);
		int ret = defence * 4 + attack;
		ret += ((dx + 2) * 5 + (dy + 2)) * 16;
		return ret;
	}
	static KPWeight eval_king_safety(Player pl, const State& state){
		KPWeight ret;
		ret.clear();
		Square ksq = state.king_sq(pl);
		int ksq_x = squareX(ksq), ksq_y = squareY(ksq);
		int west = std::min(2, ksq_x - xy_start);
		int east = std::min(2, xy_end - ksq_x - 1);
		int north = std::min(2, ksq_y - xy_start);
		int south = std::min(2, xy_end - ksq_y - 1);
		for(int dx = -west; dx <= east; dx++){
			for(int dy = -north; dy <= south; dy++){
				Square sq = ksq + dx * BoardHeight + dy;
				int attack = state.control_count(opponent(pl), sq);
				int defence = state.control_count(pl, sq);
				int idx = ks_index(attack, defence, dx * player2sign(pl), dy * player2sign(pl));
				ret += ks_table[idx];
			}
		}
		ret += ks_table[KingSafetyTurn + (pl ^ state.turn())];
		return ret;
	}
	EvalComponent::EvalComponent(const State& state){
		const EvalList& list = state.get_eval_list();
		kp[First] = eval_kp(First, list);
		kp[Second] = eval_kp(Second, list);
		//駒割
		material = 0;
		//盤上の駒
		for(int x = xy_start;x<xy_end;x++){
			for(int y=xy_start;y<xy_end;y++){
				Square sq = make_square(y, x);
				Piece piece = state.piece_on(sq);
				if(piece.empty())continue;
				material += material_value[piece.type()] * player2sign(piece.owner());
			}
		}
		//持ち駒
		for(PieceType pt = Pawn;pt < King; pt++){
			material += state.get_hand(First).count(pt) * material_value[pt];
			material -= state.get_hand(Second).count(pt) * material_value[pt];
		}
	}
	void EvalComponent::operator=(const EvalComponent& rhs){
		kp = rhs.kp;
		material = rhs.material;
	}
	void init_as_material(){
		for(int ksq = 0; ksq < 81; ksq++){
			for(int i=0;i<EvalIndexDim;i++){
				kp_table[ksq][i].clear();
			}
		}
		for(int i=0;i<KingSafetyDim;i++)ks_table[i].clear();
	}
	void State::eval_diff(EvalComponent& eval, Move lastmove, const EvalListDiff& elist_diff)const{
		if(!lastmove)return;
		Player acted = opponent(turn());
		Player noact = turn();
		//駒割
		eval.material += player2sign(acted) * lastmove.estimate();
		const EvalList& list = get_eval_list();
		if(lastmove.piece_type() == King){
			//玉が動いた
			//動いた側の玉に関するKPを再計算
			eval.kp[acted] = eval_kp(acted, list);
			if(lastmove.is_capture()){
				//動いていない方の玉についてはcaptureの分更新
				int ksq_na = list[king_id(noact)][noact] - EvalKing;
				eval.kp[noact] -= kp_table[ksq_na][elist_diff.captured.second[noact]];
				eval.kp[noact] += kp_table[ksq_na][list[elist_diff.captured.first][noact]];
			}
		}
		else{
			//玉が動いていない場合
			int ksq_na = list[king_id(noact)][noact] - EvalKing;
			int ksq = list[king_id(acted)][acted] - EvalKing;
			eval.kp[noact] -= kp_table[ksq_na][elist_diff.moved.second[noact]];
			eval.kp[noact] += kp_table[ksq_na][list[elist_diff.moved.first][noact]];
			eval.kp[acted] -= kp_table[ksq][elist_diff.moved.second[acted]];
			eval.kp[acted] += kp_table[ksq][list[elist_diff.moved.first][acted]];
			if(lastmove.is_capture()){
				//KPのCapture分
				eval.kp[noact] -= kp_table[ksq_na][elist_diff.captured.second[noact]];
				eval.kp[noact] += kp_table[ksq_na][list[elist_diff.captured.first][noact]];
				eval.kp[acted] -= kp_table[ksq][elist_diff.captured.second[acted]];
				eval.kp[acted] += kp_table[ksq][list[elist_diff.captured.first][acted]];
			}
		}
	}
	int State::evaluate()const{
		if(check())return 0;
		const EvalComponent& eval = history[ply()].eval;
		int base_value = eval.material * EvalScale;
		for(Player pl = First; pl < PlayerDim; pl++){
			KPWeight v = eval.kp[pl] + eval_king_safety(pl, *this);
			int value = v[KPBase];
			v[KPBase] = 0;
			value += v.inner_product(v) / factorization_machine::Scale;
			base_value += value * player2sign(pl);
		}
		return base_value * player2sign(turn()) / EvalScale;
	}
#ifdef LEARN
	void EvalWeight::update(const State& state, EvalWeight& grad, float g)const{
		const EvalList& list = state.get_eval_list();
		const EvalComponent& eval = state.eval_component();
		for(Player pl = First; pl < PlayerDim; pl++){
			float g_own = g * player2sign(pl);
			KPRawWeight w = factorization_machine::convert_interactions<KPInteractionDim>(eval.kp[pl] + eval_king_safety(pl, state));
			//ksの勾配
			Square ksq = state.king_sq(pl);
			int ksq_x = squareX(ksq), ksq_y = squareY(ksq);
			int west = std::min(2, ksq_x - xy_start);
			int east = std::min(2, xy_end - ksq_x - 1);
			int north = std::min(2, ksq_y - xy_start);
			int south = std::min(2, xy_end - ksq_y - 1);
			for(int dx = -west; dx <= east; dx++){
				for(int dy = -north; dy <= south; dy++){
					Square sq = ksq + dx * BoardHeight + dy;
					int attack = state.control_count(opponent(pl), sq);
					int defence = state.control_count(pl, sq);
					int idx = ks_index(attack, defence, dx * player2sign(pl), dy * player2sign(pl));
					factorization_machine::update<KPInteractionDim>(grad.ks[idx], ks[idx], w, g_own);
				}
			}
			int turn_idx = KingSafetyTurn + (pl ^ state.turn());
			factorization_machine::update<KPInteractionDim>(grad.ks[turn_idx], ks[turn_idx], w, g_own);
			
			//kpの勾配
			int ksq81 = list[king_id(pl)][pl] - EvalKing;
			for(int i=0;i<FKingID;i++){
				int idx = list[i][pl];
				KPRawWeight kp_sum;
				kp_sum.clear();
				foreach_kp_raw(ksq81, idx, [&](int raw_idx){
					kp_sum += kp[raw_idx];
				});
				foreach_kp_raw(ksq81, idx, [&](int raw_idx){
					factorization_machine::update<KPInteractionDim>(grad.kp[raw_idx], kp_sum, w, g_own);
					grad.kp_updated.set(raw_idx);
				});
			}
		}
	}
#endif
	void load_evaluate(){
		FILE* fp = fopen("eval.bin", "rb");
		bool ok = fp != nullptr;
		for(int ksq=0;ksq<81 && ok;ksq++){
			for(int i=0;i<EvalIndexDim;i++){
				ok = ok && fread(&kp_table[ksq][i], sizeof(int16_t), KPWeightDim, fp) == KPWeightDim;
			}
		}
		for(int i=0;i<KingSafetyDim;i++){
			ok = ok && fread(&ks_table[i], sizeof(int16_t), KPWeightDim, fp) == KPWeightDim;
		}
		if(!ok){
			std::cout << "eval.bin can't be loaded" << std::endl;
			init_as_material();
		}
		if(fp != nullptr)fclose(fp);
	}
	void save_evaluate(){
		FILE* fp = fopen("eval.bin", "wb");
		for(int ksq=0;ksq<81;ksq++){
			for(int i=0;i<EvalIndexDim;i++){
				fwrite(&kp_table[ksq][i], sizeof(int16_t), KPWeightDim, fp);
			}
		}
		for(int i=0;i<KingSafetyDim;i++){
			fwrite(&ks_table[i], sizeof(int16_t), KPWeightDim, fp);
		}
		fclose(fp);
	}
}