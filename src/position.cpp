#include "position.hpp"

namespace shogi{
	
	const Sfen startpos({
		"lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL", "b", "-", "1"
	});
	
	sheena::Array3d<uint64_t, Sentinel, BoardSize, PlayerDim> hash_seed;
	sheena::Array3d<uint64_t, 18, King, PlayerDim> hand_hash_seed;
	void init_hash_seed(){
		std::mt19937_64 mt(0);
		for(PieceType pt = Pawn; pt < Sentinel; pt++){
			for(Square sq = 0; sq < BoardSize; sq++){
				hash_seed[pt][sq][First] = mt() & ~1ULL;
				hash_seed[pt][sq][Second] = mt() & ~1ULL;
			}
		}
		for(PieceType pt = Pawn; pt < King; pt++){
			for(int n=0;n<18;n++){
				hand_hash_seed[n][pt][First] = mt() & ~1ULL;
				hand_hash_seed[n][pt][Second] = mt() & ~1ULL;
			}
		}
	}
	static PieceType piece_type_char(char c, Player* owner){
		for(Player pl = First; pl < PlayerDim; pl++){
			for(PieceType pt = Pawn; pt <= King; pt++){
				if(c == piece_string[pl][pt]){
					*owner = pl;
					return pt;
				}
			}
		}
		throw std::invalid_argument("c");
	}
	void SimplePosition::set_up(const std::string& board, Player turn, const std::string& hand){
		for(int i=0;i<NumPiece;i++)pieces[i].clear();
		sheena::Array<int, King + 1> piece_id({
			0, PawnIDStart, LanceIDStart, KnightIDStart, SilverIDStart,
			BishopIDStart, RookIDStart, GoldIDStart, 0,
		});
		//盤面の初期化
		int x = xy_start, y = xy_start;
		bool promotion_flag = false;
		for(char c: board){
			if(y >= xy_end)throw std::invalid_argument("a");
			if(c == '/'){
				if(x != xy_end)throw std::invalid_argument("b");
				y++;
				x = xy_start;
			}
			else if('1' <= c && c <= '9'){
				x += c - '0';
			}
			else if(c == '+'){
				promotion_flag = true;
			}
			else{
				Player pl;
				PieceType pt = piece_type_char(c, &pl);
				//駒番号を振る
				int id = piece_id[pt]++;
				if(pt == King)id = king_id(pl);
				//成の判定 駒番号の後に行う
				if(promotion_flag)pt = promote(pt);
				Square sq = make_square(y, x);
				pieces[id].set(pt, sq, pl, id);
				promotion_flag = false;
				x++;
			}
		}
		//手番
		this->turn = turn;
		//持ち駒
		int hand_count = 0;
		for(char c : hand){
			if('0' <= c && c <= '9'){
				hand_count *= 10;
				hand_count += c - '0';
			}
			else if(c == '-'){}
			else{
				Player pl;
				PieceType pt = piece_type_char(c, &pl);
				if(hand_count == 0)hand_count = 1;
				for(int i=0;i<hand_count;i++){
					int id = piece_id[pt]++;
					pieces[id].set(pt, SquareHand, pl, id);
				}
				hand_count = 0;
			}
		}
	}
	SimplePosition::SimplePosition(const Sfen& sfen){
		set_up(sfen[0], sfen[1] == "b" ? First : Second, sfen[2]);
	}

