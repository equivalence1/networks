srcdir = src
bindir = bin
objdir = obj
incdir = src/include

CXX = g++
CXXFLAGS = -Wall -Wpedantic -std=c++11

client_sources = client.cpp tcp_socket.cpp au_stream_socket.cpp common.cpp protocol.cpp opcode.cpp net_utils.cpp
client32_objects = $(client_sources:.cpp=.o_32)
client64_objects = $(client_sources:.cpp=.o_64)

server_sources = server.cpp tcp_socket.cpp au_stream_socket.cpp common.cpp protocol.cpp calc.cpp opcode.cpp net_utils.cpp
server_objects = $(server_sources:.cpp=.o_64)

test_sources = tcp_socket.cpp au_stream_socket.cpp test.cpp common.cpp net_utils.cpp
test_objects = $(test_sources:.cpp=.o_64)

all: $(bindir) $(objdir) client32 client64 server test

debug: CXXFLAGS += -DDEBUG
debug: all

$(bindir):
	mkdir -p $(bindir)

$(objdir):
	mkdir -p $(objdir)

test: $(test_objects)
	$(CXX) $(CXXFLAGS) -m64 $(addprefix $(objdir)/, $(test_objects)) -o $(bindir)/test -lpthread

client32: $(client32_objects)
	$(CXX) $(CXXFLAGS) -m32 $(addprefix $(objdir)/, $(client32_objects)) -o $(bindir)/client32 -lpthread

client64: $(client64_objects)
	$(CXX) $(CXXFLAGS) -m64 $(addprefix $(objdir)/, $(client64_objects)) -o $(bindir)/client64 -lpthread

server: $(server_objects)
	$(CXX) $(CXXFLAGS) $(addprefix $(objdir)/, $(server_objects)) -o $(bindir)/server -lpthread

%.o_32: $(srcdir)/%.cpp
	$(CXX) $(CXXFLAGS) -m32 -c $< -o $(objdir)/$@ -I./src/include

%.o_64: $(srcdir)/%.cpp
	$(CXX) $(CXXFLAGS) -m64 -c $< -o $(objdir)/$@ -I./src/include

.PHONY: clean
clean:
	rm -rf $(bindir)
	rm -rf $(objdir)
