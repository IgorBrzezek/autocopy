# Makefile for autocopy
# Author: Igor Brzezek
# Version: 0.0.4

CC = gcc
CFLAGS = -Wall -O2 -static
LIBS = -luser32 -lgdi32
TARGET = autocopy.exe
SRC = autocopy.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	if exist $(TARGET) del /Q $(TARGET)
