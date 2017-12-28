LIBS = -lprotobuf -lpthread

all:
	g++ -g -Wall client_test.cc -I ../protobuf/ ../protobuf/kv.proto.pb.o -o client_test -lprotobuf

clean:
	rm client_test *~ -rf
