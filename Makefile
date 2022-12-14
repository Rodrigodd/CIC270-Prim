CC = g++

ifeq ($(OS), Windows_NT)
	GLLIBS = -lfreeglut -lglew32 -lopengl32
	INCLUDES = -I ./libs/glew-2.2.0/include/ -I ./libs/freeglut/include/ -I ./libs/glm/
	LIBS = -L ./libs/glew-2.2.0/bin/Release/x64/ -L ./libs/freeglut/bin/x64/
	OUT = prim.exe
else
	GLLIBS = -lglut -lGLEW -lGL
	INCLUDES = 
	LIBS = 
	OUT = ./prim
endif

run: all
	$(OUT)

all: prim.cpp
	$(CC) prim.cpp utils.cpp -o prim $(GLLIBS) $(INCLUDES) $(LIBS)

clean:
	rm -f prim
