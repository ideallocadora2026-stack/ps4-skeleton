TITLE      := Geometric Wars
VERSION    := 1.17
TITLE_ID   := GEOM00001
CONTENT_ID := IV0000-GEOM00001_00-GEOMETRICWARS000

TOOLCHAIN := $(OO_PS4_TOOLCHAIN)
SDL_ROOT  ?= $(TOOLCHAIN)
INTDIR    := build
CDIR      := linux

CCX := clang++
LD  := ld.lld

CPPFILES := $(wildcard src/*.cpp)
OBJS     := $(patsubst src/%.cpp,$(INTDIR)/%.o,$(CPPFILES))

LIBS := -lSDL2 -lc -lm -lkernel -lc++ \
	-lSceUserService -lSceVideoOut -lSceAudioOut -lScePad \
	-lSceSysmodule -lSceSystemService

CXXFLAGS := --target=x86_64-pc-freebsd12-elf -std=c++14 -D_GNU_SOURCE -O2 -fPIC \
	-funwind-tables -c -isysroot $(TOOLCHAIN) \
	-isystem $(TOOLCHAIN)/include -isystem $(TOOLCHAIN)/include/c++/v1 \
	-I$(SDL_ROOT)/include -Isrc

LDFLAGS := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x \
	--eh-frame-hdr -L$(SDL_ROOT)/lib -L$(TOOLCHAIN)/lib \
	$(LIBS) $(TOOLCHAIN)/lib/crt1.o

TOOLS := $(TOOLCHAIN)/bin/$(CDIR)

PKG_FILES := eboot.bin \
	sce_sys/about/right.sprx \
	sce_sys/param.sfo \
	sce_sys/icon0.png \
	sce_sys/pic0.png \
	sce_sys/pic1.png \
	sce_module/libc.prx \
	sce_module/libSceFios2.prx

.PHONY: all clean check-toolchain check-sdl

all: check-toolchain check-sdl $(CONTENT_ID).pkg

check-toolchain:
	@test -n "$(TOOLCHAIN)" || (echo "Defina OO_PS4_TOOLCHAIN." && exit 1)
	@test -x "$(TOOLS)/create-fself"
	@test -x "$(TOOLS)/create-gp4"
	@test -x "$(TOOLS)/PkgTool.Core"
	@test -s "$(TOOLCHAIN)/lib/crt1.o"

check-sdl:
	@test -s "$(SDL_ROOT)/include/SDL2/SDL.h" || (echo "SDL2 headers nao encontrados em $(SDL_ROOT)." && exit 1)
	@test -s "$(SDL_ROOT)/lib/libSDL2.a" || (echo "libSDL2.a nao encontrada em $(SDL_ROOT)." && exit 1)

$(INTDIR):
	mkdir -p $@

$(INTDIR)/%.o: src/%.cpp | $(INTDIR)
	$(CCX) $(CXXFLAGS) -o $@ $<

$(INTDIR)/GeometricWars.elf: $(OBJS)
	$(LD) $^ -o $@ $(LDFLAGS)

eboot.bin: $(INTDIR)/GeometricWars.elf
	$(TOOLS)/create-fself -in=$< -out=$(INTDIR)/GeometricWars.oelf \
		--eboot "$@" --paid 0x3800000000000011

sce_sys/param.sfo: Makefile
	mkdir -p sce_sys
	rm -f $@
	$(TOOLS)/PkgTool.Core sfo_new $@
	$(TOOLS)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLS)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLS)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLS)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLS)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLS)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

pkg.gp4: $(PKG_FILES)
	$(TOOLS)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLS)/PkgTool.Core pkg_build $< .
	@test -s $@

clean:
	rm -rf $(INTDIR) eboot.bin pkg.gp4 sce_sys/param.sfo $(CONTENT_ID).pkg output
