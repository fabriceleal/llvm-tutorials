FLAGS=`llvm-config --cxxflags --ldflags --libs core`

TARGET=main.o

.PHONY=clean

$(TARGET): main.cc ast.cc
	g++ $? -o $@ $(FLAGS)

clean:
	rm -f $(TARGET)
