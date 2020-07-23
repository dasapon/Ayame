#pragma once
#include "shogi.hpp"

namespace shogi{
	enum HandRelation{
		HandRelEqual,
		HandRelSuperior,
		HandRelLesser,
		HandRelNone,
	};
	class Hand{
		uint64_t counts;
		sheena::Array2d<int, King, 18> stack;
		static constexpr uint64_t borrow = 0x8080808080808080ULL;
		static constexpr uint64_t magic_number = 0x40810204081ULL;
	public:
		Hand():counts(0){}
		int count(PieceType pt)const{
			return (counts >> (pt * 8)) & 0xff;
		}
		int count_all()const{
			uint64_t x = counts;
			x += x >> 32;
			x += x >> 16;
			x += x >> 8;
			return x & 0xff;
		}
		void push(PieceType pt, int id){
			int n = count(pt);
			counts += 1ULL << (pt * 8);
			assert(n < 18);
			stack[pt][n] = id;
		}
		int pop(PieceType pt){
			counts -= 1ULL << (pt * 8);
			assert(count(pt) < 18);
			return stack[pt][count(pt)];
		}
		int top(PieceType pt)const{
			return stack[pt][count(pt)];
		}
		void clear(){
			counts = 0;
		}
		operator bool()const{
			return counts != 0;
		}
		uint64_t get_counts()const{
			return counts;
		}
		HandRelation relation(uint64_t rhs_counts)const{
			if((counts - rhs_counts) & borrow){
				if((rhs_counts - counts) & borrow){
					return HandRelNone;
				}
				else{
					return HandRelLesser;
				}
			}
			else{
				if(counts == rhs_counts)return HandRelEqual;
				else return HandRelSuperior;
			}
		}
		//ある駒種が持ち駒にある時, (1 << PieceType) bit目がonになったmaskを得る
		//kindergarten bitboardでRookの縦の効きを求めるのと大体一緒
		int binary_mask()const{
			uint64_t mask = borrow - counts;
			mask = ~mask & borrow;
			mask *= magic_number;
			mask >>= 56;
			mask &= 0xff;
			return mask;
		}
	};
	
}