CFLAGS = -Wall -g -O3 -Wno-unused-variable 
CCP = g++

BIN_NAME = ../rpitxd 
SRC = main.cpp
LIBRPITX = librpitx/src/librpitx.a

$(BIN_NAME): $(SRC) $(LIBRPITX)
	$(CCP) $(CFLAGS) -o $@ $^ -Ilibrpitx/src


