#include "position.hpp"

namespace shogi{
	//[移動先候補となる8近傍のパターン][玉の移動の8近傍パターン][玉周辺4マスの空き]から、駒種の候補を得る
	static sheena::Array3d<int, 256, 256, 16> checkmate_piece_type;
	//[駒種][玉の移動可能なマス][玉周辺4マスの空きパターン]から、移動先の候補(方向は先手番が攻め方となったもの.後手が攻め方なら方向を反転する)を得る
	static sheena::Array3d<int, Sentinel, 256, 16> checkmate_dir;
	static int empty_to_empty4(int empty_mask){
		int empty4_mask = (empty_mask & 0b01011010);
		empty4_mask = ((empty4_mask >> 5) | (empty4_mask >> 1)) & 0xf;
		return empty4_mask;
	}
	template<Player pl>
	bool Position::mate1ply(Move* pmove){
		constexpr Player king_side = opponent(pl);
		const Square ksq = king_sq(king_side);
		BitBoard evasion_canditate;
		evasion_canditate.clear();
		int attack_mask = 0, attack2_mask = 0, defence_mask = 0, evasion_canditate_mask = 0, empty_mask = 0;
		for(Dir dir = NW; dir <= SE; dir++){
			int mask = 1 << (pl == First? dir : (SE - dir));
			Square sq = ksq + dir_diff[dir];
			if(board[sq].type() == Sentinel)continue;
			if(board[sq].empty()){
				empty_mask |= mask;
				evasion_canditate ^= square_bb[sq];
				evasion_canditate_mask |= mask;
			}
			else if(board[sq].owner() == pl){
				evasion_canditate ^= square_bb[sq];
				evasion_canditate_mask |= mask;
			}
			//玉方の効きの有無
			if(control_count(king_side, sq) > 1){
				defence_mask |= mask;
			}
			//攻め方の効き
			int attack_count = control_count(pl, sq);
			if(attack_count){
				attack_mask |= mask;
				if(attack_count > 1){
					attack2_mask |= mask;
				}
			}
		}
		//周辺4マスの空きマスパターン.0R0DU0L0 -> DURL
		int empty4_mask = empty_to_empty4(empty_mask);
		const int evasion_mask = evasion_canditate_mask & ~attack_mask;
		//桂馬の移動や桂打ちによる詰み
		if(evasion_mask == 0){
			BitBoard target = knight_attack[opponent(pl)][ksq];
			for(Dir dir = NNW; dir <= NNE; dir++){
				Square to = pl == First? (ksq - dir_diff[dir]) : (ksq + dir_diff[dir]);
				if(BitBoard::out_of_range(to))continue;
				if(has_control(opponent(pl), to))continue;
				if(!board[to].is_enemy_or_empty<pl>())continue;
				//飛び効きを塞いで詰まなくなってしまうかもしれない
				if(sliding_control[to] & own_slider[pl]){
					BitBoard invalidation = control_invalidation(pl, to);
					if(evasion_canditate & invalidation)continue;
				}
				//桂打ち
				if(board[to].empty() && hand[pl].count(Knight)){
					*pmove = Move::move_drop(Knight, to);
					return true;
				}
				//移動
				for(Dir dir2 = NNW; dir2 <= NNE; dir2++){
					Square from = pl == First? (to - dir_diff[dir2]) : (to + dir_diff[dir2]);
					if(BitBoard::out_of_range(from))continue;
					Piece piece = piece_on(from);
					if(piece.type() != Knight || piece.owner() != pl){
						continue;
					}
					if(!is_pinned(pl, from)){
						*pmove = Move(Knight, board[to].type(), from, to, false);
						return true;
					}
				}
			}
		}
		//駒打ち王手
		const int drop_dir_mask = attack_mask & empty_mask & ~defence_mask;
		int drop_piece_mask = checkmate_piece_type[drop_dir_mask][evasion_mask][empty4_mask];
		drop_piece_mask &= hand[pl].binary_mask() & ~(1 << Pawn);
		if(drop_piece_mask){
			//効きを見なくとも確実に詰みだと分かる手がないか?
			const int easy_drop_piece_mask = checkmate_piece_type[drop_dir_mask][evasion_canditate_mask][empty4_mask];
			if(easy_drop_piece_mask){
				PieceType pt = static_cast<PieceType>(sheena::bsf32(easy_drop_piece_mask));
				int dir_mask = checkmate_dir[pt][evasion_canditate_mask][empty4_mask] & drop_dir_mask;
				Dir dir = static_cast<Dir>(sheena::bsf32(dir_mask));
				Square to = pl == First ? (ksq + dir_diff[dir]) : (ksq - dir_diff[dir]);
				*pmove = Move::move_drop(pt, to);
				return true;
			}
			//金打ちで詰ます手
			if(drop_piece_mask & (1 << Gold)){
				int canditate = checkmate_dir[Gold][evasion_mask][empty4_mask] & drop_dir_mask;
				while(canditate){
					Dir dir = static_cast<Dir>(sheena::bsf32(canditate));
					canditate &= canditate - 1;
					Square to = pl == First ? (ksq + dir_diff[dir]) : (ksq - dir_diff[dir]);
					//本当に詰むか(効きを塞いでしまって不詰にならないか)検証
					if(sliding_control[to] & own_slider[pl]){
						BitBoard evasion = evasion_canditate;
						evasion.remove(gold_attack[pl][to]);
						evasion &= control_invalidation(pl, to);
						if(evasion)continue;
					}
					*pmove = Move::move_drop(Gold, to);
					return true;
				}
			}
			//銀打ち
			if(drop_piece_mask & (1 << Silver)){
				int canditate = checkmate_dir[Silver][evasion_mask][empty4_mask] & drop_dir_mask;
				while(canditate){
					Dir dir = static_cast<Dir>(sheena::bsf32(canditate));
					canditate &= canditate - 1;
					Square to = pl == First ? (ksq + dir_diff[dir]) : (ksq - dir_diff[dir]);
					//本当に詰むか(効きを塞いでしまって不詰にならないか)検証
					if(sliding_control[to] & own_slider[pl]){
						BitBoard evasion = evasion_canditate;
						evasion.remove(silver_attack[pl][to]);
						evasion &= control_invalidation(pl, to);
						if(evasion)continue;
					}
					*pmove = Move::move_drop(Silver, to);
					return true;
				}
			}
			BitBoard all_bb_noking = all_bb;
			all_bb_noking ^= square_bb[ksq];
			if(drop_piece_mask & (1 << Rook)){
				int canditate = checkmate_dir[Rook][evasion_mask][empty4_mask] & drop_dir_mask;
				while(canditate){
					Dir dir = static_cast<Dir>(sheena::bsf32(canditate));
					canditate &= canditate - 1;
					Square to = pl == First ? (ksq + dir_diff[dir]) : (ksq - dir_diff[dir]);
					//本当に詰むか(効きを塞いでしまって不詰にならないか)検証
					if(sliding_control[to] & own_slider[pl]){
						BitBoard evasion = evasion_canditate;
						evasion.remove(rook_attack(to, all_bb_noking));
						evasion &= control_invalidation(pl, to);
						if(evasion)continue;
					}
					*pmove = Move::move_drop(Rook, to);
					return true;
				}
			}
			else if(drop_piece_mask & (1 << Lance)){
				//香車を打って詰ますには玉頭に打つしかない
				Square to = ksq + dir_diff[pl == First? South : North];
				bool ok = true;
				if(sliding_control[to] & own_slider[pl]){
					BitBoard evasion = evasion_canditate;
					evasion.remove(lance_attack(pl, to, all_bb_noking));
					evasion &= control_invalidation(pl, to);
					ok = false;
				}
				if(ok){
					*pmove = Move::move_drop(Lance, to);
					return true;
				}
			}
			if(drop_piece_mask & (1 << Bishop)){
				int canditate = checkmate_dir[Bishop][evasion_mask][empty4_mask] & drop_dir_mask;
				while(canditate){
					Dir dir = static_cast<Dir>(sheena::bsf32(canditate));
					canditate &= canditate - 1;
					Square to = pl == First ? (ksq + dir_diff[dir]) : (ksq - dir_diff[dir]);
					if(sliding_control[to] & own_slider[pl]){
						BitBoard evasion = evasion_canditate;
						evasion.remove(bishop_attack(to, all_bb_noking));
						evasion &= control_invalidation(pl, to);
						if(evasion)continue;
					}
					*pmove = Move::move_drop(Bishop, to);
					return true;
				}
			}
		}
		//周辺8マスへの駒移動による詰み
		int canditate = attack2_mask & ~defence_mask;
		while(canditate){
			Dir dir = static_cast<Dir>(sheena::bsf32(canditate));
			canditate &= canditate - 1;
			Square to = pl == First ? (ksq + dir_diff[dir]) : (ksq - dir_diff[dir]);
			if(!board[to].is_enemy_or_empty<pl>())continue;
			//短い効きによる攻撃
			int shortcontrol = short_control[pl][to];
			while(shortcontrol){
				Square from = to - dir_diff[sheena::bsf32(shortcontrol)];
				shortcontrol &= shortcontrol - 1;
				PieceType pt = board[from].type();
				//玉で王手することはない
				if(pt == King)continue;
				//自殺手や影効きで取られる手でない
				if(discovered(pl, from, to))continue;
				if(sliding_control[from] & own_slider[opponent(pl)] & square_relation[from][to])continue;
				bool promotion = can_promote(pl, pt, from, to);
				PieceType pt_promoted = promotion? promote(pt) : pt;
				//駒の効きの変化を考えなければ詰む場合にのみ、細かく検証する
				if(checkmate_dir[pt_promoted][evasion_mask][empty4_mask] & (1 << dir)){
					//効きの変化を考えなくとも詰むことが分かる
					if(checkmate_dir[pt_promoted][evasion_canditate_mask][empty4_mask] & (1 << dir)){
						*pmove = Move(pt, board[to].type(), from, to, promotion);
						return true;
					}
					if(mate1move<pl>(pmove, from, to, evasion_canditate)){
						return true;
					}
				}
			}
			//長い効きによる攻撃
			int slidingcontrol = sliding_control[to] & own_slider[pl];
			while(slidingcontrol){
				int id = sliding2id[sheena::bsf32(slidingcontrol)];
				slidingcontrol &= slidingcontrol - 1;
				Square from = pieces[id].square();
				PieceType pt = pieces[id].type();
				//自殺手や影効きで取られる手でない
				if(discovered(pl, from, to))continue;
				if(sliding_control[from] & own_slider[opponent(pl)] & square_relation[from][to])continue;
				bool promotion = can_promote(pl, pt, from, to);
				PieceType pt_promoted = promotion? promote(pt) : pt;
				//駒の効きの変化を考えなければ詰む場合にのみ、細かく検証する
				if(checkmate_dir[pt_promoted][evasion_mask][empty4_mask] & (1 << dir)){
					//効きの変化を考えなくとも詰むことが分かる
					if(checkmate_dir[pt_promoted][evasion_canditate_mask][empty4_mask] & (1 << dir)){
						*pmove = Move(pt, board[to].type(), from, to, promotion);
						return true;
					}
					if(mate1move<pl>(pmove, from, to, evasion_canditate)){
						return true;
					}
				}
			}
		}
		return false;
	}
	bool Position::Mate1Ply(Move* pmove){
		assert(!check());
		if(turn() == First)return mate1ply<First>(pmove);
		else return mate1ply<Second>(pmove);
	}
	template<Player pl>
	bool Position::mate1move(Move* pmove, Square from, Square to, const BitBoard& evasion_canditate){
		const Square ksq = king_sq(opponent(pl));
		PieceType pt = board[from].type();
		//効きを求める
		bool promotion = can_promote(pl, pt, from, to);
		//効きを求めるとき、盤から玉自体を消しておく
		BitBoard all_bb_noking = all_bb;
		all_bb_noking.remove(ksq);
		BitBoard attack = attack_bb(pl, promotion? promote(pt) : pt, to, all_bb_noking);
		int id = board[from].id();
		//元の効きを消す
		xor_control<pl>(pt, from, id);
		//効きを計算
		BitBoard attack_new = attack_neighbor8(pl, ksq);
		attack_new.remove(control_invalidation(pl, to));
		attack_new |= attack;
		BitBoard evasion(evasion_canditate);
		evasion.remove(attack_new);
		bool ret = false;
		if(!evasion){
			*pmove = Move(pt, board[to].type(), from, to, promotion);
			ret = true;
		}
		//元の効きを戻す
		xor_control<pl>(pt, from, id);
		return ret;
	}
	
