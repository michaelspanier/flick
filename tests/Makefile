## Logic tests for Flick. They need no Haiku: any C++ compiler will do.
##
##   make -C tests

CXX ?= c++
CXXFLAGS ?= -O1 -g
CXXFLAGS += -Wall -Wextra -I../src

TESTS = FlickLogicTest FlickControlsLogicTest

all: $(TESTS)
	@for test in $(TESTS); do ./$$test || exit 1; done

FlickLogicTest: FlickLogicTest.cpp ../src/FlickLogic.cpp ../src/FlickLogic.h
	$(CXX) $(CXXFLAGS) -o $@ FlickLogicTest.cpp ../src/FlickLogic.cpp

FlickControlsLogicTest: FlickControlsLogicTest.cpp ../src/FlickControlsLogic.h
	$(CXX) $(CXXFLAGS) -o $@ FlickControlsLogicTest.cpp

clean:
	rm -f $(TESTS)

.PHONY: all clean
