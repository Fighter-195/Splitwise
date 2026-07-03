# Splitwise (C++) — build the CLI and the Dear ImGui GUI.
#
#   make            build the CLI  -> ./splitwise (splitwise.exe on Windows)
#   make run        build and run the CLI
#   make gui        build the GUI  -> ./splitwise-gui.exe   (Windows only)
#   make run-gui    build and run the GUI
#   make clean      remove build artifacts

CXX      ?= g++
CXXFLAGS ?= -std=c++14 -Wall -Wextra -O2

# ---- shared engine + storage ------------------------------------------------
CORE_SRCS := $(wildcard engine/*.cpp) $(wildcard storage/*.cpp)

# ---- CLI front-end ----------------------------------------------------------
CLI_SRCS := $(CORE_SRCS) cli/main.cpp
CLI_BIN  := splitwise

$(CLI_BIN): $(CLI_SRCS)
	$(CXX) $(CXXFLAGS) -o $(CLI_BIN) $(CLI_SRCS)

# ---- GUI front-end (Win32 + Direct3D 9 + Dear ImGui) ------------------------
# ImGui is vendored under gui/imgui/. DX9 ships with Windows + MinGW, so no
# external libraries need to be installed. Sources are compiled to object files
# under build/ so the heavy ImGui translation units only recompile when changed.
GUI_BIN  := splitwise-gui
GUI_LIBS := -ld3d9 -lgdi32 -limm32
# Notes on the flags:
#   IMGUI_IMPL_WIN32_DISABLE_GAMEPAD  bare MinGW.org lacks <xinput.h>
#   -include gui/win32_compat.h       supplies a few symbols the old MinGW.org
#                                     headers omit (TME_NONCLIENT,
#                                     GET_XBUTTON_WPARAM, RTL_OSVERSIONINFOEXW);
#                                     a no-op on modern mingw-w64.
GUI_DEFS := -DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD -include gui/win32_compat.h
GUI_CXXFLAGS := -std=c++14 -O2 -Igui $(GUI_DEFS)

BUILD := build
GUI_OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(CORE_SRCS) gui/main_gui.cpp $(wildcard gui/imgui/*.cpp))

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(GUI_CXXFLAGS) -c $< -o $@

gui: $(GUI_BIN)
$(GUI_BIN): $(GUI_OBJS)
	$(CXX) $(GUI_CXXFLAGS) -o $(GUI_BIN) $(GUI_OBJS) $(GUI_LIBS) -mwindows

.PHONY: gui run run-gui clean
run: $(CLI_BIN)
	./$(CLI_BIN)

run-gui: gui
	./$(GUI_BIN)

clean:
	rm -f $(CLI_BIN) $(CLI_BIN).exe $(GUI_BIN) $(GUI_BIN).exe *.o
	rm -rf $(BUILD)
