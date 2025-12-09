CXX = g++
CXXFLAGS = -IC:/msys64/ucrt64/include/SDL2
LDFLAGS = -LC:/msys64/ucrt64/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image

TARGET = test1.exe
SRC = test1.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(SRC) -o $(TARGET) $(CXXFLAGS) $(LDFLAGS)

clean:
	del $(TARGET)
