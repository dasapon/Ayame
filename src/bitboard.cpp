#include "bitboard.hpp"

namespace shogi{
	BitBoard sentinel_bb, board_bb;
	sheena::Array2d<BitBoard, PlayerDim, BoardSize> pawn_attack, lance_attack_mask, knight_attack, silver_attack, gold_attack;
	sheena::Array<BitBoard, BoardSize> king_attack, file_mask, rank_mask, diag_mask, diag2_mask, square_bb;
	sheena::Array2d<BitBoard, BoardSize, 128> file_diag_attack, rank_attack;
	/**
	 *    0  11  22  33  44  55  66  77  88  99 110 121
	 *      -----------------------------------
	 *    1| 12  23  34  45  56  67  78  89 100|111 122
	 *    2| 13  24  35  46  57  68  79  90 101|112 123
	 *    3| 14  25  36  47  58  69  70  91 102|113 124
	 *    4| 15  26  37  48  59  70  81  92 103|114 125
	 *    5| 16  27  38  49  60  71  82  93 104|115 126
	 *    6| 17  28  39  50  61  72  83  94 105|116 127
	 *    7| 18  29  40  51  62  73  84  95 106|117
	 *    8| 19  30  41  52  63  74  85  96 107|118
	 *    9| 20  31  42  53  64  75  86  97 108|119
	 *      -----------------------------------
	 *   10  21  32  43  54  65  76  87  98 109 120
	 * 
	 * rotated bitboard
	 * 縦の効き, 斜めの効き
	 * 2つのbitsのマージ (b64[1] << 20) | b64[0]
	 * magic number
	 * 1 | (1 << 11) | (1 << 22) | (1 << 33) | (1 << 44)
	 * 
	 * 横の効き
	 * マージ (b64[0] >> (y - 1 + 23)) | (b64[1] << (9 - y + 3))
	 * orの直前に立っているbit
	 * 	b64[0]:0, 11, 22, (33)
	 *  b64[1]:(3), 14, 25, 36
	 * magic number
	 * (1 <<  27) | (1 << 37) | (1 << 47) | (1 << 57)
	 */
	static constexpr uint64_t file_diag_magic = 1 | (1ULL << 11) | (1ULL << 22) | (1ULL << 33) | (1ULL << 44);
	static constexpr uint64_t rank_magic = (1ULL <<  27) | (1ULL << 37) | (1ULL << 47) | (1ULL << 57);
	uint64_t BitBoard::file_diag_hash(Square sq)const{
		uint64_t mask = b64[0] | (b64[1] << 20);
		mask *= file_diag_magic;
		return mask >> 57;
	}
	uint64_t BitBoard::rank_hash(Square sq)const{
		int y = squareY(sq);
		uint64_t mask = (b64[0] >> (y + 22)) | (b64[1] << (12 - y));
		mask *= rank_magic;
		return mask >> 57;
	}
	BitBoard bishop_attack(Square sq, const BitBoard& occupied){
		uint64_t diag_hash = (occupied & diag_mask[sq]).file_diag_hash(sq);
		BitBoard ret = file_diag_attack[sq][diag_hash] & diag_mask[sq];
		uint64_t diag2_hash = (occupied & diag2_mask[sq]).file_diag_hash(sq);
		ret |= file_diag_attack[sq][diag2_hash] & diag2_mask[sq];
		return ret;
	}
	BitBoard rook_attack(Square sq, const BitBoard& occupied){
		//縦の効き
		uint64_t file_hash = (occupied & file_mask[sq]).file_diag_hash(sq);
		BitBoard ret = file_diag_attack[sq][file_hash] & file_mask[sq];
		//横の効き
		uint64_t rank_hash = (occupied & rank_mask[sq]).rank_hash(sq);
		ret |= rank_attack[sq][rank_hash] & rank_mask[sq];
		return ret;
	}
	void init_bitboard(){
		//番兵の初期化
		sentinel_bb.clear();
		board_bb.clear();
		for(Square sq = 0; sq < BoardSize; sq++){
			sentinel_bb.set(sq);
		}
		for(int y=xy_start;y<xy_end;y++){
			for(int x=xy_start;x<xy_end;x++){
				board_bb.set(make_square(y, x));
			}
		}
		sentinel_bb.remove(board_bb);
		//効きの初期化
		for(Square sq = 0;sq < BoardSize; sq++){
			for(Player pl = First; pl < PlayerDim; pl++){
				pawn_attack[pl][sq].clear();
				knight_attack[pl][sq].clear();
				silver_attack[pl][sq].clear();
				gold_attack[pl][sq].clear();
			}
			king_attack[sq].clear();
			if(sentinel_bb[sq])continue;

			auto set = [=](BitBoard& bb, Dir dir){
				Square target = sq + dir_diff[dir];
				if(target >= 0 && target < BoardSize && !sentinel_bb[target]){
					bb.set(target);
				}
			};
			set(pawn_attack[First][sq], North);
			set(pawn_attack[Second][sq], South);
			set(knight_attack[First][sq], NNW);
			set(knight_attack[First][sq], NNE);
			set(knight_attack[Second][sq], SSW);
			set(knight_attack[Second][sq], SSE);

			for(Dir dir = NW; dir <= SE; dir++){
				set(king_attack[sq], dir);
				if(dir != East && dir != West){
					if(dir != South)set(silver_attack[First][sq], dir);
					if(dir != North)set(silver_attack[Second][sq], dir);
				}
				if(dir != SW && dir != SE)set(gold_attack[First][sq], dir);
				if(dir != NW && dir != NE)set(gold_attack[Second][sq], dir);
			}
		}
		//マスクの初期化
		for(Square sq = 0; sq < BoardSize; sq++){
			file_mask[sq].clear();
			rank_mask[sq].clear();
			diag_mask[sq].clear();
			diag2_mask[sq].clear();
			lance_attack_mask[First][sq].clear();
			lance_attack_mask[Second][sq].clear();
			square_bb[sq].clear();
			square_bb[sq].set(sq);
			if(sentinel_bb[sq])continue;
			auto set = [=](BitBoard& bb, Dir dir){
				Square next = sq + dir_diff[dir];
				while(!sentinel_bb[next]){
					bb.set(next);
					next += dir_diff[dir];
				}
			};
			file_mask[sq].set(sq);
			rank_mask[sq].set(sq);
			diag_mask[sq].set(sq);
			diag2_mask[sq].set(sq);
			set(file_mask[sq], North);
			set(file_mask[sq], South);
			set(rank_mask[sq], East);
			set(rank_mask[sq], West);
			set(diag_mask[sq], NW);
			set(diag_mask[sq], SE);
			set(diag2_mask[sq], NE);
			set(diag2_mask[sq], SW);
			set(lance_attack_mask[First][sq], North);
			set(lance_attack_mask[Second][sq], South);
		}
		//縦効き、斜め効きの初期化
		for(int hash = 0; hash < 128; hash++){
			BitBoard occupied;
			occupied.clear();
			BitBoard occupied_rank;
			occupied_rank.clear();
			for(int x=xy_start;x<xy_end;x++){
				for(int y=xy_start;y<xy_end;y++){
					if((hash << (xy_start + 1)) & (1 << y))occupied.set(make_square(y, x));
					if(hash << (xy_start + 1) & (1 << x))occupied_rank.set(make_square(y, x));
				}
			}
			for(Square sq = 0;sq < BoardSize;sq++){
				file_diag_attack[sq][hash].clear();
				rank_attack[sq][hash].clear();
				if(sentinel_bb[sq])continue;
				auto set = [&](BitBoard all, BitBoard& bb, Dir dir){
					Square next = sq + dir_diff[dir];
					while(!sentinel_bb[next]){
						bb.set(next);
						if(all[next])break;
						next += dir_diff[dir];
					}
				};
				set(occupied, file_diag_attack[sq][hash], North);
				set(occupied, file_diag_attack[sq][hash], South);
				set(occupied, file_diag_attack[sq][hash], NW);
				set(occupied, file_diag_attack[sq][hash], NE);
				set(occupied, file_diag_attack[sq][hash], SW);
				set(occupied, file_diag_attack[sq][hash], SE);
				set(occupied_rank, rank_attack[sq][hash], West);
				set(occupied_rank, rank_attack[sq][hash], East);
			}
		}
	}
	BitBoard attack_bb(Player pl, PieceType pt, Square sq, BitBoard occupied){
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