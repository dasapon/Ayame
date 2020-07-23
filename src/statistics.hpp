#pragma once

#include <limits>
#include <cstdint>
#include <cmath>

class Statistics{
	double min_, max_, sum, sq_sum;
	uint64_t cnt;
public:
	Statistics():min_(std::numeric_limits<double>::max()), max_(std::numeric_limits<double>::min()), sum(0), sq_sum(0), cnt(0){}
	void operator+=(const Statistics& rhs){
		min_ = std::min(rhs.min_, min_);
		max_ = std::max(rhs.max_, max_);
		sum += rhs.sum;
		sq_sum += rhs.sq_sum;
		cnt += rhs.cnt;
	}
	double min()const{
		return min_;
	}
	double max()const{
		return max_;
	}
	double mean()const{
		return sum / cnt;
	}
	double variance()const{
		double m = mean();
		return sq_sum / cnt - m * m;
	}
	void update(double x){
		min_ = std::min(x, min_);
		max_ = std::max(x, max_);
		sum += x;
		sq_sum += x * x;
		cnt++;
	}
};