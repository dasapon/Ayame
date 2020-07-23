#include "position.hpp"

namespace shogi{
	static int add_move(Player pl, MoveArray& moves, int n, PieceType pt, PieceType cap, Square from, Square to){
		if(can_promote(pl, pt, from, to)){
			moves[n++] = Move(pt, cap, from, to, true);
		}
		if(!must_promote(pl, pt, to)){
			moves[n++] = Move(pt, cap, from, to, false);
		}
		return n;
	}
	int Position::generate_capture(MoveArray& moves, int n)const{
		const Player pl = turn();
		const Player enemy = opponent(pl);
		PieceSet targets = pieces_on_board[enemy] & ~PieceSetKing;
		while(targets){
			Piece target = pieces[sheena::bsf64(targets)];
			targets &= targets - 1;
			Square to = target.square();
			PieceType cap = target.type();
			//短い効きによる攻撃
			int shortcontrol = short_control[pl][to];
			while(shortcontrol){
				Square from = to - dir_diff[sheena::bsf32(shortcontrol)];
				shortcontrol &= shortcontrol - 1;
				PieceType pt = board[from].type();
				if(pt == King){
					if(!has_control(enemy, to)){
						moves[n++] = Move(King, cap, from, to, false);
					}
				}
				else{
					if(!discovered(pl, from, to)){
						n = add_move(pl, moves, n, pt, cap, from, to);
					}
				}
			}
			//長い効きによる攻撃
			int slidingcontrol = sliding_control[to] & own_slider[pl];
			while(slidingcontrol){
				int id = sliding2id[sheena::bsf32(slidingcontrol)];
				slidingcontrol &= slidingcontrol - 1;
				Square from = pieces[id].square();
				if(!discovered(pl, from, to)){
					n = add_move(pl, moves, n, pieces[id].type(), cap, from, to);
				}
			}
		}
		return n;
	}
	template<PieceType pt>
	int Position::generate_nocapture_sub(Player pl, MoveArray& moves, int n, PieceSet piece_set)const{
		while(piece_set){
			int id = sheena::bsf64(piece_set);
			piece_set &= piece_set - 1;
			Square from = pieces[id].square();
			BitBoard attack = attack_bb<pt>(pl, from, all_bb);
			attack.remove(all_bb);
			attack.foreach([&](Square to){
				if(!discovered(pl, from, to))n = add_move(pl, moves, n, pt, Empty, from, to);
			});
		}
		return n;
	}
	int Position::generate_nocapture(MoveArray& moves, int n)const{
		const Player pl = turn();
		//駒種ごとに着手を生成する
		//玉の場合、移動先に相手の効きがない場合のみok
		{
			Square from = king_sq(pl);
			BitBoard attack = king_attack[from];
			attack.remove(all_bb);
			attack.foreach([&](Square to){
				if(!has_control(opponent(pl), to))moves[n++] = Move(King, Empty, from, to, false);
			});
		}
		//歩の場合、駒の効きが1箇所にしかないため、BitBoardによる効き生成をしない
		{
			PieceSet pawns = ~promoted_pieces & pieces_on_board[pl] & PieceSetPawns;
			while(pawns){
				int id = sheena::bsf64(pawns);
				pawns &= pawns - 1;
				Square from = pieces[id].square();
				Square to = from + dir_diff[pl == First? North : South];
				if(board[to].empty() && !discovered(pl, from, to)){
					n = add_move(pl, moves, n, Pawn, Empty, from, to);
				}
			}
		}
		//香車
		n = generate_nocapture_sub<Lance>(pl, moves, n, ~promoted_pieces & pieces_on_board[pl] & PieceSetLances);
		//桂馬
		n = generate_nocapture_sub<Knight>(pl, moves, n, ~promoted_pieces & pieces_on_board[pl] & PieceSetKnights);
		//銀
		n = generate_nocapture_sub<Silver>(pl, moves, n, ~promoted_pieces & pieces_on_board[pl] & PieceSetSilvers);
		//角
		n = generate_nocapture_sub<Bishop>(pl, moves, n, ~promoted_pieces & pieces_on_board[pl] & PieceSetBishops);
		//飛車
		n = generate_nocapture_sub<Rook>(pl, moves, n, ~promoted_pieces & pieces_on_board[pl] & PieceSetRooks);
		//金、成り金
		{
			PieceSet golds = (pieces_on_board[pl] & PieceSetGolds) | (pieces_on_board[pl] & promoted_pieces & PieceSetSmalls);
			while(golds){
				Piece piece = pieces[sheena::bsf64(golds)];
				golds &= golds - 1;
				Square from = piece.square();
				BitBoard attack = gold_attack[pl][from];
				attack.remove(all_bb);
				attack.foreach([&](Square to){
					if(!discovered(pl, from, to))moves[n++] = Move(piece.type(), Empty, from, to, false);
				});
			}
		}
		//馬
		n = generate_nocapture_sub<ProBishop>(pl, moves, n, promoted_pieces & pieces_on_board[pl] & PieceSetBishops);
		//龍
		n = generate_nocapture_sub<ProRook>(pl, moves, n, promoted_pieces & pieces_on_board[pl] & PieceSetRooks);
		return n;
	}
	template<PieceType pt>
	int Position::generate_nocapture_promotion(MoveArray& moves, int n)const{
		static_assert(pt < Gold);
		if(pt == Pawn){
			//歩の場合、駒の効きが1箇所にしかないため、BitBoardによる効き生成をしない
			PieceSet pawns = ~promoted_pieces & pieces_on_board[turn()] & PieceSetPawns;
			while(pawns){
				int id = sheena::bsf64(pawns);
				pawns &= pawns - 1;
				Square from = pieces[id].square();
				Square to = from + dir_diff[turn() == First? North : South];
				if(board[to].empty() && !discovered(turn(), from, to) && can_promote(turn(), Pawn, from, to)){
					moves[n++] = Move(Pawn, Empty, from, to, true);
				}
			}
		}
		else{
			//香車, 桂馬, 銀, 角, 飛車の場合
			PieceSet piece_set = ~promoted_pieces & pieces_on_board[turn()];
			switch(pt){
			case Lance:
				piece_set &= PieceSetLances;
				break;
			case Knight:
				piece_set &= PieceSetKnights;
				break;
			case Silver:
				piece_set &= PieceSetSilvers;
				break;
			case Bishop:
				piece_set &= PieceSetBishops;
				break;
			case Rook:
				piece_set &= PieceSetRooks;
				break;
			}
			while(piece_set){
				int id = sheena::bsf64(piece_set);
				piece_set &= piece_set - 1;
				Square from = pieces[id].square();
				BitBoard attack = attack_bb<pt>(turn(), from, all_bb);
				attack.remove(all_bb);
				attack.foreach([&](Square to){
					if(!discovered(turn(), from, to) && can_promote(turn(), pt, from, to)){
						moves[n++] = Move(pt, Empty, from, to, true);
					}
				});
			}
		}
		return n;
	}
	template int Position::generate_nocapture_promotion<Pawn>(MoveArray&, int)const;
	template int Position::generate_nocapture_promotion<Lance>(MoveArray&, int)const;
	template int Position::generate_nocapture_promotion<Knight>(MoveArray&, int)const;
	template int Position::generate_nocapture_promotion<Silver>(MoveArray&, int)const;
	template int Position::generate_nocapture_promotion<Bishop>(MoveArray&, int)const;
	template int Position::generate_nocapture_promotion<Rook>(MoveArray&, int)const;
	template<Player pl, int N>
	int Position::generate_drop_sub2(MoveArray& moves, int n, sheena::Array<PieceType, 6>& pieces)const{
		//2段目への駒打ち
		n = generate_drop_y<N>(moves, pl == First? xy_start + 1 : (xy_end - 2), n, pieces);
		if(hand[pl].count(Knight)){
			pieces[N] = Knight;
			for(int y=(pl==First? xy_start + 2 : xy_start); y < (pl == First? xy_end : xy_end - 2); y++){
				n = generate_drop_y<N + 1>(moves, y, n, pieces);
			}
		}
		else{
			for(int y=(pl==First? xy_start + 2 : xy_start); y < (pl == First? xy_end : xy_end - 2); y++){
				n = generate_drop_y<N>(moves, y, n, pieces);
			}
		}
		return n;
	}
	template<Player pl, int N>
	int Position::generate_drop_sub(MoveArray& moves, int n, sheena::Array<PieceType, 6>& pieces)const{
		//1段目への駒打ちの生成
		n = generate_drop_y<N>(moves, pl == First? xy_start : (xy_end - 1), n, pieces);
		//2段目以降への駒打ち
		if(hand[pl].count(Lance)){
			pieces[N] = Lance;
			return generate_drop_sub2<pl, N+1>(moves, n, pieces);
		}
		else{
			return generate_drop_sub2<pl, N>(moves, n, pieces);
		}
	}
	template<int N>
	int Position::generate_drop_y(MoveArray& moves, int y, int n, sheena::Array<PieceType, 6>& pieces)const{
		static_assert(N < 7);
		if(N == 0)return n;
		Square to = make_square(y, xy_start);
		for(int x=xy_start; x < xy_end; x++, to += BoardHeight){
			if(board[to].empty()){
				if(N>=1)moves[n++] = Move::move_drop(pieces[0], to);
				if(N>=2)moves[n++] = Move::move_drop(pieces[1], to);
				if(N>=3)moves[n++] = Move::move_drop(pieces[2], to);
				if(N>=4)moves[n++] = Move::move_drop(pieces[3], to);
				if(N>=5)moves[n++] = Move::move_drop(pieces[4], to);
				if(N>=6)moves[n++] = Move::move_drop(pieces[5], to);
			}
		}
		return n;
	}
	template<Player pl>
	int Position::generate_drop(MoveArray& moves, int n)const{
		if(!hand[pl])return n;
		//歩打ち生成
		if(hand[pl].count(Pawn)){
			BitBoard bb = board_bb;
			bb.remove(all_bb | pawn_exist_file_mask[pl]);
			bb.remove(rank_mask[make_square(pl == First? xy_start : (xy_end - 1), xy_start)]);
			bb.remove(square_bb[drop_pawn_mate_square()]);
			bb.foreach([&](Square to){
				moves[n++] = Move::move_drop(Pawn, to);
			});
		}
		//歩以外の駒打ち生成
		int mask = hand[pl].binary_mask();
		mask &= ~(1 << Pawn);
		if(!mask)return n;
		sheena::Array<PieceType, 6> sgbr_l;
		mask &= ~((1 << Lance) | (1 << Knight));
		int n_pieces = 0;
		sheena::Array<PieceType, 6> pieces;
		while(mask){
			pieces[n_pieces++] = static_cast<PieceType>(sheena::bsf32(mask));
			mask &= mask - 1;
		}
		switch(n_pieces){
		case 0:
			return generate_drop_sub<pl, 0>(moves, n, pieces);
		case 1:
			return generate_drop_sub<pl, 1>(moves, n, pieces);
		case 2:
			return generate_drop_sub<pl, 2>(moves, n, pieces);
		case 3:
			return generate_drop_sub<pl, 3>(moves, n, pieces);
		default://4
			return generate_drop_sub<pl, 4>(moves, n, pieces);
		}
	}
	template<PieceType pt>
	int Position::generate_drop_check_sub(MoveArray& moves, int n, Square ksq)const{
		const Player pl = turn_player;
		static_assert(Lance <= pt && pt < King);
		if(hand[pl].count(pt)){
			BitBoard target = attack_bb<pt>(opponent(pl), ksq, all_bb);
			target.remove(all_bb);
			target.foreach([&](Square to){
				moves[n++] = Move::move_drop(pt, to);
			});
		}
		return n;
	}
	int Position::generate_drop_check(MoveArray& moves, int n)const{
		assert(!check());
		const Player pl = turn();
		if(!hand[pl])return n;
		Square ksq = king_sq(opponent(pl));
		//歩打ち王手は、2歩と打ち歩の判定が必要
		if(hand[pl].count(Pawn)){
			Square to = ksq - dir_diff[pl == First? North : South];
			if(board[to].empty() && !pawn_exist(pl, to)
			&& drop_pawn_mate_square() != to){
				moves[n++] = Move::move_drop(Pawn, to);
			}
		}
		//歩以外の駒打ち王手
		n = generate_drop_check_sub<Lance>(moves, n, ksq);
		n = generate_drop_check_sub<Knight>(moves, n, ksq);
		n = generate_drop_check_sub<Silver>(moves, n, ksq);
		n = generate_drop_check_sub<Bishop>(moves, n, ksq);
		n = generate_drop_check_sub<Rook>(moves, n, ksq);
		n = generate_drop_check_sub<Gold>(moves, n, ksq);
		return n;
	}
	int Position::generate_evasion_sub(MoveArray& moves, int n, Square to, Square drop_pawn_mate_sq)const{
		const Player pl = turn();
		PieceType cap = board[to].type();
		//短い効きによる攻撃
		int shortcontrol = short_control[pl][to];
		while(shortcontrol){
			Square from = to - dir_diff[sheena::bsf32(shortcontrol)];
			shortcontrol &= shortcontrol - 1;
			PieceType pt = board[from].type();
			if(pt != King && !is_pinned(pl, from)){
				n = add_move(pl, moves, n, board[from].type(), cap, from, to);
			}
		}
		//長い効きによる攻撃
		int slidingcontrol = sliding_control[to] & own_slider[pl];
		while(slidingcontrol){
			int id = sliding2id[sheena::bsf32(slidingcontrol)];
			slidingcontrol &= slidingcontrol - 1;
			Square from = pieces[id].square();
			if(!is_pinned(pl, from)){
				n = add_move(pl, moves, n, pieces[id].type(), cap, from, to);
			}
		}
		//駒打ち(合い駒))
		if(cap == Empty && hand[pl]){
			for(PieceType pt = Silver;pt < King;pt++){
				if(hand[pl].count(pt))moves[n++] = Move::move_drop(pt, to);
			}
			if(!must_promote(pl, Pawn, to)){
				if(hand[pl].count(Pawn) && !pawn_exist(pl, to) && to != drop_pawn_mate_sq)moves[n++] = Move::move_drop(Pawn, to);
				if(hand[pl].count(Lance))moves[n++] = Move::move_drop(Lance, to);
				if(!must_promote(pl, Knight, to) && hand[pl].count(Knight))moves[n++] = Move::move_drop(Knight, to);
			}
		}
		return n;
	}
	int Position::generate_evasion(MoveArray& moves, int n)const{
		const Player pl = turn();
		const Player enemy = opponent(pl);
		Square ksq = king_sq(pl);
		assert(has_control(enemy, ksq));
		int slidingcontrol = sliding_control[ksq] & own_slider[enemy];
		//玉が動く手を生成する
		{
			BitBoard attack = king_attack[ksq];
			attack.foreach([&](Square to){
				if(!has_control(enemy, to)){
					if(board[to].empty()){
						if((slidingcontrol & square_relation[ksq][to]) == 0){
							moves[n++] = Move(King, Empty, ksq, to, false);
						}
					}else if(board[to].owner() == enemy){
						if((slidingcontrol & square_relation[ksq][to]) == 0){
							moves[n++] = Move(King, board[to].type(), ksq, to, false);
						}
					}
				}
			});
		}
		if(slidingcontrol == 0){
			assert(short_control[opponent(pl)][ksq]);
			//飛び効きによる王手がない場合
			//玉以外の駒で王手してきた駒をとる手を生成
			Square to = ksq - dir_diff[sheena::bsf32(short_control[opponent(pl)][ksq])];
			n = generate_evasion_sub(moves, n, to, SquareHand);
		}
		else{
			//両王手の場合は、玉移動以外の手はない
			if((slidingcontrol & (slidingcontrol - 1)) | short_control[opponent(pl)][ksq])return n;
			Square d = dir_diff[sliding2dir[sheena::bsf32(slidingcontrol)]];
			Square to = ksq - d;
			Square drop_pawn_mate_sq = drop_pawn_mate_square();
			for(; board[to].empty(); to -= d){
				n = generate_evasion_sub(moves, n, to, drop_pawn_mate_sq);
			}
			n = generate_evasion_sub(moves, n, to, drop_pawn_mate_sq);
		}
		return n;
	}

