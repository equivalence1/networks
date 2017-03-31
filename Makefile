srcdir = src
bindir = bin
objdir = obj
incdir = src/include

CXX = g++
CXXFLAGS = -Wall -Wpedantic -std=c++11

client_sources = client.cpp tcp_socket.cpp common.cpp protocol.cpp
client_objects = $(client_sources:.cpp=.o)

server_sources = server.cpp tcp_socket.cpp common.cpp protocol.cpp
server_objects = $(server_sources:.cpp=.o)

all: $(bindir) $(objdir) client server

debug: CXXFLAGS += -DDEBUG
debug: all

$(bindir):
	mkdir -p $(bindir)

$(objdir):
	mkdir -p $(objdir)

client: $(client_objects)
	$(CXX) $(CXXFLAGS) $(addprefix $(objdir)/, $(client_objects)) -o $(bindir)/client -lpthread

server: $(server_objects)
	$(CXX) $(CXXFLAGS) $(addprefix $(objdir)/, $(server_objects)) -o $(bindir)/server -lpthread

%.o: $(srcdir)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $(objdir)/$@ -I./src/include


.PHONY: clean
clean:
	rm -rf $(bindir)
	rm -rf $(objdir)
