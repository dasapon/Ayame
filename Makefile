SRCS = $(shell ls src/*.cpp)
OBJS = $(SRCS:src/%.cpp=obj/release/%.o)
LOBJS = $(SRCS:src/%.cpp=obj/learn/%.o)
POBJS= $(SRCS:src/%.cpp=obj/profile/%.o)
DPS = $(SRCS:src/%.cpp=obj/release/%.d) $(SRCS:src/%.cpp=obj/learn/%.d) $(SRCS:src/%.cpp=obj/profile/%.d)
BINARIES = bin/iris bin/learn bin/profile

OPTIONS = -std=c++17 -fopenmp -Wall -march=native -O3 -DNDEBUG
LIBS = -lomp -lpthread
CXX = clang++

all:$(BINARIES)

-include $(DPS)

.PHONY: iris learn clean

iris:bin/iris
learn:bin/learn
profile:bin/profile
	rm -f bin/gmon.out
	cd bin;./profile bench 1 > prof.txt
	cd bin;gprof profile >> prof.txt
	rm -f bin/gmon.out

bin/iris:$(OBJS)
	$(CXX) $(OPTIONS) -flto -o $@ $(OBJS) $(LIBS)

bin/learn:$(LOBJS)
	$(CXX) $(OPTIONS) -flto -DLEARN -o $@ $(LOBJS) $(LIBS)

bin/profile:$(POBJS)
	$(CXX) $(OPTIONS) -pg -o $@ $(POBJS) $(LIBS)

obj/release/%.o:src/%.cpp
	if [ ! -d obj/release ]; then mkdir -p obj/release; fi
	$(CXX) -MMD $(OPTIONS) -o $@ -c $<
obj/learn/%.o:src/%.cpp
	if [ ! -d obj/learn ]; then mkdir -p obj/learn; fi
	clang++ -MMD $(OPTIONS) -DLEARN -o $@ -c $<
obj/profile/%.o:src/%.cpp
	if [ ! -d obj/profile ]; then mkdir -p obj/profile; fi
	$(CXX) -MMD $(OPTIONS) -pg -o $@ -c $<

clean:
	rm -rf obj
	rm -f $(BINARIES)
