#include "hash_table.hpp"
#include "state.hpp"

namespace shogi{
	bool HashTable::probe(const State& state, HashEntry* entry){
		uint64_t key = state.key();
		for(int i=0;i<ways;i++){
			*entry = tbl[key & mask][i];
			if(entry->match(key))return true;
		}
		return false;
	}
	void HashTable::store_sub(HashEntry entry, uint64_t key){
		int new_entry_priority = priority(entry);
		int min_priority = new_entry_priority;
		int idx = -1;
		for(int i=0;i<ways;i++){
			HashEntry e = tbl[key & mask][i];
			int p = priority(e);
			if(e.match(key)){
				if(p <= new_entry_priority){
					if(p < LowerLimitDepth)hash_used++;
					tbl[key & mask][i] = entry;
				}
				return;
			}
			if(p <= min_priority){
				idx = i;
				min_priority = p;
			}
		}
		if(idx >= 0){
			if(min_priority < LowerLimitDepth)hash_used++;
			tbl[key & mask][idx] = entry;
		}
	}
	void HashTable::store(const State& state, int alpha, int beta, int value, Move bestmove, int depth){
		uint64_t key = state.key();
		HashEntry entry(key, alpha, beta, value, bestmove, depth, generation);
		assert(entry.match(key));
		assert(entry.move(state) == bestmove);
		store_sub(entry, key);
	}
	void HashTable::store(const State& state, HashEntry entry){
		entry.update_generation(generation);
		store_sub(entry, state.key());
	}
	HashTable::HashTable(){
		generation = 0;
		resize(256);
	}
	void HashTable::resize(int mb){
		while(mb & (mb - 1))mb &= mb - 1;
		mask = mb * (1024 * 1024 / (sizeof(HashEntry) * ways)) - 1;
		tbl.resize(mask + 1, sizeof(HashEntry) * ways);
		clear();
	}
	void HashTable::prefetch(const State& state)const{
		_mm_prefetch(reinterpret_cast<const char*>(&tbl[state.key() & mask]), _MM_HINT_T1);
	}
	void HashTable::prefetch(const State& state, Move move)const{
		_mm_prefetch(reinterpret_cast<const char*>(&tbl[state.next_position_key(move) & mask]), _MM_HINT_T1);
	}
}