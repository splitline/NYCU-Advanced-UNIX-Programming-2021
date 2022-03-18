all:
	g++ -std=c++17 main.cc sdb.cc -lcapstone -o hw4

zip:
	rm -rf 309551021_hw4
	rm -f 309551021_hw4.zip
	mkdir 309551021_hw4
	cp -t 309551021_hw4 main.cc sdb.cc sdb.hpp utils.hpp Makefile
	zip -r 309551021_hw4.zip 309551021_hw4/
	rm -rf 309551021_hw4/%    