#pragma once

#include "move.hpp"

namespace shogi{
	enum ValueType{
		Invalid = 0,
		LowerBound,
		UpperBound,
		Exact,
	};
	class HashEntry{
		friend class HashTable;
		/**データの中身
		 * valueType	 2bit
		 * generation	 2bit
		 * value 		16bit
		 * move			15bit
		 * depth		 9bit
		 * key			20bit
		 * 
		 * 下位から見た位置
		 * 00 - 15 value
		 * 16 - 30 move
		 * 31 - 39 depth
		 * 40 - 41 value_type
		 * 42 - 43 generation
		 * 44 - 63 key
		 */ 
		uint64_t data;
		static constexpr uint64_t generation_mask = 3ULL << 42;
		static constexpr uint64_t key_mask = 0xfffffULL << 44;
		void update_generation(uint64_t gen){
			data &= ~generation_mask;
			data |= gen;
		}
	public:
		uint64_t generation()const{
			return data & generation_mask;
		}
		int value()const{
			return static_cast<int16_t>(data & 0xffffULL);
		}
		Move move(const State& state)const{
			return Move::unpack_15bit((data >> 16) & 0x7fff, state);
		}
		int depth()const{
			return static_cast<int>((data >> 31) & 0x1ffULL) + LowerLimitDepth;
		}
		ValueType value_type()const{
			return static_cast<ValueType>((data >> 40) & 0x3ULL);
		}
		bool match(uint64_t key)const{
			return (key & key_mask) == (data & key_mask);
		}
		HashEntry():data(0){}
		HashEntry(uint64_t key, int alpha, int beta, int value, Move bestmove, int depth, uint64_t gen){
			ValueType vt = value <= alpha ? UpperBound : (value >= beta? LowerBound : Exact);
			assert(value >= -MateValue);
			assert(depth >= LowerLimitDepth);
			assert((gen & ~generation_mask) == 0);
			data = (static_cast<uint64_t>(value) & 0xffffULL)
			| (static_cast<uint64_t>(bestmove.pack_15bit()) << 16)
			| (static_cast<uint64_t>(depth - LowerLimitDepth) << 31)
			| (static_cast<uint64_t>(vt) << 40)
			| gen
			| (key & key_mask);
			assert(match(key));
		}
		void operator=(const HashEntry& rhs){
			data = rhs.data;
		}
	};
	class HashTable{
		static constexpr size_t ways = 4;
		sheena::ArrayAlloc<sheena::Array<HashEntry, ways>> tbl;
		uint64_t generation, mask;
		size_t hash_used;
		void store_sub(HashEntry entry, uint64_t key);
		int priority(HashEntry e)const{
			if(e.value_type() == Invalid){
				return -65536 * 2;
			}
			if(e.generation() == generation){
				return e.depth();
			}
			else{
				return -65536 + e.depth();
			}
		}
	public:
		HashTable();
		void resize(int mb);
		void new_gen(){
			generation += 1ULL << 42;
			generation &= HashEntry::generation_mask;
			hash_used = 0;
		}
		void clear(){
			for(size_t i=0;i<mask + 1;i++){
				memset(&tbl[i], 0, sizeof(sheena::Array<HashEntry, ways>));
			}
		}
		bool probe(const State& state, HashEntry* entry);
		void store(const State& state, int alpha, int beta, int value, Move bestmove, int depth);
		void store(const State& state, HashEntry entry);
		void prefetch(const State& state)const;
		void prefetch(const State& state, Move move)const;
		size_t hashfull()const{
			return hash_used * (1000 / ways) / (mask + 1);
		}
	};
}