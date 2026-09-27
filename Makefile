# Nexium is written in Nexa (nexium/*.nxa) and drawn with std/ui. NexaC turns
# it into one C++ file and compiles that; `make` also keeps that C++ in
# transpiled/nexium.cpp so the generated source can be read, diffed and built
# by hand. Needs NexaC 0.1.15 or newer (std/ui); point NEXAC at it if the one
# on PATH is older.
NEXAC ?= NexaC
SRC    = $(wildcard nexium/*.nxa) nexium/native.hpp
RES_O  = res/nexium_res.o

all: Nexium.exe transpiled/nexium.cpp

$(RES_O): res/nexium.rc res/nexium.ico res/resource.h
	windres -I res -O coff -o $@ res/nexium.rc

Nexium.exe: $(SRC) $(RES_O)
	$(NEXAC) nexium/main.nxa --no-console -p --link $(abspath $(RES_O)) -o Nexium.exe

# The generated C++, with the header include made relative so the file builds
# from any checkout:  clang++ -std=c++17 -O2 transpiled/nexium.cpp ...
transpiled/nexium.cpp: $(SRC)
	mkdir -p transpiled
	$(NEXAC) nexium/main.nxa -p --source $@
	sed -i 's|^#include ".*/nexium/native.hpp"|#include "../nexium/native.hpp"|' $@

transpiled: transpiled/nexium.cpp

# Single-file installer: the setup stub (Nexa) with Nexium.exe, the icon and
# the docs appended by the packer. Nexium.exe links the C++ runtime
# statically, so there are no DLLs to ship. The stub and packer are
# intermediates, so make deletes them afterwards.
installer: NexiumSetup.exe

installer/setup-stub.exe: installer/setup.nxa
	$(NEXAC) installer/setup.nxa --no-console -o installer/setup-stub.exe

installer/pack.exe: installer/pack.nxa
	$(NEXAC) installer/pack.nxa -o installer/pack.exe

NexiumSetup.exe: installer/pack.exe installer/setup-stub.exe Nexium.exe res/nexium.ico README.md LICENSE
	installer/pack.exe NexiumSetup.exe installer/setup-stub.exe Nexium.exe res/nexium.ico README.md LICENSE

.INTERMEDIATE: installer/setup-stub.exe installer/pack.exe

.PHONY: all transpiled installer clean

clean:
	rm -f Nexium.exe NexiumSetup.exe installer/*.exe $(RES_O)
