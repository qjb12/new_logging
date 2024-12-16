CXX = g++
CXXFLAGS = -std=c++11 -Wall -pthread -g -O0

TARGET = main
SRCS = main.cpp log.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

debug: CXXFLAGS += -DDEBUG
debug: clean all

clean:
	rm -f $(TARGET) $(OBJS)
