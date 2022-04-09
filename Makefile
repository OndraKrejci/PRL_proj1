
CXXFLAGS = -std=c++17 -Wextra -Wall -Werror -pedantic -Wunused -Wshadow

.PHONY: run clean zip

oems: oems.cpp
	mpic++ -o oems oems.cpp $(CXXFLAGS)

numbers:
	dd if=/dev/random bs=1 count=8 of=numbers status=none

run: oems numbers
	mpirun -np 19 --oversubscribe ./oems numbers

clean:
	rm -f oems numbers xkrejc69.zip

zip: oems
	zip xkrejc69.zip oems.cpp test.sh xkrejc69.pdf
