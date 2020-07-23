#pragma once
#include "../sheena/sheena.hpp"

#include <atomic>
#include <cstring>
#include <iostream>
#include <map>
#include <climits>
#include <omp.h>
#include <unordered_map>
#include <bitset>
#include <tuple>
#include <random>
#include <functional>
#include <condition_variable>

namespace shogi{
	/**
	 * 盤面は11x11の121 + 7
	 *    0  11  22  33  44  55  66  77  88  99 110 121
	 *      -----------------------------------
	 *    1| 12  23  34  45  56  67  78  89 100|111 122
	 *    2| 13  24  35  46  57  68  79  90 101|112 123
	 *    3| 14  25  36  47  58  69  80  91 102|113 124
	 *    4| 15  26  37  48  59  70  81  92 103|114 125
	 *    5| 16  27  38  49  60  71  82  93 104|115 126
	 *    6| 17  28  39  50  61  72  83  94 105|116 127
	 *    7| 18  29  40  51  62  73  84  95 106|117
	 *    8| 19  30  41  52  63  74  85  96 107|118
	 *    9| 20  31  42  53  64  75  86  97 108|119
	 *      -----------------------------------
	 *   10  21  32  43  54  65  76  87  98 109 120
	 */
	constexpr int BoardWidth = 11, BoardHeight = 11;
	constexpr int BoardSize = 128;
	constexpr int xy_start = 1, xy_end = 10;
	constexpr int NumPiece = 40;
	constexpr int MaxLegalMove = 608;
	using Square = int;
	constexpr Square SquareHand = 0;
	inline Square make_square(int y, int x){
		return y + x * BoardHeight;
	}
	inline Square flip(Square sq){
		return 120 - sq;
	}
	inline int squareX(Square sq){
		return sq / BoardHeight;
	}
	inline int squareY(Square sq){
		return sq % BoardHeight;
	}
	enum Player{
		First,
		Second,
		PlayerDim,
	};
	inline Player operator++(Player& pl, int){
		Player p = pl;
		pl = static_cast<Player>(pl + 1);
		return p;
	}
	extern const std::string file_string;
	extern const std::string rank_string;
	inline constexpr Player opponent(Player pl){return static_cast<Player>(Second - pl);}
	inline int player2sign(Player pl){return 1 - pl * 2;}
	enum PieceType{
		Empty,
		Pawn,
		Lance,
		Knight,
		Silver,
		Bishop,
		Rook,
		Gold,
		King,
		ProPawn,
		ProLance,
		ProKnight,
		ProSilver,
		ProBishop,
		ProRook,
		Sentinel,
	};
	extern const sheena::Array<std::string, PlayerDim> piece_string;
	inline PieceType operator++(PieceType& pt, int){
		PieceType p = pt;
		pt = static_cast<PieceType>(pt + 1);
		return p;
	}
	inline constexpr PieceType promote(PieceType pt){
		assert(pt < King);
		return static_cast<PieceType>(pt + 8);
	}
	inline constexpr PieceType unpromote(PieceType pt){
		assert(pt != King);
		return static_cast<PieceType>(pt & 7);
	}

	class Piece{
		union{
			int32_t data32;
			sheena::Array<uint8_t, 4> data;
		};
	public:
		PieceType type()const{
			return static_cast<PieceType>(data[0]);
		}
		Square square()const{
			return static_cast<Square>(data[1]);
		}
		Player owner()const{
			return static_cast<Player>(data[2]);
		}
		int id()const{
			return static_cast<int>(data[3]);
		}
		bool empty()const{
			return data32 == 0;
		}
		void clear(){
			data32 = 0;
		}
		template<Player pl>
		bool is_enemy_or_empty()const{
			if(pl == First)return empty() || owner() == Second;
			else return owner() == First && type() != Sentinel;
		}
		Piece():data32(0){}
		Piece(const Piece& rhs):data32(rhs.data32){}
		void operator=(const Piece& rhs){
			data32 = rhs.data32;
		}
		void set(PieceType pt, Square sq, Player owner, int id){
			data[0] = pt;
			data[1] = sq;
			data[2] = owner;
			data[3] = id;
		}
	};
	using ShortControl = int16_t;
	enum Dir{
		NW,
		West,
		SW,
		North,
		South,
		NE,
		East,
		SE,
		NNW,
		NNE,
		SSW,
		SSE,
		DirDim,
	};
	inline Dir operator++(Dir& dir, int){
		Dir d = dir;
		dir = static_cast<Dir>(dir + 1);
		return d;
	}
	extern const sheena::Array<Square, DirDim> dir_diff;
	//駒番号
	using PieceSet = uint64_t;
	enum : PieceSet{
		PieceSetPawns = 0x3ffff,
		PieceSetLances = 0xfULL << 18,
		PieceSetKnights = 0xfULL << 22,
		PieceSetSilvers = 0xfULL << 26,
		PieceSetBishops = 0x3ULL << 30,
		PieceSetRooks = 0x3ULL << 32,
		PieceSetGolds = 0xfULL << 34,
		PieceSetFKing = 0x1ULL << 38,
		PieceSetSKing = 0x1ULL << 39,
		PieceSetKing = PieceSetFKing | PieceSetSKing,
		PieceSetSmalls = PieceSetPawns | PieceSetLances | PieceSetKnights | PieceSetSilvers | PieceSetGolds,
	};
	enum{
		PawnIDStart = 0,
		LanceIDStart = 18,
		KnightIDStart = 22,
		SilverIDStart = 26,
		BishopIDStart = 30,
		RookIDStart = 32,
		GoldIDStart = 34,
		FKingID = 38,
		SKingID = 39,
	};
	inline int king_id(Player pl){
		return FKingID + pl;
	}
	//飛び効き
	using SlidingControl = int32_t;
	enum :SlidingControl{
		Lance1North = 1,
		Lance1South = 2,
		Lance1Sliding = Lance1North | Lance1South,

