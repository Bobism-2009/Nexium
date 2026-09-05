NEXA_LANG ?= D:/Projects/Nexa-Lang
CXX      ?= g++
CXXFLAGS  = -std=c++17 -O2 -DNDEBUG
INCLUDES  = -Isrc -Ires -Ithird_party/imgui -Ithird_party/imgui/backends -Ithird_party/imgui/misc/cpp -I"$(NEXA_LANG)/include"
LIBS      = -ld3d11 -ldxgi -ld3dcompiler -ldwmapi -lwinhttp -lole32 -luuid -lshell32 -lcomdlg32 -luser32 -lgdi32 -limm32
SRC       = src/main.cpp src/util.cpp src/highlight.cpp src/editor.cpp src/workspace.cpp src/terminal.cpp src/agent.cpp src/ide.cpp
RES       = res/nexium.res
IMGUI     = third_party/imgui/imgui.cpp \
            third_party/imgui/imgui_draw.cpp \
            third_party/imgui/imgui_tables.cpp \
            third_party/imgui/imgui_widgets.cpp \
            third_party/imgui/backends/imgui_impl_win32.cpp \
            third_party/imgui/backends/imgui_impl_dx11.cpp \
            third_party/imgui/misc/cpp/imgui_stdlib.cpp

all: Nexium.exe

res/nexium.res: res/nexium.rc res/nexium.ico res/resource.h
	windres -I res -O coff -o res/nexium.res res/nexium.rc

Nexium.exe: $(SRC) $(IMGUI) src/nexium.hpp $(RES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -mwindows -o Nexium.exe $(SRC) $(IMGUI) $(RES) $(LIBS)

clean:
	del /Q Nexium.exe 2>nul || true
