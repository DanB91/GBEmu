_PLATFORM_SRC=sdl_main.cpp
_GAME_SRC=gbemu.cpp

GBEMU_VERSION:=$(shell cat ../version.txt)
PLATFORM_SRC=$(patsubst %.cpp, ../src/%.cpp, $(_PLATFORM_SRC))
GAME_SRC=$(patsubst %.cpp, ../src/%.cpp, $(_GAME_SRC))

PLATFORM_OBJ=$(patsubst %.cpp, build/%.o, $(_PLATFORM_SRC))
GAME_OBJ=$(patsubst %.cpp, build/%.o, $(_GAME_SRC))

PLATFORM_OBJ += build/imgui.o

VERSION=$(shell cat ../version.txt)

CPPFLAGS=-fno-exceptions -mmacosx-version-min=10.13 -Wall -Wextra -Wno-nullability-completeness -std=c++11 -fobjc-arc -Wconversion -isystem /usr/local/include -Wno-int-to-void-pointer-cast -I/Library/Frameworks/SDL2.framework/Headers
METALFLAGS=
CC=clang++

CTIME = ./ctime_mac

LDFLAGS += -framework CoreFoundation -framework AppKit -Fresources -framework SDL2 -framework Metal -lstdc++
PLATFORM_OBJ += build/mac.o

first:
	@echo 'Please use the build.sh script to build gbemu.'

profile: CPPFLAGS+=-O2 -DCO_PROFILE -DNDEBUG -g 
profile: ctime_begin generate_version build build/gbemu_release ctime_end

release: CPPFLAGS+=-O2 -DNDEBUG
release: ctime_begin  generate_version build build/gbemu_release ctime_end

debug: CPPFLAGS+=-DCO_DEBUG -DCO_PROFILE -g
debug: FPIC+=-fPIC
debug: ctime_begin generate_version build build/gbemu build/gbemu.so ctime_end

test: build/test

build/sdl_main.o: $(PLATFORM_SRC) $(PLATFORM_INC) FORCE 
	$(CC) -c -o $@ $< $(CPPFLAGS) 

build/gbemu.o: $(GAME_SRC) $(GAME_INC) FORCE 
	$(CC) -c -o $@ $< $(CPPFLAGS) $(FPIC)

build/imgui.o: ../src/3rdparty/imgui.cpp ../src/3rdparty/imgui.h ../src/3rdparty/imconfig.h ../src/3rdparty/imgui_draw.cpp
	$(CC) -c -o $@ $< $(CPPFLAGS)

build/imgui-fpic.o: ../src/3rdparty/imgui.cpp ../src/3rdparty/imgui.h ../src/3rdparty/imconfig.h ../src/3rdparty/imgui_draw.cpp
	$(CC) -c -o $@ $< $(CPPFLAGS) -fPIC

build/mac.o: ../src/mac.mm  FORCE
	$(CC) -c -o $@ $< $(CPPFLAGS)

build/default.metallib: ../src/shaders.metal
	xcrun --sdk macosx metal $(METALFLAGS) -o $@ $< 

build/gbemu: $(PLATFORM_OBJ) build/default.metallib FORCE 
	$(CC) $(PLATFORM_OBJ) $(LDFLAGS) -ldl  -o $@

build/gbemu.so: $(GAME_OBJ) build/imgui-fpic.o FORCE
	@touch build/soLock #so the game doesn't open the .so too early
	$(CC) $(GAME_OBJ) build/imgui-fpic.o -shared -fPIC -o $@
	@rm build/soLock

build/gbemu_release: $(PLATFORM_OBJ)  build/default.metallib
	$(CC) $(PLATFORM_OBJ) $(LDFLAGS) -ldl -o $@
	@mv $@ build/gbemu
build/test: ../src/tests/unitTests.cpp FORCE
	$(CC) -o $@ $< $(CPPFLAGS) -g
	build/test

generate_version: ../version.txt
	@echo "constexpr char GBEMU_VERSION[] = \"$(VERSION)\";" > ../src/version.h 
        
print-%  : ; @echo $* = $($*)

FORCE:

build:
	mkdir -p build

.PHONY: clean ctime_begin ctime_end generate_version

ctime_begin:
	$(CTIME) -begin buildtime.ctf
ctime_end:
	$(CTIME) -end buildtime.ctf
clean:
	rm -rf build
