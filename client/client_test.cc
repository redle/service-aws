#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string>
#include <array>
#include <iostream>
#include <sys/time.h>
#include <math.h>

#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "kv.proto.pb.h"

#define HOSTNAME "52.67.158.82"
//#define HOSTNAME "ec2-52-67-158-82.sa-east-1.compute.amazonaws.com"
//#define HOSTNAME "localhost"
#define DEFAULT_PORT 8000
#define MAXDATASIZE 4096

using std::cout;
using std::endl;
using std::string;
using std::array;

using namespace kv::proto;
using namespace google::protobuf::io;

int read_body(int , google::protobuf::uint32 );
google::protobuf::uint32 read_header(char *);
int read_handler(int );
void send_msg(int , req_envelope*);

int setRequest(int fd, string key, string value) {
    string msg;
    req_envelope *envelope = new req_envelope();
    set_request *s_request = new set_request();
    data *payload = new data();

    payload->set_key(key.c_str());
    payload->set_value(value.c_str());
    s_request->set_allocated_req(payload);

    envelope->set_type(kv::proto::req_envelope_msg_type::req_envelope_msg_type_set_request_t);
    envelope->set_allocated_set_req(s_request);

    //printf("Envelope size:%i\n", envelope->ByteSize());

    send_msg(fd, envelope);
    return read_handler(fd);
}

int getRequest(int fd, string key) {
    req_envelope *envelope = new req_envelope();
    get_request *g_request = new get_request();

    g_request->set_key(key.c_str());

    envelope->set_type(kv::proto::req_envelope_msg_type::req_envelope_msg_type_get_request_t);
    envelope->set_allocated_get_req(g_request);
  
    //printf("Envelope size:%i\n", envelope->ByteSize());

    send_msg(fd, envelope);
    return read_handler(fd);
}

int read_handler(int socketfd) {
    char buffer[4];
    int bytecount = 0;

    memset(buffer, '\0', 4);

    if ((bytecount = recv(socketfd, buffer, 4, MSG_PEEK))== -1){
				fprintf(stderr, "Error receiving data %d\n", errno);
    }

    return read_body(socketfd, read_header(buffer));
}

int read_body(int sockfd, google::protobuf::uint32 siz) {
  siz += 4;
  int bytecount;
  char buffer[siz];
  req_envelope payload;
  req_envelope* envelope;

  if((bytecount = recv(sockfd, (void *)buffer, siz, MSG_WAITALL))== -1){
    fprintf(stderr, "Error receiving data %d\n", errno);
  }

  google::protobuf::io::ArrayInputStream ais(buffer,siz);

  CodedInputStream coded_input(&ais);
  coded_input.ReadVarint32(&siz);

  google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(siz);

  payload.ParseFromCodedStream(&coded_input);
  coded_input.PopLimit(msgLimit);

  cout << "Message received: " << payload.DebugString() << endl;

  envelope = new kv::proto::req_envelope();
  envelope->CopyFrom(payload);

  int err = 0;

  switch (envelope->type()) {
    case req_envelope::set_response_t: {
        set_response *response = envelope->release_set_resp();

        if (response->error() == set_response_error_t_internal) {
            err =1;
        }

    }
    break;
    case req_envelope::get_response_t: {
        get_response *response = envelope->release_get_resp();
        if (response->error() == get_response_error_t_internal) {
            err = 1;
        }
    }
    break;
    case req_envelope::set_request_t:
    case req_envelope::get_request_t:
    break;
  }

  return err;
}

void send_msg(int sockfd, req_envelope* envelope) {
    int siz = envelope->ByteSize()+4;
    char *pkt = new char [siz];
    google::protobuf::io::ArrayOutputStream aos(pkt,siz);
    CodedOutputStream *coded_output = new CodedOutputStream(&aos);
    coded_output->WriteVarint32(envelope->ByteSize());
    envelope->SerializeToCodedStream(coded_output);

    int numbytes = send(sockfd, (void *) pkt, siz, 0);
    if (numbytes == -1) {
        fprintf(stderr, "Error sending data %d\n", errno);
        return;
        //exit(1);
    }
    //printf("Sent bytes %d\n", numbytes);
	  cout << "Message sent: " << envelope->DebugString() << endl;
}

