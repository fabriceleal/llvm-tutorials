FLAGS=`llvm-config --cxxflags --ldflags --libs core`

TARGET=main.o

.PHONY=clean

$(TARGET): main.cc
	g++ $? -o $@ $(FLAGS)

clean:
	rm -f $(TARGET)
