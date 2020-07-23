#include "shogi.hpp"
#include "bitboard.hpp"
#include "evaluate.hpp"

namespace shogi{
	const std::string file_string("x987654321xx");
	const std::string rank_string("xabcdefghix");
	const sheena::Array<std::string, PlayerDim> piece_string({
		"XPLNSBRGK","xplnsbrgk"
	});
	const sheena::Array<Square, DirDim> dir_diff({
		-BoardWidth - 1, -BoardWidth, -BoardWidth + 1,
		-1, 1,
		BoardWidth - 1, BoardWidth, BoardWidth + 1,
		- BoardWidth - 2, BoardWidth - 2,
		- BoardWidth + 2, BoardWidth + 2,
	});
	const sheena::Array<int, 24> sliding2id({
		LanceIDStart, LanceIDStart,
		LanceIDStart + 1, LanceIDStart + 1,
		LanceIDStart + 2, LanceIDStart + 2,
		LanceIDStart + 3, LanceIDStart + 3,
		RookIDStart, RookIDStart, RookIDStart, RookIDStart,
		RookIDStart + 1, RookIDStart + 1, RookIDStart + 1, RookIDStart + 1,
		
		BishopIDStart, BishopIDStart, BishopIDStart, BishopIDStart,
		BishopIDStart + 1, BishopIDStart + 1, BishopIDStart + 1, BishopIDStart + 1,
	});
	const sheena::Array<Dir, 24> sliding2dir({
		North, South,
		North, South,
		North, South,
		North, South,

		North, West, East, South,
		North, West, East, South,

		NW, NE, SW, SE,
		NW, NE, SW, SE,
	});
	const sheena::Array<SlidingControl, NumPiece> id2sliding({
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		Lance1Sliding, Lance2Sliding, Lance3Sliding, Lance4Sliding,
		0,0,0,0,
		0,0,0,0,
		Bishop1Sliding, Bishop2Sliding, 
		Rook1Sliding, Rook2Sliding,
		0,0,0,0,
		0,0,
	});
	sheena::Array2d<SlidingControl, 8, 8> cut_off_table;
	const sheena::Array<SlidingControl, 8> pin_mask({
		SlidingSE, SlidingEast, SlidingNE, SlidingSouth, SlidingNorth, SlidingSW, SlidingWest, SlidingNW,
	});
	sheena::Array2d<int8_t, BoardSize, 2> square_xy;
	sheena::Array2d<SlidingControl, BoardSize, BoardSize> square_relation;
	sheena::Array2d<int, PlayerDim, BoardSize> can_promote_table;
	sheena::Array2d<int, PlayerDim, BoardSize> must_promote_table;
	void init_tables(){
		init_bitboard();
		EvalIndex::init();
		for(Square sq1 = 0; sq1 < BoardSize; sq1++){
			square_relation[sq1][sq1] = 0;
			for(Square sq2 = 0; sq2 < sq1; sq2++){
				SlidingControl rel1 = 0, rel2 = 0;
				BitBoard filebb = file_mask[sq1] & file_mask[sq2];
				if(filebb){
					rel1 |= SlidingNorth;
					rel2 |= SlidingSouth;
				}
				BitBoard rankbb = rank_mask[sq1] & rank_mask[sq2];
				if(rankbb){
					rel1 |= SlidingWest;
					rel2 |= SlidingEast;
				}
				BitBoard diagbb = diag_mask[sq1] & diag_mask[sq2];
				if(diagbb){
					rel1 |= SlidingNW;
					rel2 |= SlidingSE;
				}
				BitBoard diag2bb = diag2_mask[sq1] & diag2_mask[sq2];
				if(diag2bb){
					rel1 |= SlidingSW;
					rel2 |= SlidingNE;
				}
				square_relation[sq1][sq2] = rel1;
				square_relation[sq2][sq1] = rel2;
			}
		}
		//cut_off_tableの初期化
		{
			Square sq = make_square(5, 5);
			for (Dir dir1 = NW; dir1 <= SE; dir1++) {
				for (Dir dir2 = NW; dir2 <= SE; dir2++) {
					cut_off_table[dir1][dir2] = ~square_relation[sq + dir_diff[dir1]][sq + dir_diff[dir2]];
				}
			}
		}
		//can_promote_table, must_promote_tableの初期化
		for(Player pl = First; pl < PlayerDim; pl++){
			for(Square sq = 0; sq < BoardSize; sq++){
				can_promote_table[pl][sq] = 0;
				must_promote_table[pl][sq] = 0;
				int y = squareY(sq);
				if(pl == Second)y = squareY(flip(sq));
				if(y < xy_start + 3){
					can_promote_table[pl][sq] = (1 << Gold) - 1;
				}
				if(y < xy_start + 2){
					must_promote_table[pl][sq] |= 1 << Knight;
				}
				if(y < xy_start + 1){
					must_promote_table[pl][sq] |= (1 << Pawn) | (1 << Lance);
				}
			}
		}
		//1手詰め用のテーブル初期化
		init_mate1_table();
	}
	const sheena::Array<int, Sentinel> material_value({
		0,
		PawnValue,
		LanceValue,
		KnightValue,
		SilverValue,
		BishopValue,
		RookValue,
		GoldValue,
		0,
		GoldValue,
		GoldValue,
		GoldValue,
		GoldValue,
		ProBishopValue,
		ProRookValue,
	});
	const sheena::Array<int, Sentinel> exchange_value({
		0,
		PawnValue * 2,
		LanceValue * 2,
		KnightValue * 2,
		SilverValue * 2,
		BishopValue * 2,
		RookValue * 2,
		GoldValue * 2,
		ProRookValue * 16,
		GoldValue + PawnValue,
		GoldValue + LanceValue,
		GoldValue + KnightValue,
		GoldValue + SilverValue,
		ProBishopValue + BishopValue,
		ProRookValue + RookValue,
	});
	const sheena::Array<int, King> promotion_value({
		0,
		GoldValue - PawnValue,
		GoldValue - LanceValue,
		GoldValue - KnightValue,
		GoldValue - SilverValue,
		ProBishopValue - BishopValue,
		ProRookValue - RookValue,
	});
}