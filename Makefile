CC = gcc

SANITIZE = -fsanitize=address -fsanitize=undefined
MACOS_CFLAGS  = -F/Library/Frameworks
MACOS_LDFLAGS = -F/Library/Frameworks -framework SDL2 -framework SDL2_image -framework OpenGL -lGLEW 

INCLUDES = -Iglla -I$(CURDIR)
LDFLAGS	 = $(SDL) -Llua-5.4.4/src -llua $(MACOS_LDFLAGS)
LIBFLAGS = $(SDL) -Llua-5.4.4/src -llua $(MACOS_LDFLAGS) -dynamiclib
SHADERS	 = $(wildcard shaders/*.vs shaders/*.fs shaders/*.gs)
# LUA      = $(patsubst %.c,%.o,$(wildcard lua53/*.c)) #Should use this for everything except the experiments folder
EXE 	 = tu
LIB      = libtu.so

#Module includes append to OBJECTS and define other custom rules.
include root.mk
include datastructures/datastructures.mk
include math/math.mk
include models/models.mk
include entity/entity.mk
include glsw/glsw.mk
include space/space.mk
include experiments/experiments.mk
include trackball/trackball.mk
include meter/meter.mk
include components/components.mk
include systems/systems.mk
include luaengine/luaengine.mk
include test/test.mk

MAIN_OBJ = main.o
LIBTU_OBJ = libtu.o

CFLAGS = -Wall -c -std=c11 -g -pthread $(MACOS_CFLAGS) $(INCLUDES) #-march=native -O3
all: $(OBJECTS) $(MAIN_OBJ) $(LIBTU_OBJ) #ceffectpp/ceffectpp
	$(CC) $(LDFLAGS) $(OBJECTS) $(MAIN_OBJ) -o $(EXE)
	$(CC) $(LIBFLAGS) $(OBJECTS) $(LIBTU_OBJ) -o $(LIB)


test_main.o: $(wildcard test/*.test.c)

test: $(OBJECTS) test_main.o Makefile
	./$(EXE) test

.DUMMY: all clean test

#I want "all" to be the default rule, so any module-specific build rules should be specified in a separate makefile.
include models/Makefile

.depend:
	gcc -MM $(INCLUDES) $(CFLAGS) $(patsubst %.o,%.c,$(OBJECTS)) > .depend #Generate dependencies from all .c files, searching recursively.

effects.c: $(SHADERS) effects.h lua-5.4.4 ceffectpp/ceffectpp
	ceffectpp/ceffectpp -c $(SHADERS) > effects.c

effects.h: $(SHADERS) lua-5.4.4 ceffectpp/ceffectpp
	ceffectpp/ceffectpp -h $(SHADERS) > effects.h

ceffectpp/ceffectpp:
	cd ceffectpp; make

open-simplex-noise.o:
	cd open-simplex-noise-in-c; make

lua-5.4.4:
	cd lua-5.4.4; make macosx

clean:
	rm $(OBJECTS)
	rm $(EXE)
	rm .depend
	cd ceffectpp; make clean
	cd open-simplex-noise-in-c; make clean

include .depend
