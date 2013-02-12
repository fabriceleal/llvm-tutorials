FLAGS=`llvm-config --cxxflags --ldflags --libs core jit native`

TARGET=main

.PHONY=clean all

all: main

#ast.o: ast.cc llvm_stuff.cc
#	g++ $(FLAGS) $? -o $@

#main.o: main.cc
#	g++ $(FLAGS) $? -o $@

$(TARGET): main.cc ast.cc gen.cc
	g++ -g -O3 -rdynamic $? $(FLAGS) -o $@

clean:
	rm -f *.o $(TARGET)
