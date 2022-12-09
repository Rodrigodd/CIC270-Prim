CC = g++

GLLIBS = -lfreeglut -lglew32 -lopengl32
INCLUDES = -I ./libs/glew-2.2.0/include/ -I ./libs/freeglut/include/ -I ./libs/glm/
LIBS = -L ./libs/glew-2.2.0/bin/Release/x64/ -L ./libs/freeglut/bin/x64/

run: all
	prim.exe

all: prim.cpp
	$(CC) prim.cpp utils.cpp -o prim $(GLLIBS) $(INCLUDES) $(LIBS)

clean:
	rm -f prim
