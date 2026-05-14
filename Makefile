CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS := $(shell pkg-config --libs gtk+-3.0 2>/dev/null)

all: server client client_gui

server: server.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

client: client.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

client_gui: client_gui.cpp
	$(CXX) $(CXXFLAGS) $(GTK_CFLAGS) $< -o $@ $(GTK_LIBS)

clean:
	rm -f server client client_gui