	SimplePosition::SimplePosition(const std::string& regularized_sfen){
		std::vector<std::string> v = sheena::split_string(regularized_sfen, ' ');
		set_up(v[0], First, v[1]);
	}
	static void xor_sliding(Square sq, Square d, SlidingControl mask, sheena::Array<Piece, BoardSize>& board, sheena::VInt<BoardSize>& controls){
		while(true){
			sq += d;
			controls[sq] ^= mask;
			if(!board[sq].empty())break;
		}
	}
	static void xor_control_short(Square sq, Dir dir, sheena::Array<ShortControl, BoardSize>& control){
		control[sq + dir_diff[dir]] ^= 1 << dir;
	}
	template<Player pl>
	void Position::xor_control(PieceType pt, Square sq, int id){
		constexpr Dir up = pl == First? North : South;
		constexpr Dir ul = pl == First? NW : SE;
		constexpr Dir ur = pl == First? NE : SW;
		switch(pt){
		case Pawn:
			xor_control_short(sq, up, short_control[pl]);
			break;
		case Lance:
			if(pl == First)xor_sliding(sq, dir_diff[North], id2sliding[id] & SlidingNorth, board, sliding_control);
			else xor_sliding(sq, dir_diff[South], id2sliding[id] & SlidingSouth, board, sliding_control);
			break;
		case Knight:
			if(pl == First){
				xor_control_short(sq, NNW, short_control[pl]);
				xor_control_short(sq, NNE, short_control[pl]);
			}else{
				xor_control_short(sq, SSW, short_control[pl]);
				xor_control_short(sq, SSE, short_control[pl]);
			}
			break;
		case Silver:
			xor_control_short(sq, NW, short_control[pl]);
			xor_control_short(sq, SW, short_control[pl]);
			xor_control_short(sq, up, short_control[pl]);
			xor_control_short(sq, NE, short_control[pl]);
			xor_control_short(sq, SE, short_control[pl]);
			break;
		case Bishop:
			xor_sliding(sq, dir_diff[NW], id2sliding[id] & SlidingNW, board, sliding_control);
			xor_sliding(sq, dir_diff[SW], id2sliding[id] & SlidingSW, board, sliding_control);
			xor_sliding(sq, dir_diff[NE], id2sliding[id] & SlidingNE, board, sliding_control);
			xor_sliding(sq, dir_diff[SE], id2sliding[id] & SlidingSE, board, sliding_control);
			break;
		case Rook:
			xor_sliding(sq, dir_diff[West], id2sliding[id] & SlidingWest, board, sliding_control);
			xor_sliding(sq, dir_diff[North], id2sliding[id] & SlidingNorth, board, sliding_control);
			xor_sliding(sq, dir_diff[South], id2sliding[id] & SlidingSouth, board, sliding_control);
			xor_sliding(sq, dir_diff[East], id2sliding[id] & SlidingEast, board, sliding_control);
			break;
		case Gold:
			xor_control_short(sq, ul, short_control[pl]);
			xor_control_short(sq, West, short_control[pl]);
			xor_control_short(sq, North, short_control[pl]);
			xor_control_short(sq, South, short_control[pl]);
			xor_control_short(sq, East, short_control[pl]);
			xor_control_short(sq, ur, short_control[pl]);
			break;
		case King:
			for(Dir dir = NW; dir <= SE; dir++)xor_control_short(sq, dir, short_control[pl]);
			break;
		case ProPawn:
		case ProLance:
		case ProKnight:
		case ProSilver:
			xor_control_short(sq, ul, short_control[pl]);
			xor_control_short(sq, West, short_control[pl]);
			xor_control_short(sq, North, short_control[pl]);
			xor_control_short(sq, South, short_control[pl]);
			xor_control_short(sq, East, short_control[pl]);
			xor_control_short(sq, ur, short_control[pl]);
			break;
		case ProBishop:
			xor_sliding(sq, dir_diff[NW], id2sliding[id] & SlidingNW, board, sliding_control);
			xor_sliding(sq, dir_diff[SW], id2sliding[id] & SlidingSW, board, sliding_control);
			xor_sliding(sq, dir_diff[NE], id2sliding[id] & SlidingNE, board, sliding_control);
			xor_sliding(sq, dir_diff[SE], id2sliding[id] & SlidingSE, board, sliding_control);
			xor_control_short(sq, West, short_control[pl]);
			xor_control_short(sq, North, short_control[pl]);
			xor_control_short(sq, South, short_control[pl]);
			xor_control_short(sq, East, short_control[pl]);
			break;
		case ProRook:
			xor_sliding(sq, dir_diff[West], id2sliding[id] & SlidingWest, board, sliding_control);
			xor_sliding(sq, dir_diff[North], id2sliding[id] & SlidingNorth, board, sliding_control);
			xor_sliding(sq, dir_diff[South], id2sliding[id] & SlidingSouth, board, sliding_control);
			xor_sliding(sq, dir_diff[East], id2sliding[id] & SlidingEast, board, sliding_control);
			xor_control_short(sq, NW, short_control[pl]);
			xor_control_short(sq, SW, short_control[pl]);
			xor_control_short(sq, NE, short_control[pl]);
			xor_control_short(sq, SE, short_control[pl]);
			break;
		default:
			assert(false);
		}
	}
	void Position::open_or_close_pin_slidings(Square sq){
		for(Player pl = First; pl < PlayerDim; pl++){
			if(pin[pl][sq]){
				//玉のいる方向dirを得る
				Dir dir = sliding2dir[sheena::bsf64(pin[pl][sq])];
				xor_sliding(sq, -dir_diff[dir], pin[pl][sq], board, pin[pl]);
			}
		}
		SlidingControl control = sliding_control[sq];
		while(control){
			int idx = sheena::bsf64(control);
			SlidingControl mask = 1 << idx;
			control &= ~mask;
			Dir dir = sliding2dir[idx];
			xor_sliding(sq, dir_diff[dir], mask, board, sliding_control);
		}
	}
	BitBoard Position::control_invalidation(Player pl, Square sq)const{
		BitBoard ret;
		ret.clear();
		SlidingControl control = sliding_control[sq] & own_slider[pl];
		while(control){
			int idx = sheena::bsf64(control);
			control &= control - 1;
			SlidingControl mask = 1 << idx;
			Dir dir = sliding2dir[idx];
			Square next = sq;
			while(true){
				next += dir_diff[dir];
				if(short_control[pl][next] == 0
				&& (sliding_control[next] & own_slider[pl] & ~mask) == 0){
					ret ^= square_bb[next];
				}
				if(!board[next].empty())break;
			}
		}
		return ret;
	}
	BitBoard Position::attack_neighbor8(Player pl, Square sq)const{
		BitBoard ret;
		ret.clear();
		for(Dir dir = NW; dir <= SE; dir++){
			Square s = sq + dir_diff[dir];
			if(board[s].type() != Sentinel && has_control(pl, s))ret ^= square_bb[s];
		}
		return ret;
	}
	void Position::set_pin(Player pl, Square sq){
		for(Dir dir = NW; dir <= SE;dir++){
			xor_sliding(sq, dir_diff[dir], pin_mask[dir], board, pin[pl]);
		}
	}
	Position::Position(){
		set_up(startpos);
	};
	Position::Position(const SimplePosition& sp){
		set_up(sp);
	}
	Position::Position(const Position& pos){
		memcpy(this, &pos, sizeof(Position));
	};
	void Position::set_up(const SimplePosition& sp){
		//初期化
		for(Player pl = First; pl < PlayerDim; pl++){
			pin[pl].clear();
			for(Square sq = 0;sq < BoardSize; sq++){
				short_control[pl][sq] = 0;
			}
			own_slider[pl] = 0;
			pieces_on_board[pl] = 0;
			hand[pl].clear();
			pawn_exist_file_mask[pl].clear();
		}
		sliding_control.clear();
		for(Square sq = 0;sq < BoardSize; sq++){
			board[sq].clear();
		}
		all_bb.clear();
		for(int i=0;i<NumPiece;i++){
			pieces[i].clear();
			eval_list[i] = EvalIndex::none();
		}
		promoted_pieces = 0;
		board_key = hand_key = 0;
		//番兵のセット
		sentinel_bb.foreach([&](Square sq){
			board[sq].set(Sentinel, sq, First, 0);
		});
		for(int id = 0; id < NumPiece; id++){
			Piece piece = sp.pieces[id];
			if(piece.empty())continue;
			Player pl = piece.owner();
			PieceType pt = piece.type();
			Square sq = piece.square();
			if(sq != SquareHand){
				if(pt > King){
					promoted_pieces ^= 1ULL << id;
				}
				put_piece(pl, pt, sq, id);
				pieces_on_board[pl] ^= 1ULL << id;
				if(pt == Pawn)pawn_exist_file_mask[pl] |= file_mask[sq];
			}
			else{
				inc_hand(pl, pt, id);
			}
			own_slider[pl] ^= id2sliding[id];
		}
		//効きとピン情報のセット
		for(int id = 0; id < NumPiece; id++){
			if(pieces[id].square() == SquareHand)continue;
			if(pieces[id].owner() == First)xor_control<First>(pieces[id].type(), pieces[id].square(), id);
			else xor_control<Second>(pieces[id].type(), pieces[id].square(), id);
		}
		set_pin(First, king_sq(First));
		set_pin(Second, king_sq(Second));
		//手番
		turn_player = sp.turn;
		board_key ^= turn_player;
	};
	template<Player pl>
	EvalListDiff Position::make_move(Move move){
		EvalListDiff difference;
		assert(pl == turn());
		//null move
		if(!move){
			//手番反転
			turn_player = opponent(pl);
			board_key ^= 1;
			return difference;
		}
		assert(is_move_valid(move));
		//駒打ちの場合
		//(A-1)駒台の駒を減らす
		//(A-2)移動先に駒を置く
		//(A-3)pieces_on_board, pawn_exist_file_maskの更新
		//(A-4)移動後の駒の効きをセット
		//(A-5)遮断した効きやピンの更新
		//駒打ちでない場合
		//(B-1)玉を動かす手であれば、pin情報をクリア
		//(B-2)動かす駒の効きを消す
		//(B-3)駒を取る手の場合
			//(B-3-1)取られる駒の効きを消す
			//(B-3-2)fromの駒を消し、fromの飛び効き、ピン情報を更新
			//(B-3-3)取られる駒を駒台へ
			//(B-3-4)own_slider, pieces_on_board, pawn_exist_file_maskの更新
		//(B-4)駒を取らない手の場合
			//(B-4-1)fromの駒を消し、fromの飛び効き、ピン情報を更新
			//(B-4-2)toの飛び効き、ピン情報を更新
		//(B-5)toに駒を置く promoted_pieces, pawn_exist_file_maskも必要に応じて更新
		//(B-6)動かした駒が玉であれば、pin情報を更新
		Square to = move.to();
		Square from = move.from();
		PieceType pt = move.piece_type();
		PieceType cap = move.capture();
		//駒打ちの場合
		if(move.is_drop()){
			assert(hand[pl].count(pt) > 0);
			//(A-1)駒台の駒を減らす
			int id = dec_hand(pl, pt);
			difference.moved.first = id;
			difference.moved.second = eval_list[id];
			//(A-2)移動先に駒を置く
			put_piece(pl, pt, to, id);
			//(A-3)pieces_on_board, pawn_exist_file_maskの更新
			pieces_on_board[pl] ^= 1ULL << id;
			if(pt == Pawn)pawn_exist_file_mask[pl] |= file_mask[to];
			//(A-4)移動後の駒の効きをセット
			xor_control<pl>(pt, to, id);
			//(A-5)遮断した効きやピンの更新
			open_or_close_pin_slidings(to);
		}
		//駒打ちでない場合
		else{
			//(B-1)玉を動かす手であれば、pin情報をクリア
			if(pt == King)pin[pl].clear();
			//(B-2)動かす駒の効きを消す
			int id = board[from].id();
			difference.moved.first = id;
			difference.moved.second = eval_list[id];
			xor_control<pl>(pt, from, id);
		//(B-3)駒を取る手の場合
			if(cap != Empty){
				//(B-3-1)取られる駒の効きを消す
				int cap_id = board[to].id();
				difference.captured.first = cap_id;
				difference.captured.second = eval_list[cap_id];
				xor_control<opponent(pl)>(cap, to, cap_id);
				//(B-3-2)fromの駒を消し、fromの飛び効き、ピン情報を更新
				remove_piece<pl>(from);
				open_or_close_pin_slidings(from);
				//(B-3-3)取られる駒を駒台へ
				remove_piece<opponent(pl)>(to);
				inc_hand(pl, unpromote(cap), cap_id);
				//(B-3-4)own_slider, pieces_on_board, pawn_exist_file_maskの更新
				own_slider[First] ^= id2sliding[cap_id];
				own_slider[Second] ^= id2sliding[cap_id];
				if(cap == Pawn)pawn_exist_file_mask[opponent(pl)].remove(file_mask[to]);
				else if(cap > King)promoted_pieces ^= 1ULL << cap_id;
				pieces_on_board[opponent(pl)] ^= 1ULL << cap_id;
			}
			else{
				//(B-4-1)fromの駒を消し、fromの飛び効き、ピン情報を更新
				remove_piece<pl>(from);
				open_or_close_pin_slidings(from);
				//(B-4-2)toの飛び効き、ピン情報を更新
				open_or_close_pin_slidings(to);
			}
			//(B-5)toに駒を置く promoted_pieces, pawn_exist_file_maskも必要に応じて更新
			if(move.is_promotion()){
				if(pt == Pawn)pawn_exist_file_mask[pl].remove(file_mask[to]);
				promoted_pieces ^= 1ULL << id;
				pt = promote(pt);
			}
			put_piece(pl, pt, to, id);
			xor_control<pl>(pt, to, id);
			//(B-6)動かした駒が玉であれば、pin情報を更新
			if(pt == King){
				set_pin(pl, to);
			}
		}
		assert(!check());
		//手番反転
		turn_player = opponent(pl);
		board_key ^= 1;
		return difference;
	}
	template<Player pl>
	void Position::unmake_move(Move move){
		assert(pl != turn());
		//基本方針 : make_moveを逆向きに
		//手番反転
		turn_player = pl;
		board_key ^= 1;
		if(!move)return;
		//駒打ちの場合
		//(A-1)toの飛び効き、ピンの更新
		//(A-2)駒の効きを消去
		//(A-3)pieces_on_board, pawn_exist_file_maskの更新
		//(A-4)toの駒を消す
		//(A-5)駒台に駒を置く

		//駒打ちでない場合
		//(B-1)動かした駒が玉であれば、pin情報をクリア
		//(B-2)toの駒を取り除く promoted_pieces, pawn_exist_file_maskも必要に応じて更新
		//(B-3)駒を取る手だった場合
			//(B-3-1)own_slider, pieces_on_board, pawn_exist_file_maskの更新
			//(B-3-2)取られた駒を戻す
			//(B-3-3)fromの駒を戻し、fromの飛び効き、ピン情報を更新
			//(B-3-4)取られた駒の効きを戻す
		//(B-4)駒を取らない手の場合
			//(B-4-1)toの飛び効き、ピン情報を更新
			//(B-4-2)fromの駒を戻し、fromの飛び効き、ピン情報を更新
		//(B-5)動かした駒の効きを戻す
		//(B-6)動かした駒が玉であれば、pin情報をセット
		Square to = move.to();
		Square from = move.from();
		PieceType pt = move.piece_type();
		PieceType cap = move.capture();
		//駒打ちの場合
		if(move.is_drop()){
			int id = board[to].id();
			//(A-1)toの飛び効き、ピンの更新
			open_or_close_pin_slidings(to);
			//(A-2)駒の効きを消去
			xor_control<pl>(pt, to, id);
			//(A-3)pieces_on_board, pawn_exist_file_maskの更新
			pieces_on_board[pl] ^= 1ULL << id;
			if(pt == Pawn)pawn_exist_file_mask[pl].remove(file_mask[to]);
			//(A-4)toの駒を消す
			remove_piece<pl>(to);
			//(A-5)駒台に駒を置く
			inc_hand(pl, pt, id);
		}
		else{
			//(B-1)動かした駒が玉であれば、pin情報をクリア
			if(pt == King)pin[pl].clear();
			//(B-2)toの駒を取り除く promoted_pieces, pawn_exist_file_maskも必要に応じて更新
			int id = board[to].id();
			remove_piece<pl>(to);
			if(move.is_promotion()){
				if(pt == Pawn)pawn_exist_file_mask[pl] |= file_mask[to];
				promoted_pieces ^= 1ULL << id;
				xor_control<pl>(promote(pt), to, id);
			}
			else{
				xor_control<pl>(pt, to, id);
			}
			//(B-3)駒を取る手だった場合
			if(cap != Empty){
				int cap_id = dec_hand(pl, unpromote(cap));
				//(B-3-1)own_slider, pieces_on_board, pawn_exist_file_maskの更新
				own_slider[First] ^= id2sliding[cap_id];
				own_slider[Second] ^= id2sliding[cap_id];
				if(cap == Pawn)pawn_exist_file_mask[opponent(pl)] |= file_mask[to];
				else if(cap > King)promoted_pieces ^= 1ULL << cap_id;
				pieces_on_board[opponent(pl)] ^= 1ULL << cap_id;
				//(B-3-2)取られた駒を戻す
				put_piece(opponent(pl), cap, to, cap_id);
				//(B-3-3)fromの駒を戻し、fromの飛び効き、ピン情報を更新
				put_piece(pl, pt, from, id);
				open_or_close_pin_slidings(from);
				//(B-3-4)取られた駒の効きを戻す
				xor_control<opponent(pl)>(cap, to, cap_id);
			}
			//(B-4)駒を取らない手の場合
			else{
				//(B-4-1)toの飛び効き、ピン情報を更新
				open_or_close_pin_slidings(to);
				//(B-4-2)fromの駒を戻し、fromの飛び効き、ピン情報を更新
				put_piece(pl, pt, from, id);
				open_or_close_pin_slidings(from);
			}
			//(B-5)動かした駒の効きを戻す
			xor_control<pl>(pt, from, id);
			//(B-6)動かした駒が玉であれば、pin情報をセット
			if(pt == King){
				set_pin(pl, from);
			}
		}
	}
	template EvalListDiff Position::make_move<First>(Move);
	template EvalListDiff Position::make_move<Second>(Move);
	template void Position::unmake_move<First>(Move);
	template void Position::unmake_move<Second>(Move);
	Move Position::str2move(std::string str)const{
		Square to = make_square(str[3] - 'a' + xy_start, xy_end - str[2] + '0');
		bool promotion = str.size() == 5;
		Square from;
		PieceType pt = Empty;
		if(str[1] == '*'){
			from = 0;
			for(;pt < King; pt++){
				if(piece_string[First][pt] == str[0])break;
			}
		}
		else{
			from = make_square(str[1] - 'a' + xy_start, xy_end - str[0] + '0');
			pt = board[from].type();
		}
		PieceType cap = board[to].type();
		return Move(pt, cap, from, to, promotion);
	}
	std::string Position::board_sfen(bool flip)const{
		std::string ret;
		int empty_count = 0;
		for(int y=xy_start; y < xy_end; y++){
			for(int x = xy_start; x < xy_end;x++){
				Square sq = make_square(y, x);
				if(flip) sq = shogi::flip(sq);
				Piece piece = board[sq];
				if(piece.empty())empty_count++;
				else{
					if(empty_count)ret += std::to_string(empty_count);
					empty_count = 0;
					Player owner = piece.owner();
					if(flip)owner = opponent(owner);
					if(piece.type() > King){
						//成駒
						ret += "+";
						ret += piece_string[owner][unpromote(piece.type())];
					}
					else{
						ret += piece_string[owner][piece.type()];
					}
				}
			}
			if(empty_count)ret += std::to_string(empty_count);
			empty_count = 0;
			if(y + 1 < xy_end)ret += "/";
		}
		return ret;
	}
	std::string Position::hand_sfen(bool flip)const{
		//持ち駒
		if(!hand[First] && !hand[Second])return "-";
		std::string ret;
		for(Player pl = First; pl < PlayerDim; pl++){
			for(PieceType pt = Pawn; pt < King; pt++){
				int n = hand[flip ? opponent(pl) : pl].count(pt);
				if(n > 1)ret += std::to_string(n);
				if(n)ret += piece_string[pl][pt];
			}
		}
		return ret;
	}
	
	Sfen Position::sfen(bool flip)const{
		Sfen ret;
		ret[0] = board_sfen(flip);
		//手番
		if(turn() == (flip? Second : First))ret[1] = "b";
		else ret[1] = "w";
		ret[2] = hand_sfen(flip);
		ret[3] = "1";
		return ret;
	}

	template<Player pl>
	bool Position::nyugyoku()const {
		if (enemy_side<pl>(king_sq(pl)) > 3)return false;
		int point = 0;
		int number = 0;
		for (int i = 0; i < FKingID; i++) {
			Piece piece = this->piece_id(i);
			if (piece.empty() || piece.owner() != pl)continue;
			Square sq = piece.square();
			PieceType pt = unpromote(piece.type());
			if (sq == SquareHand) {
				if (pt == Bishop || pt == Rook)point += 5;
				else point++;
			}
			else if (enemy_side<pl>(sq) <= 3) {
				if (pt == Bishop || pt == Rook)point += 5;
				else point++;
				number++;
			}
		}
		return point >= 28 - pl && number >= 10;
	}
	bool Position::nyugyoku_declaration()const{
		assert(!check());
		if (turn() == First) {
			return nyugyoku<First>();
		}
		else {
			return nyugyoku<Second>();
		}
	}
}