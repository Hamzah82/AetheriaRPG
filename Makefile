CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
LDLIBS = -lX11 -ldl -pthread
TEST_LDLIBS = -ldl -pthread
BUILD = build
SRC = src/gfx.cpp src/player.cpp src/world.cpp src/art.cpp src/battle.cpp \
      src/game.cpp src/game_render.cpp src/audio_synth.cpp src/audio.cpp \
      src/updater.cpp src/platform.cpp src/settings.cpp src/savegame.cpp src/main.cpp
OBJ = $(addprefix $(BUILD)/,$(notdir $(SRC:.cpp=.o)))
TEST_SRC = tests/test_main.cpp src/gfx.cpp src/player.cpp src/world.cpp src/art.cpp \
           src/battle.cpp src/game.cpp src/game_render.cpp src/audio_synth.cpp \
           src/audio.cpp src/updater.cpp src/settings.cpp src/savegame.cpp
TEST_OBJ = $(addprefix $(BUILD)/,$(notdir $(TEST_SRC:.cpp=.o)))
DEPS = $(sort $(OBJ:.o=.d) $(TEST_OBJ:.o=.d))

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
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_OBJ) $(TEST_LDLIBS)

test: tests/run_tests
	./tests/run_tests

run: rpgcpp
	./rpgcpp

shots: rpgcpp
	./rpgcpp --screenshot shots

clean:
	rm -rf $(BUILD) rpgcpp tests/run_tests

-include $(DEPS)
