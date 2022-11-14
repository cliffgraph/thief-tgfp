# Makefile

TARGET = thief-tgfp

include targetfiles.txt

CC = gcc
CXX = g++
CFLAGS = -std=c99 -Wall -O2 $(INCPATH)
CFLAGS += -D_UNICODE -DUNICODE
CFLAGS += -DUSE_UDPL_DLL
CXXFLAGS = -std=c++11 -Wall -O2 $(INCPATH)
CXXFLAGS += -D_UNICODE -DUNICODE
CXXFLAGS += -DNDEBUG
CXXFLAGS += -DUSE_RAMSXMUSE

LDFLAGS = -pthread -lrt -lz -lwiringPi
LDFLAGS += -Wl,-Map=${TARGET}.map

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET) $(TARGET).map

.PHONY: ver
ver:
	$(CXX) --version

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $(TARGET)

