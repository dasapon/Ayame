#pragma once
#include "move.hpp"
#include "position.hpp"
namespace shogi{
	class Record : std::vector<std::pair<int16_t, int16_t>>{
	public:
		SimplePosition sp;
		int result;
		using std::vector<std::pair<int16_t, int16_t>>::operator[];
		using std::vector<std::pair<int16_t, int16_t>>::begin;
		using std::vector<std::pair<int16_t, int16_t>>::end;
		using std::vector<std::pair<int16_t, int16_t>>::size;
		void clear(){
			resize(0);
			result = 0;
		}
		void operator=(const Record& rhs){
			std::vector<std::pair<int16_t, int16_t>>::operator=(rhs);
			sp = rhs.sp;
			result = rhs.result;
		}
		void push_back(Move move, int16_t v){
			std::vector<std::pair<int16_t, int16_t>>::push_back(std::make_pair(move.pack_15bit(), v));
		}
		void set_up(State& state, int ply)const;
	};
	struct Label{
		Move move;
		int eval, result;
	};
	class DataSet{
		std::vector<SimplePosition> sp;
		std::vector<int8_t> result;
		std::vector<int16_t> moves;
		std::vector<int16_t> values;
		std::vector<uint64_t> rand;
		std::vector<uint64_t> pos_count;
		int get_record_id(uint64_t idx, int s, int e)const;
	public:
		size_t size()const{
			return rand.size();
		}
		DataSet(){
			pos_count.push_back(0);
		}
		void add(const Record& record){
			int n = sp.size();
			pos_count.push_back(pos_count[n] + record.size());
			for(int i=0;i<record.size();i++){
				rand.push_back(rand.size());
			}
			sp.push_back(record.sp);
			result.push_back(record.result);
			for(int i=0;i<record.size();i++){
				moves.push_back(record[i].first);
				values.push_back(record[i].second);
			}
		}
		void shuffle(std::mt19937& mt){
			if(rand.size() > 0)std::shuffle(rand.begin(), rand.end(), mt);
		}
		Label get(State& state, uint64_t idx)const;
	};
	constexpr size_t learning_threads = 16;
	extern void selfplay(int depth, int nthreads, int ngames);
	extern void store_records(std::vector<Record>& records, const std::string& filename);
	extern void selfplay_startpos();
	extern void selfplay_startpos(DataSet& dataset, int n);
	extern DataSet load_records(const std::vector<std::string>& filenames);
	extern std::vector<Record> load_csa1(const std::string& filename);
	inline double eval2wr(int v, float scale = factorization_machine::Scale / EvalScale){
		return sheena::sigmoid(v / scale);
	}
	inline double log_e2wr(int v, float scale = factorization_machine::Scale / EvalScale){
		double x = v / scale;
		if(v > 0)return -log(1 + std::exp(-x));
		else return x - log(std::exp(x) + 1);
	}
	inline double cross_entropy_loss(float r, int v, float scale = factorization_machine::Scale / EvalScale){
		return -(r * log_e2wr(v, scale) + (1 - r) * log_e2wr(-v, scale));
	}
#ifdef LEARN
	extern void optimize_eval(const std::vector<std::string>& sl_files, const std::vector<std::string>& rl_files);
	class ExpAverage{
		double d;
		double cnt;
		double alpha;
	public:
		ExpAverage(double alpha = 0.001):d(0), cnt(0), alpha(alpha){}
		void update(double x){
			d = d * (1.0 - alpha) + x * alpha;
			cnt = cnt * (1.0 - alpha) + alpha;
		}
		double score()const{
			return d / cnt;
		}
	};
	class ClassificationStatistics{
		int cnt, accurate;
		double loss;
	public:
		ClassificationStatistics():cnt(0), accurate(0), loss(0){}
		double average_loss()const{
			return loss / cnt;
		}
		double accuracy()const{
			return double(accurate) / cnt;
		}
		int count()const{return cnt;}
		void update(double l, bool a){
			loss += l;
			if(a)accurate++;
			cnt++;
		}
		void operator+=(const ClassificationStatistics& rhs){
			cnt += rhs.cnt;
			accurate += rhs.accurate;
			loss += rhs.loss;
		}
	};
#endif
}