google::protobuf::uint32 read_header(char *buf)
{
  google::protobuf::uint32 size;
  google::protobuf::io::ArrayInputStream ais(buf,4);
  CodedInputStream coded_input(&ais);
  coded_input.ReadVarint32(&size);//Decode the HDR and get the size
  //cout<<"size of payload is "<<size<<endl;
  return size;
}

int get_random(int limit) {
	  return (rand() % limit) + 1;
}

void rand_string(char *str, size_t size) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK";
    if (size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return;
}

void print_usage() {
    printf("Usage:\n");
		printf("\t-h <host> \t AWS hostname\n");
		printf("\t-p <port> \t AWS service port, default port 8000 \n");
		printf("\t-s        \t set_request create a new item in dynamodb spefic -k and -v \n");
		printf("\t-g        \t get_request get a item from dynamodb spefic -k \n");
		printf("\t-k <key>  \t key is required with option -s or -g\n");
		printf("\t-v <value>\t value is required with option -g\n");
		printf("\t-t 	      \t test mode set_request and get_request in loop with random data interval 5s\n");
		exit(1);
}

int main(int argc, char** argv) {
    int fd;
    struct hostent *he;
    struct sockaddr_in server;
		int option = 0;
		int port = DEFAULT_PORT;
		int set_mode = 0;
		int get_mode = 0;
		int test_mode = 0;
		int burst_mode = 0;
		int random_seed = 0;
		char *hostname = NULL;
		char *key_item = NULL;
		char *value_item = NULL;

    while ((option = getopt(argc, argv,"h:p:sgk:v:tbr:")) != -1) {
        switch (option) {
             case 'h' : hostname = optarg;
                 break;
             case 'p' : port = atoi(optarg);
                 break;
             case 's' : set_mode = 1;
                 break;
             case 'g' : get_mode = 1;
								 break;
             case 't' : test_mode = 1;
                 break;
             case 'b' : burst_mode = 1;
                 break;
             case 'k' : key_item = optarg;
                 break;
             case 'v' : value_item = optarg;
                 break;
             case 'r': random_seed = atoi(optarg);
                 break;
             default: print_usage();
                 exit(EXIT_FAILURE);
        }
    }

		if (!hostname) {
				print_usage();
		}

		if (get_mode && !key_item) {
				printf("Usage:\n \t %s -h <host> -g -k <key>\n", argv[0]);
				exit(1);
		}

		if (set_mode && (!key_item || !value_item)) {
				//print_usage();
				printf("Usage:\n \t %s -h <host> -s -k <key> -v <value>\n", argv[0]);
				exit(1);
		}

		if (!set_mode && !get_mode && !test_mode) {
				print_usage();
		}

    struct timeval  tv;
    gettimeofday(&tv, NULL);
    if (random_seed > 0) {
	      srand(random_seed);
    } else {
        uint32_t time_in_mill = round(tv.tv_usec / 1000);
	      srand(time(NULL) + time_in_mill);
    }

    if ((he = gethostbyname(hostname)) == NULL) {
        perror("gethostbyname");
        exit(1);
    }

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr = *((struct in_addr *)he->h_addr);

    if (connect(fd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1) {
        perror("connect");
        exit(1);
    }

		if (set_mode) {
        std::string key = key_item;
        std::string value = value_item;
				setRequest(fd, key, value);
		} else if (get_mode) {
        std::string key = key_item;
				getRequest(fd, key);
		} else if (test_mode) {
		    int kidx;
				char svalue[64];

		    while (1) {
						cout << "#####################################" << endl;
						//srand(time(NULL) + get_random(1234));
		        kidx = get_random(100000);

						//rand_string(svalue, 16);
						rand_string(svalue, 64);

		        std::string key = std::to_string(kidx);
		        std::string value = svalue;

		        while (setRequest(fd, key, value) != 0) {
                usleep(100);
            }

		        while (getRequest(fd, key) != 0) {
                usleep(100);
            }

            if (!burst_mode)
								sleep(1);
				}
		}
    close(fd);
    return 0;
}




