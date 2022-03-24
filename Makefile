
CXXFLAGS = -std=c++17 -Wextra -Wall -Werror -pedantic -Wunused -Wshadow

.PHONY: run clean

oems: oems.cpp
	mpic++ -o oems oems.cpp $(CXXFLAGS)

numbers:
	dd if=/dev/random bs=1 count=8 of=numbers

run: oems numbers
	mpirun -np 19 --oversubscribe ./oems numbers

clean:
	rm oems numbers
