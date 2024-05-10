main.e: main.cpp
	g++ $< -o $@ -O3 -std=c++23 -g -Wall -Wextra

clean:
	rm -f *.e

clean-data:
	rm -f data/*.csv data/*.attacker data/*.defender data/*.png