	template<Player pl>
	int Position::generate_moves(MoveArray& moves)const{
		if(check()){
			return generate_evasion(moves);
		}
		else{
			int n = generate_capture(moves, 0);
			n = generate_nocapture(moves, n);
			n = generate_drop<pl>(moves, n);
			return n;
		}
	}
	int Position::GenerateMoves(MoveArray& moves)const{
		if(turn() == First)return generate_moves<First>(moves);
		else return generate_moves<Second>(moves);
	}
	template int Position::generate_drop<First>(MoveArray&, int)const;
	template int Position::generate_drop<Second>(MoveArray&, int)const;
	template int Position::generate_moves<First>(MoveArray&)const;
	template int Position::generate_moves<Second>(MoveArray&)const;
	Square Position::drop_pawn_mate_square()const{
		const Player pl = turn();
		const Player king_side = opponent(pl);
		const Dir drop_dir = pl == First? South : North;
		const Square ksq = king_sq(opponent(pl));
		const Square sq = ksq + dir_diff[drop_dir];
		if(!board[sq].empty())return SquareHand;
		//sqのマスの歩を取ることができるか?
		int shortcontrol = short_control[king_side][sq];
		while(shortcontrol){
			Square from = sq - dir_diff[sheena::bsf32(shortcontrol)];
			shortcontrol &= shortcontrol - 1;
			if(board[from].type() != King && !is_pinned(king_side, from)){
				return SquareHand;
			}
		}
		//長い効きによる攻撃
		int slidingcontrol = sliding_control[sq] & own_slider[king_side];
		while(slidingcontrol){
			int id = sliding2id[sheena::bsf32(slidingcontrol)];
			slidingcontrol &= slidingcontrol - 1;
			Square from = pieces[id].square();
			if(!is_pinned(king_side, from)){
				return SquareHand;
			}
		}
		//玉が逃げられるマスの判定
		for(Dir dir = NW; dir <= SE; dir++){
			Square sq_ev = ksq + dir_diff[dir];
			if(sentinel_bb[sq_ev])continue;
			if(short_control[pl][sq_ev])continue;
			Piece cap = board[sq_ev];
			if(cap.empty() || cap.owner() == pl){
				//飛び効きがあるか判定
				if((sliding_control[sq_ev] & own_slider[pl] & cut_off_table[drop_dir][dir]) == 0){
					return SquareHand;
				}
			}
		}
		return sq;
	}
}