		Lance2North = 1 << 2,
		Lance2South = 2 << 2,
		Lance2Sliding = Lance2North | Lance2South,
		
		Lance3North = 1 << 4,
		Lance3South = 2 << 4,
		Lance3Sliding = Lance3North | Lance3South,
		
		Lance4North = 1 << 6,
		Lance4South = 2 << 6,
		Lance4Sliding = Lance4North | Lance4South,

		Rook1North = 1 << 8,
		Rook1West = 2 << 8,
		Rook1East = 4 << 8,
		Rook1South = 8 << 8,
		Rook1Sliding = Rook1North | Rook1West | Rook1East | Rook1South,
		
		Rook2North = 1 << 12,
		Rook2West = 2 << 12,
		Rook2East = 4 << 12,
		Rook2South = 8 << 12,
		Rook2Sliding = Rook2North | Rook2West | Rook2East | Rook2South,

		Bishop1NW = 1 << 16,
		Bishop1NE = 2 << 16,
		Bishop1SW = 4 << 16,
		Bishop1SE = 8 << 16,
		Bishop1Sliding = Bishop1NW | Bishop1NE | Bishop1SW | Bishop1SE,

		Bishop2NW = 1 << 20,
		Bishop2NE = 2 << 20,
		Bishop2SW = 4 << 20,
		Bishop2SE = 8 << 20,
		Bishop2Sliding = Bishop2NW | Bishop2NE | Bishop2SW | Bishop2SE,

		SlidingNorth = Lance1North | Lance2North | Lance3North | Lance4North | Rook1North | Rook2North,
		SlidingSouth = Lance1South | Lance2South | Lance3South | Lance4South | Rook1South | Rook2South,
		SlidingWest = Rook1West | Rook2West,
		SlidingEast = Rook1East | Rook2East,

		SlidingNW = Bishop1NW | Bishop2NW,
		SlidingNE = Bishop1NE | Bishop2NE,
		SlidingSW = Bishop1SW | Bishop2SW,
		SlidingSE = Bishop1SE | Bishop2SE,
	};
	//飛び効きを駒番号に変換
	extern const sheena::Array<int, 24> sliding2id;
	//飛び効きを方向に変換
	extern const sheena::Array<Dir, 24> sliding2dir;
	//駒番号から飛び効きのマスクを取得
	extern const sheena::Array<SlidingControl, NumPiece> id2sliding;
	//ある地点のdir1方向に駒が置かれた時,dir2方向のマスの飛び効きが遮られるかを判定するために使用する
	extern sheena::Array2d<SlidingControl, 8, 8> cut_off_table;
	//ピン
	extern const sheena::Array<SlidingControl, 8> pin_mask;
	//square_relation[sq1][sq2]が飛び効きの8方向の位置にあるとき、その飛び効きのマスクを得る
	//例えば、sq1から見てsq2が上(北)にあるとき, square_relation[sq1][sq2] = SlidingNorthとなる
	extern sheena::Array2d<SlidingControl, BoardSize, BoardSize> square_relation;
	extern void init_tables();
	extern void init_mate1_table();
	enum{
		ValueInf = 32100,
		MateValue = 32000,
		SupValue = 30000,

		PawnValue = 100,
		LanceValue = 380,
		KnightValue = 400,
		SilverValue = 500,
		GoldValue = 600,
		BishopValue = 700,
		RookValue = 900,
		ProBishopValue = 1000,
		ProRookValue = 1200,
	};
	inline int value_mate_in(int ply){
		return MateValue - ply;
	}
	enum{
		DepthScale = 4,
		LowerLimitDepth = -8 * DepthScale,
	};
	extern const sheena::Array<int, Sentinel> material_value;
	extern const sheena::Array<int, Sentinel> exchange_value;
	extern const sheena::Array<int, King> promotion_value;
	//pl側から見てsqが何段目にあるか?
	template<Player pl>
	inline int enemy_side(Square sq){
		int y = squareY(sq);
		if(pl == First)return y;
		else return xy_end - y;
	}
	//その駒が成れるか、不成が許されるか
	extern sheena::Array2d<int, PlayerDim, BoardSize> can_promote_table;
	extern sheena::Array2d<int, PlayerDim, BoardSize> must_promote_table;
	inline bool can_promote(Player pl, PieceType pt, Square from, Square to){
		int mask = can_promote_table[pl][from];
		mask |= can_promote_table[pl][to];
		return (mask & (1 << pt)) != 0;
	}
	inline bool must_promote(Player pl, PieceType pt, Square to){
		return (must_promote_table[pl][to] & (1 << pt)) != 0;
	}
}