	void init_mate1_table(){
		Square ksq = make_square(xy_start + 5, xy_start + 5);
		for(int king_evasion_pattern = 0; king_evasion_pattern < 256; king_evasion_pattern++){
			for(int empty_pattern= 0; empty_pattern < 256; empty_pattern++){
				BitBoard occupied;
				occupied.clear();
				int empty4_mask = empty_to_empty4(empty_pattern);
				//玉が逃げられるマスを表すbitboard
				BitBoard evasion;
				evasion.clear();
				for(Dir dir = NW; dir <= SE; dir++){
					int dir_mask = 1 << dir;
					if(king_evasion_pattern & dir_mask){
						evasion.set(ksq + dir_diff[dir]);
					}
					if((empty_pattern & dir_mask) == 0){
						occupied.set(ksq + dir_diff[dir]);
					}
				}
				//checkmate_dirの初期化
				for(PieceType pt = Pawn; pt < Sentinel; pt++){
					checkmate_dir[pt][king_evasion_pattern][empty4_mask] = 0;
					//玉で王手することはない
					if(pt == King)continue;
					for(Dir dir = NW; dir <= SE; dir++){
						Square to = ksq + dir_diff[dir];
						BitBoard attack = attack_bb(First, pt, to, occupied);
						if(!attack[ksq])continue;
						BitBoard ev = evasion;
						ev.remove(attack);
						if(!ev){
							checkmate_dir[pt][king_evasion_pattern][empty4_mask] |= 1 << dir;
						}
					}
				}
				//checkmate_piece_typeの初期化
				for(int to_pattern = 0; to_pattern < 256; to_pattern++){
					int mask = 0;
					for(PieceType pt = Pawn; pt < Sentinel; pt++){
						for(Dir dir = NW; dir <= SE; dir++){
							if((to_pattern & (1 << dir)) == 0)continue;
							if(checkmate_dir[pt][king_evasion_pattern][empty4_mask] & (1 << dir)){
								mask |= 1 << pt;
								break;
							}
						}
					}
					checkmate_piece_type[to_pattern][king_evasion_pattern][empty4_mask] = mask;
				}
			}
		}
	}
}