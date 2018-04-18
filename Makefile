# Makefile for Basler pylon sample program
.PHONY: all clean

# The program to build
NAME := NiteLite_115
MULTI := NiteLite_115_multi
BASLERCTRL := baslerctrl
LSBASLER := lsbaslers
HANDLEUSB := handleusb

# Installation directories for pylon
PYLON_ROOT ?= /opt/pylon5

# Build tools and flags
LD         := $(CXX)
CPPFLAGS   := $(shell $(PYLON_ROOT)/bin/pylon-config --cflags) -std=c++11 
CXXFLAGS   := #e.g., CXXFLAGS=-g -O0 for debugging
LDFLAGS    := $(shell $(PYLON_ROOT)/bin/pylon-config --libs-rpath)
LDLIBS     := $(shell $(PYLON_ROOT)/bin/pylon-config --libs) -lpthread

# Rules for building
all: $(NAME) $(MULTI) $(BASLERCTRL) $(LSBASLER) $(HANDLEUSB) 

$(NAME): $(NAME).o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(MULTI): $(MULTI).o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BASLERCTRL): $(BASLERCTRL).o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(LSBASLER): $(LSBASLER).o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(HANDLEUSB): $(HANDLEUSB).o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.cpp.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	$(RM) $(NAME).o $(NAME)
