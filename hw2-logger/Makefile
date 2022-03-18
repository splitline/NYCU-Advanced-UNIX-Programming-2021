CC=g++
CFLAGS=-shared -fPIC -fconcepts -fpermissive -Wno-format -std=c++17 
TARGET=logger.so
SRC_DIR=src

all: $(TARGET)

$(TARGET): $(wildcard $(SRC_DIR)/*.cc) $(wildcard $(SRC_DIR)/*.h)
	$(CC) -o $(TARGET) $(CFLAGS) $^ -ldl

zip:
	rm 309551021_hw2.zip
	mkdir 309551021_hw2
	cp -r -t 309551021_hw2 logger Makefile src/
	zip -r 309551021_hw2.zip 309551021_hw2/
	rm -rf 309551021_hw2/