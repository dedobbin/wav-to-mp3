C=g++
FILES=main.cpp

all: $(FILES)
	$(C) *.cpp -o converter -l:libmp3lame.a -lpthread