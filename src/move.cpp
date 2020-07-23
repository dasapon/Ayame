#include "move.hpp"
#include "state.hpp"

namespace shogi{
	static std::string sq2string(Square sq){
		return std::string({file_string[squareX(sq)], rank_string[squareY(sq)]});
	}
	std::string Move::string()const{
		if(!operator bool())return "resign";
		if(is_win())return "win";
		std::string ret;
		if(!is_drop())ret = sq2string(from()) + sq2string(to());
		else ret = std::string({piece_string[First][piece_type()], '*'}) + sq2string(to());
		if(is_promotion())ret += "+";
		return ret;
	}
	int Move::pack_15bit()const{
		int ret = move;
		if(is_drop()){
			ret += piece_type() << 7;
		}
		return ret & 0x7fff;
	}
	Move Move::unpack_15bit(int m, const State& state){
		Square to = m & 0x7f;
		Square from = (m >> 7) & 0x7f;
		if(from < King){
			return move_drop(static_cast<PieceType>(from), to);
		}
		bool promotion = m & PromotionFlag;
		PieceType pt = state.piece_on(from).type(), cap = state.piece_on(to).type();
		return Move(pt, cap, from, to, promotion);
	}
	class SeeQueue{
		//下位8bit -> square
		//上位24bit -> 駒の価値
		sheena::Array<int, 30> queue;
		int s, e;
	public:
		SeeQueue():s(1), e(1){
			queue[0] = INT_MIN;
		};
		bool empty()const{return s == e;}
		int pop(){
			return queue[s++];
		}
		void push(int value, Square sq){
			queue[e++] = value * BoardSize + sq;
		}
		void insert(int value, Square sq){
			int x = value * BoardSize + sq;
			int j;
			for(j = e; j > s && x < queue[j - 1]; j--){
				queue[j] = queue[j-1];
			}
			queue[j] = x;
			e++;
		}
		void sort(){
			for(int i=s + 1; i<e; i++){
				if(queue[i] < queue[i - 1]){
					int tmp = queue[i];
					int j = i;
					do{
						queue[j] = queue[j - 1];
						j--;
					}while(tmp < queue[j - 1]);
					queue[j] = tmp;
				}
			}
		}
	};
	int Position::see_sub(Square target_square, int hanged, Player pl, sheena::Array<SeeQueue, PlayerDim>& queue)const{
		if(queue[pl].empty())return 0;
		int x = queue[pl].pop();
		Square sq = x % BoardSize;
		int v = x / BoardSize;
		//効きが開くことによる駒追加
		int hidden_attacker_id = find_hidden_attacker(target_square, sq);
		if(hidden_attacker_id >= 0){
			Piece piece = pieces[hidden_attacker_id];
			queue[piece.owner()].insert(exchange_value[piece.type()], piece.square());
		}
		return std::max(0, hanged - see_sub(target_square, v, opponent(pl), queue));
	}
	//影効きを作っている駒を得る
	int Position::find_hidden_attacker(Square target_sq, Square forward_attacker_sq)const{
		SlidingControl hidden_control = sliding_control[forward_attacker_sq] & square_relation[forward_attacker_sq][target_sq];
		if(hidden_control){
			return sliding2id[sheena::bsf32(hidden_control)];
		}
		return -1;
	}
	//SEE
	int Position::see(Move move)const{
		Player pl = turn_player;
		Square to = move.to();
		Square from = move.from();
		PieceType pt = move.piece_type();
		int ret = exchange_value[move.capture()];
		int hanged = exchange_value[pt];
		if(move.is_promotion()){
			ret += promotion_value[pt];
			hanged += promotion_value[pt];
		}
		sheena::Array<SeeQueue, PlayerDim> attacker_queue;
		//toに効いている駒
		//影効き
		int hidden_piece_id = find_hidden_attacker(to, from);
		if(hidden_piece_id >= 0){
			Piece piece = pieces[hidden_piece_id];
			attacker_queue[piece.owner()].push(exchange_value[piece.type()], piece.square());
		}
		//防御側の短い効き
		int shortcontrol = short_control[opponent(pl)][to];
		while(shortcontrol){
			Dir dir = static_cast<Dir>(sheena::bsf32(shortcontrol));
			shortcontrol &= shortcontrol - 1;
			Square sq = to - dir_diff[dir];
			Piece piece = board[sq];
			attacker_queue[opponent(pl)].push(exchange_value[piece.type()], sq);
		}
		//飛び効き
		SlidingControl slidingcontrol = sliding_control[to];
		while(slidingcontrol){
			int id = sliding2id[sheena::bsf32(slidingcontrol)];
			slidingcontrol &= slidingcontrol - 1;
			Piece piece = pieces[id];
			Square sq = piece.square();
			if(sq != from){
				attacker_queue[piece.owner()].push(exchange_value[piece.type()], sq);
			}
		}
		//防御側の効きがない場合, 攻め方の効きを見る必要がない
		if(attacker_queue[opponent(pl)].empty())return ret;
		//攻撃側の短い効き
		shortcontrol = short_control[pl][to];
		while(shortcontrol){
			Dir dir = static_cast<Dir>(sheena::bsf32(shortcontrol));
			shortcontrol &= shortcontrol - 1;
			Square sq = to - dir_diff[dir];
			if(sq != from){
				Piece piece = board[sq];
				attacker_queue[pl].push(exchange_value[piece.type()], sq);
			}
		}
		//ソート
		attacker_queue[First].sort();
		attacker_queue[Second].sort();
		//
		return ret - see_sub(to, hanged, opponent(pl), attacker_queue);
	}
}