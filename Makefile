C=g++
FILES=main.cpp
INCLUDE_DIRS = -I/usr/include/lame

all: $(FILES)
	$(C) $(INCLUDE_DIRS)  $(FILES) -o converter -l:libmp3lame.a -lpthread