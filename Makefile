NEXA_LANG ?= D:/Projects/Nexa-Lang
NEXAC    ?= NexaC
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

# Single-file installer: the setup stub (Nexa) with Nexium.exe, the icon, the
# docs and the MinGW runtime DLLs Nexium.exe needs appended by the packer.
# The stub and packer are intermediates, so make deletes them afterwards and
# only Nexium.exe and NexiumSetup.exe are left.
MINGW_BIN   ?= $(abspath $(dir $(shell $(CXX) -print-libgcc-file-name))../../../../bin)
THREAD_DLL   = $(firstword $(wildcard $(MINGW_BIN)/libmcfgthread-2.dll $(MINGW_BIN)/libwinpthread-1.dll))
RUNTIME_DLLS = $(wildcard $(MINGW_BIN)/libstdc++-6.dll $(MINGW_BIN)/libgcc_s_seh-1.dll) $(THREAD_DLL)

installer: NexiumSetup.exe

installer/setup-stub.exe: installer/setup.nxa
	$(NEXAC) installer/setup.nxa --no-console -o installer/setup-stub.exe

installer/pack.exe: installer/pack.nxa
	$(NEXAC) installer/pack.nxa -o installer/pack.exe

NexiumSetup.exe: installer/pack.exe installer/setup-stub.exe Nexium.exe res/nexium.ico README.md LICENSE
	installer/pack.exe NexiumSetup.exe installer/setup-stub.exe Nexium.exe res/nexium.ico README.md LICENSE $(RUNTIME_DLLS)

.INTERMEDIATE: installer/setup-stub.exe installer/pack.exe

.PHONY: all installer clean

clean:
	del /Q Nexium.exe NexiumSetup.exe installer\*.exe 2>nul || true
