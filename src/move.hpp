#pragma once

#include "shogi.hpp"

namespace shogi{
	class State;
	class Move{
		/**
		 * xxxx xxxx xxxx xxxx xxxx xxxx x111 1111 to
		 * xxxx xxxx xxxx xxxx xx11 1111 1xxx xxxx from
		 * xxxx xxxx xxxx xxxx x1xx xxxx xxxx xxxx promotion
		 * xxxx xxxx xxxx x111 1xxx xxxx xxxx xxxx piecetype
		 * xxxx xxxx x111 1xxx xxxx xxxx xxxx xxxx capture
		 */
		static constexpr int PromotionFlag = 1 << 14;
		uint32_t move;
		Move(int i):move(i){}
	public:
		Move(){}
		Move(PieceType pt, PieceType capture, Square from, Square to, bool promotion){
			move = (capture << 19) | (pt << 15) | (from << 7) | to;
			if(promotion)move |= PromotionFlag;
		}
		static Move move_drop(PieceType pt, Square to){
			return (pt << 15) | to;
		}
		static Move null_move(){
			return Move(0);
		}
		static Move win(){
			return Move(1);
		}
		Square to()const{
			return move & 0x7f;
		}
		Square from()const{
			return (move >> 7) & 0x7f;
		}
		PieceType piece_type()const{
			return static_cast<PieceType>((move >> 15) & 0xf);
		}
		PieceType pt_promoted()const{
			if(is_promotion())return promote(piece_type());
			else return piece_type();
		}
		PieceType capture()const{
			return static_cast<PieceType>((move >> 19) & 0xf);
		}
		bool is_drop()const{
			return from() == 0;
		}
		bool is_promotion()const{
			return (move & PromotionFlag) != 0;
		}
		bool is_capture()const{
			return (move & (0xf << 19)) != 0;
		}
		bool is_win()const{
			return move == 1;
		}
		void operator=(const Move& rhs){
			move = rhs.move;
		}
		bool operator==(const Move& rhs)const{
			return move == rhs.move;
		}
		bool operator!=(const Move& rhs)const{
			return move != rhs.move;
		}
		operator bool()const{
			return move != 0;
		}
		int mvv_lva()const{
			return exchange_value[capture()] * 256 - exchange_value[piece_type()];
		}
		int estimate()const{
			int ret = exchange_value[capture()];
			if(is_promotion()){
				ret += promotion_value[piece_type()];
			}
			return ret;
		}
		int pack_15bit()const;
		static Move unpack_15bit(int m, const State& state);
		std::string string()const;
	};

	using PV = sheena::Array<Move, 128>;
}