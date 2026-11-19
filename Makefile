CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
BUILD = build

# Windows: native MSYS2/MinGW or a MinGW cross compiler on Linux. The compiler
# target is authoritative, with OS=Windows_NT as a fallback.
TARGET := $(shell $(CXX) -dumpmachine 2>/dev/null)
ifneq (,$(findstring mingw,$(TARGET))$(findstring windows,$(TARGET)))
  WINDOWS := 1
endif
ifeq ($(OS),Windows_NT)
  WINDOWS := 1
endif
WINDOWS ?= 0

ifeq ($(WINDOWS),1)
  EXE := .exe
  PLATFORM_SRC := src/platform_win32.cpp
  LDLIBS = -lgdi32 -luser32 -lwinmm
  TEST_LDLIBS = -lwinmm
  APP_LDFLAGS := -mwindows -static
  TEST_LDFLAGS := -static
  WINDRES ?= $(if $(TARGET),$(TARGET)-windres,windres)
  HAVE_WINDRES := $(shell command -v $(WINDRES) >/dev/null 2>&1 && echo 1)
  ifeq ($(HAVE_WINDRES),1)
    RES_OBJ := $(BUILD)/aetheria_res.o
  endif
else
  EXE :=
  PLATFORM_SRC := src/platform.cpp
  LDLIBS = -lX11 -ldl -pthread
  TEST_LDLIBS = -ldl -pthread
  APP_LDFLAGS :=
  TEST_LDFLAGS :=
endif

SRC = src/gfx.cpp src/player.cpp src/party.cpp src/world.cpp src/art.cpp src/battle.cpp \
      src/game.cpp src/game_render.cpp src/audio_synth.cpp src/audio.cpp \
      src/updater.cpp $(PLATFORM_SRC) src/settings.cpp src/savegame.cpp src/main.cpp
OBJ = $(addprefix $(BUILD)/,$(notdir $(SRC:.cpp=.o)))
TEST_SRC = tests/test_main.cpp src/gfx.cpp src/player.cpp src/party.cpp src/world.cpp \
           src/art.cpp src/battle.cpp src/game.cpp src/game_render.cpp src/audio_synth.cpp \
           src/audio.cpp src/updater.cpp src/settings.cpp src/savegame.cpp
TEST_OBJ = $(addprefix $(BUILD)/,$(notdir $(TEST_SRC:.cpp=.o)))
DEPS = $(sort $(OBJ:.o=.d) $(TEST_OBJ:.o=.d))

.PHONY: all test clean run shots

all: rpgcpp$(EXE)

rpgcpp$(EXE): $(OBJ) $(RES_OBJ)
	$(CXX) $(CXXFLAGS) $(APP_LDFLAGS) -o $@ $(OBJ) $(RES_OBJ) $(LDLIBS)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -Isrc -c $< -o $@

$(BUILD)/test_main.o: tests/test_main.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -MMD -MP -Isrc -c $< -o $@

$(BUILD)/aetheria_res.o: assets/aetheria.rc assets/aetheria.ico | $(BUILD)
	$(WINDRES) -Iassets -O coff -o $@ assets/aetheria.rc

$(BUILD):
	mkdir -p $(BUILD)

tests/run_tests$(EXE): $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $(TEST_LDFLAGS) -o $@ $(TEST_OBJ) $(TEST_LDLIBS)

test: tests/run_tests$(EXE)
	./tests/run_tests$(EXE)

run: rpgcpp$(EXE)
	./rpgcpp$(EXE)

shots: rpgcpp$(EXE)
	./rpgcpp$(EXE) --screenshot shots

clean:
	rm -rf $(BUILD) rpgcpp rpgcpp.exe tests/run_tests tests/run_tests.exe

-include $(DEPS)
