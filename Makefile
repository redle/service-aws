CC		    = g++
CFLAGS		= -c -Wall -g3 -pg
LDFLAGS		= -lpthread -lprotobuf -laws-cpp-sdk-core -laws-cpp-sdk-dynamodb -laws-cpp-sdk-kms
SOURCES		= server/aws_client.cc server/procotol.cc server/message.cc server/database.cc server/server.cc threads/thread.cc tcpsockets/tcpacceptor.cc tcpsockets/tcpstream.cc protobuf/kv.proto.pb.cc
INCLUDES	= -Ithreads -Iwqueue -Itcpsockets -Iprotobuf -Iserver
OBJECTS		= $(SOURCES:.cc=.o) 
TARGET		= service

all: $(SOURCES) $(TARGET) client-test

gprotobuf:
	#protoc --cpp_out=./ protobuf/kv.proto.proto
	make -C protobuf/

client-test:
	g++ -g -Wall client/client_test.cc -I protobuf/ protobuf/kv.proto.pb.o -o client_test -lprotobuf

$(TARGET): $(OBJECTS) 
	$(CC)  $(OBJECTS) -o $@ $(LDFLAGS)

.cc.o:
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

clean:
	rm -rf $(OBJECTS) $(TARGET) client_test
