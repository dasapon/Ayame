#pragma once

#include "shogi.hpp"

namespace shogi{
	class alignas(16) BitBoard{
		union{
			uint64_t b64[2];
			__m128i b128;
		};
		BitBoard(__m128i b):b128(b){}
	public:
		BitBoard(){}
		BitBoard(const BitBoard& bb):b128(bb.b128){}
		static bool out_of_range(int x){
			return (x & ~0x7f) != 0;
		}
		void clear(){
			b128 = _mm_setzero_si128();
		}
		void set(Square sq){
			b64[sq / 64] |= 1ULL << (sq % 64);
		}
		void remove(Square sq){
			b64[sq / 64] &= ~(1ULL << (sq % 64));
		}
		void remove(const BitBoard& rhs){
			b128 = _mm_andnot_si128(rhs.b128, b128);
		}
		void operator|=(const BitBoard& rhs){
			b128 = _mm_or_si128(b128, rhs.b128);
		}
		void operator&=(const BitBoard& rhs){
			b128 = _mm_and_si128(b128, rhs.b128);
		}
		void operator^=(const BitBoard& rhs){
			b128 = _mm_xor_si128(b128, rhs.b128);
		}
		BitBoard operator&(const BitBoard& rhs)const{
			return BitBoard(_mm_and_si128(b128, rhs.b128));
		}
		BitBoard operator|(const BitBoard& rhs)const{
			return BitBoard(_mm_or_si128(b128, rhs.b128));
		}
		bool operator[](Square sq)const{
			return (b64[sq / 64] & (1ULL << (sq % 64))) != 0;
		}
		uint64_t file_diag_hash(Square sq)const;
		uint64_t rank_hash(Square sq)const;
		template<typename FUNC>
		void foreach(FUNC func)const{
			uint64_t b = b64[0];
			while(b){
				Square sq = sheena::bsf64(b);
				b &= b - 1;
				func(sq);
			}
			b = b64[1];
			while(b){
				Square sq = 64 + sheena::bsf64(b);
				b &= b - 1;
				func(sq);
			}
		}
		operator bool()const{
			return (b64[0] | b64[1]) != 0;
		}
		std::string string()const{
			std::string ret = "";
			for(int y=xy_start;y<xy_end;y++){
				if(y != xy_start)ret += "\n";
				for(int x=xy_start;x<xy_end;x++){
					if((*this)[make_square(y, x)])ret += "1";
					else ret += "0";
				}
			}
			return ret;
		}
	};
	extern BitBoard sentinel_bb, board_bb;
	extern sheena::Array2d<BitBoard, PlayerDim, BoardSize> pawn_attack, lance_attack_mask, knight_attack, silver_attack, gold_attack;
	extern sheena::Array<BitBoard, BoardSize> king_attack, file_mask, rank_mask, diag_mask, diag2_mask, square_bb;
	extern sheena::Array2d<BitBoard, BoardSize, 128> file_diag_attack, rank_attack;
	extern void init_bitboard();
	extern BitBoard bishop_attack(Square sq, const BitBoard& occupied);
	extern BitBoard rook_attack(Square sq, const BitBoard& occupied);
	inline BitBoard lance_attack(Player pl, Square sq, const BitBoard& occupied){
		const BitBoard& mask = lance_attack_mask[pl][sq];
		uint64_t hash = (occupied & mask).file_diag_hash(sq);
		return file_diag_attack[sq][hash] & mask;
	}
	extern BitBoard attack_bb(Player pl, PieceType pt, Square sq, BitBoard occupied);
	template<PieceType pt>
	BitBoard attack_bb(Player pl, Square sq, BitBoard occupied){
		switch(pt){
		case Pawn:
			return pawn_attack[pl][sq];
		case Lance:
			return lance_attack(pl, sq, occupied);
		case Knight:
			return knight_attack[pl][sq];
		case Silver:
			return silver_attack[pl][sq];
		case Bishop:
			return bishop_attack(sq, occupied);
		case Rook:
			return rook_attack(sq, occupied);
		case Gold:
			return gold_attack[pl][sq];
		case King:
			return king_attack[sq];
		case ProPawn:
		case ProLance:
		case ProKnight:
		case ProSilver:
			return gold_attack[pl][sq];
		case ProBishop:
			return king_attack[sq] | bishop_attack(sq, occupied);
		case ProRook:
			return king_attack[sq] | rook_attack(sq, occupied);
		default:
			assert(false);
			return sentinel_bb;
		}
	}
}