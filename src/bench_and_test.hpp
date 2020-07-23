#pragma once

namespace shogi{
	extern void genmove_bench();
	extern void perft();
	extern void search_bench(int threads);
	extern void test_mate1ply();
}