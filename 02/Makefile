FLAGS=-I/usr/lib/llvm-2.8/include  -DNDEBUG -D_GNU_SOURCE -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -L/usr/lib/llvm-2.8/lib  -lpthread -lffi -ldl -lm -lLLVMCore -lLLVMSupport -lLLVMSystem -L/usr/lib/llvm-2.8/libx

TARGET=main.o

.PHONY=clean

$(TARGET): main.cc
	g++ $? -o $@ $(FLAGS)

clean:
	rm -f $(TARGET)
