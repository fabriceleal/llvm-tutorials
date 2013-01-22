FLAGS=`llvm-config --cxxflags --ldflags --libs core`

TARGET=main

.PHONY=clean all

all: main

ast.o: ast.cc
	g++ -c $?

main.o: main.cc
	g++ -c $?

$(TARGET): main.o ast.o
	g++ $? -o $@ $(FLAGS)

clean:
	rm -f *.o $(TARGET)
