TITLE      := Geometric Wars
VERSION    := 01.00
TITLE_ID   := GEOM00001
CONTENT_ID := IV0000-GEOM00001_00-GEOMETRICWARS000

TOOLCHAIN := $(OO_PS4_TOOLCHAIN)
COMMONDIR := openorbis_common
INTDIR    := build
CDIR      := linux

CCX := clang++
LD  := ld.lld

LIBS     := -lc -lkernel -lc++ -lSceSystemService -lSceVideoOut -lSceSysmodule
CXXFLAGS := --target=x86_64-pc-freebsd12-elf -O2 -fPIC -funwind-tables -c \
	-isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include \
	-isystem $(TOOLCHAIN)/include/c++/v1 -I$(COMMONDIR)
LDFLAGS  := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr \
	-L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

TOOLS       := $(TOOLCHAIN)/bin/$(CDIR)
GRAPHICS_URL := https://raw.githubusercontent.com/OpenOrbis/OpenOrbis-PS4-Toolchain/v0.5.4/samples/_common

.PHONY: all helpers clean check-toolchain

all: check-toolchain $(CONTENT_ID).pkg

check-toolchain:
	@test -n "$(TOOLCHAIN)" || (echo "Defina OO_PS4_TOOLCHAIN." && exit 1)
	@test -x "$(TOOLS)/create-fself"
	@test -x "$(TOOLS)/create-gp4"
	@test -x "$(TOOLS)/PkgTool.Core"

helpers: $(COMMONDIR)/graphics.h $(COMMONDIR)/graphics.cpp $(COMMONDIR)/log.h

$(COMMONDIR):
	mkdir -p $@

$(COMMONDIR)/graphics.h: | $(COMMONDIR)
	curl -fL --retry 3 "$(GRAPHICS_URL)/graphics.h" -o $@

$(COMMONDIR)/graphics.cpp: | $(COMMONDIR)
	curl -fL --retry 3 "$(GRAPHICS_URL)/graphics.cpp" -o $@

$(COMMONDIR)/log.h: | $(COMMONDIR)
	curl -fL --retry 3 "$(GRAPHICS_URL)/log.h" -o $@

$(INTDIR):
	mkdir -p $@

$(INTDIR)/main.o: src/main.cpp $(COMMONDIR)/graphics.h | $(INTDIR)
	$(CCX) $(CXXFLAGS) -o $@ $<

$(INTDIR)/graphics.o: $(COMMONDIR)/graphics.cpp $(COMMONDIR)/graphics.h $(COMMONDIR)/log.h | $(INTDIR)
	$(CCX) $(CXXFLAGS) -o $@ $<

$(INTDIR)/GeometricWars.elf: $(INTDIR)/main.o $(INTDIR)/graphics.o
	$(LD) $^ -o $@ $(LDFLAGS)

eboot.bin: $(INTDIR)/GeometricWars.elf
	$(TOOLS)/create-fself -in=$< -out=$(INTDIR)/GeometricWars.oelf \
		--eboot "$@" --paid 0x3800000000000011

sce_sys/about/right.sprx:
	@mkdir -p sce_sys/about
	@source_file="$$(find "$(TOOLCHAIN)/samples" -type f -path '*/sce_sys/about/right.sprx' -print -quit)"; \
	if [ -z "$$source_file" ]; then \
		echo "right.sprx não foi encontrado nos samples do OpenOrbis."; exit 1; \
	fi; \
	cp "$$source_file" "$@"

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

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png
	$(TOOLS)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLS)/PkgTool.Core pkg_build $< .
	@test -s $@

clean:
	rm -rf $(INTDIR) $(COMMONDIR) eboot.bin pkg.gp4 sce_sys/param.sfo \
		$(CONTENT_ID).pkg

