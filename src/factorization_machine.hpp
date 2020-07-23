#pragma once
#include <cstdlib>
#include "../sheena/sheena.hpp"

namespace factorization_machine{
	enum{
		LinearTerm,
	};
	enum{
		Scale = 1024,
	};
	template<size_t InteractDim>
	using Weight = sheena::VInt16<InteractDim + 1>;
	template<size_t InteractDim>
	using RawWeight = sheena::VFlt<InteractDim + 1>;

	template<size_t InteractDim>
	Weight<InteractDim> convert(const RawWeight<InteractDim>& raw_weight){
		RawWeight<InteractDim> retflt = raw_weight / std::sqrt(2.0);
		retflt[0] = 0;
		retflt[0] = raw_weight[0] - retflt.inner_product(retflt);
		retflt *= Scale;
		Weight<InteractDim> ret;
		for(int i=0;i<ret.size();i++){
			ret[i] = retflt[i];
		}
		return ret;
	}
	template<size_t InteractDim>
	RawWeight<InteractDim> convert(const Weight<InteractDim>& weight){
		RawWeight<InteractDim> ret;
		for(int i=0;i<ret.size();i++){
			ret[i] = weight[i];
		}
		ret *= 1.0 / Scale;
		float w_linear = ret[0];
		ret[0] = 0;
		ret[0] = w_linear + ret.inner_product(ret);
		ret *= std::sqrt(2.0);
		return ret;
	}
	template<size_t InteractDim>
	RawWeight<InteractDim> convert_interactions(const Weight<InteractDim>& w){
		RawWeight<InteractDim> ret;
		for(int i=0;i<ret.size();i++){
			ret[i] = w[i];
		}
		ret *= std::sqrt(2.0) / Scale;
		return ret;
	}
	template<size_t InteractDim>
	void update(RawWeight<InteractDim>& grad, const RawWeight<InteractDim>& raw_weight, const RawWeight<InteractDim>& sum_interactions, float g){
		RawWeight<InteractDim> diff = sum_interactions - raw_weight;
		diff[LinearTerm] = 1;
		grad.add_product(diff, g);
	}
	template<size_t Dim, size_t InteractDim>
	using RawWeightTable = sheena::Array<RawWeight<InteractDim>, Dim>;
	template<size_t Dim, size_t InteractDim>
	void initialize_table(RawWeightTable<Dim, InteractDim>& raw_weights, std::normal_distribution<double>& dist, std::mt19937& mt){
		for(int i=0;i<Dim;i++){
			for(int j=0;j<raw_weights[i].size();j++){
				raw_weights[i][j] = dist(mt);
			}
		}
	}
}