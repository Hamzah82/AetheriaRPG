CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
LDLIBS = -lX11
BUILD = build
SRC = src/gfx.cpp src/player.cpp src/world.cpp src/art.cpp src/battle.cpp \
      src/game.cpp src/game_render.cpp src/platform.cpp src/main.cpp
OBJ = $(addprefix $(BUILD)/,$(notdir $(SRC:.cpp=.o)))
DEPS = $(OBJ:.o=.d)
TEST_SRC = tests/test_main.cpp src/gfx.cpp src/player.cpp src/world.cpp src/art.cpp \
           src/battle.cpp src/game.cpp src/game_render.cpp
TEST_OBJ = $(addprefix $(BUILD)/,$(notdir $(TEST_SRC:.cpp=.o)))

.PHONY: all test clean run shots

all: rpgcpp

rpgcpp: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LDLIBS)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -Isrc -c $< -o $@

$(BUILD)/test_main.o: tests/test_main.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -Isrc -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

tests/run_tests: $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_OBJ)

test: tests/run_tests
	./tests/run_tests

run: rpgcpp
	./rpgcpp

shots: rpgcpp
	./rpgcpp --screenshot shots

clean:
	rm -rf $(BUILD) rpgcpp tests/run_tests

-include $(DEPS)
