TITLE      := Boyceta PS4
TITLE_ID   := BOYC00001
VERSION    := 01.00
APP_VER    := 01.00

LIBS       := -lSceSystemService_stub_weak -lSceUserService_stub_weak
SRCS       := src/main.cpp
OBJS       := $(SRCS:.cpp=.o)

include $(OPENORBIS)/usr/lib/OpenOrbis.mk
