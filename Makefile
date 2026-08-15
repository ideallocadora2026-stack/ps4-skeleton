TITLE := Geometric Wars
TITLE_ID := GEOM00001
CONTENT_ID := IV0000-GEOM00001_00-GEOMETRICWARS000

VERSION := 01.00
APP_VER := 01.00

LIBS := -lc -lkernel -lc++ -lSceSystemService

SRCS := src/main.cpp
OBJS := $(SRCS:.cpp=.o)

# O PKG final é criado pelo GitHub Actions em:
# .github/workflows/build.yml
#
# Não reutilize Boyceta-ps4/sce_sys/param.sfo.
# O workflow cria um param.sfo novo para cada build.
