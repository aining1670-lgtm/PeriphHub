# PeriphHub — MinGW-w64 build
#
#   export PATH="/c/msys64/usr/bin:/c/msys64/mingw64/bin:$PATH"
#   make            # 编译 build/PeriphHub.exe
#   make test       # 协议与桥的纯函数单测（不需要硬件）
#   make clean
#
CXX      ?= g++
WINDRES  ?= windres
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
            -mwindows -municode -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 \
            -Inative/src -Ithird_party/webview2/include
SRC      := $(wildcard native/src/*.cpp) $(wildcard native/src/drivers/*.cpp)
WEB      := $(wildcard web/*)
RES_OBJ  := build/resource.o
LIBS     := -lhid -lsetupapi -lcfgmgr32 -lshell32 -luser32 -lgdi32 -lole32 -loleaut32 -luuid -ladvapi32 -lcomctl32
OUT      := build/PeriphHub.exe

all: $(OUT)

$(RES_OBJ): native/src/resource.rc native/src/app.manifest assets/periphhub.ico $(WEB) third_party/webview2/x64/WebView2Loader.dll | build
	$(WINDRES) -I native/src native/src/resource.rc -O coff -o $@

$(OUT): $(SRC) $(WEB) $(RES_OBJ) | build
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ $(SRC) $(RES_OBJ) -o $@ $(LIBS)
	cp third_party/webview2/x64/WebView2Loader.dll build/
	rm -rf build/web && cp -r web build/web

build:
	mkdir -p build

TEST_CXX := $(CXX) -std=c++17 -O2 -Wall -Wextra -mconsole -DUNICODE -D_UNICODE -static -static-libgcc -static-libstdc++

test: | build
	$(TEST_CXX) tests/protocol_test.cpp native/src/protocol.cpp -Inative/src -o build/protocol_test.exe
	./build/protocol_test.exe
	$(TEST_CXX) tests/text_test.cpp native/src/text.cpp -Inative/src -o build/text_test.exe
	./build/text_test.exe
	$(TEST_CXX) tests/bridge_test.cpp native/src/bridge.cpp native/src/json.cpp -Inative/src -o build/bridge_test.exe
	./build/bridge_test.exe
	$(TEST_CXX) tests/macro_test.cpp native/src/protocol.cpp native/src/macro_store.cpp \
	    native/src/macro_json.cpp native/src/json.cpp -Inative/src -o build/macro_test.exe
	./build/macro_test.exe
	$(TEST_CXX) tests/command_json_test.cpp native/src/command_json.cpp native/src/bridge.cpp \
	    native/src/json.cpp -Inative/src -o build/command_json_test.exe
	./build/command_json_test.exe
	$(TEST_CXX) tests/peripheral_test.cpp native/src/peripheral_merge.cpp native/src/bridge.cpp \
	    native/src/json.cpp -Inative/src -o build/peripheral_test.exe
	./build/peripheral_test.exe
	$(TEST_CXX) tests/yjx_protocol_test.cpp native/src/yjx_protocol.cpp native/src/driver_router.cpp \
	    native/src/drivers/registry.cpp native/src/drivers/builtin_caps.cpp \
	    native/src/bridge.cpp native/src/json.cpp -Inative/src -o build/yjx_protocol_test.exe
	./build/yjx_protocol_test.exe
	$(TEST_CXX) tests/royuan_protocol_test.cpp native/src/royuan_protocol.cpp native/src/driver_router.cpp \
	    native/src/drivers/registry.cpp native/src/drivers/builtin_caps.cpp \
	    native/src/bridge.cpp native/src/json.cpp -Inative/src -o build/royuan_protocol_test.exe
	./build/royuan_protocol_test.exe
	$(TEST_CXX) tests/driver_registry_test.cpp native/src/drivers/registry.cpp \
	    native/src/drivers/builtin_caps.cpp native/src/driver_router.cpp \
	    native/src/bridge.cpp native/src/json.cpp -Inative/src -o build/driver_registry_test.exe
	./build/driver_registry_test.exe

dist: $(OUT)
	rm -rf dist/PeriphHub
	mkdir -p dist/PeriphHub
	cp build/PeriphHub.exe dist/PeriphHub/

probe-example: examples/battery-probe/battery_probe.cpp | build
	$(CXX) -std=c++17 -O2 -Wall -Wextra -mconsole -static -static-libgcc -static-libstdc++ \
	    examples/battery-probe/battery_probe.cpp -o build/battery-probe-example.exe

clean:
	rm -rf build

.PHONY: all test dist probe-example clean
