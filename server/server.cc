/*
   main.cpp

   Multithreaded work queue based example server in C++.

   ------------------------------------------

   Copyright (c) 2013 Vic Hargrave

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <list>
#include <utility>
#include <getopt.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>

#include "thread.h"
#include "wqueue.h"
#include "tcpacceptor.h"
#include "message.h"

using std::cout;
using std::endl;
using std::string;

#include <iostream>

int DEBUG_MODE = 0;

class WorkItem
{
    TCPStream* m_stream;
		int m_state;
		int m_fds;

  public:
    WorkItem(TCPStream* stream) : m_stream(stream) {}
    ~WorkItem() { delete m_stream; }

		int getState() { return m_state; }
		void setState(int state) { m_state = state; }
		int getFds() { return m_fds; }
		void setFds(int fds) { m_fds = fds; }
    TCPStream* getStream() { return m_stream; }

		enum {
				processing = 0,
				pending = 1,
				available = 2
		};
};


class WorkItemMessage
{
    TCPStream* m_stream;
		Message* m_message;

  public:
    WorkItemMessage(TCPStream* stream, Message* message) : m_stream(stream), m_message(message) {}
    ~WorkItemMessage() {
				//delete m_stream;
				delete m_message;

				//delete m_database;
				delete m_handler;
		}

    TCPStream* getStream() { return m_stream; }
		Message*& getMessage() { return m_message; }

		enum {
				processing = 0,
				pending = 1,
				available = 2
		};

		DynamoDB* m_database;
		//FooDB* m_database;
		MessageHandler* m_handler;
};

wqueue<WorkItem*> feedback;

class ConnectionMessageHandler : public Thread
{
    wqueue<WorkItemMessage*>& m_queue;

  public:
    ConnectionMessageHandler(wqueue<WorkItemMessage*>& queue) : m_queue(queue) {}

    void* run() {
				int err;
        AwsClient *awsClient = AwsClient::instance();
				/*Create a object poll to reuse old objects*/
        DynamoDB *database = new DynamoDB(awsClient, true);
        //FooDB *database;
				MessageHandler *msg_handler;
				WorkItemMessage* item;

        for (int i = 0;; i++) {
            //printf("thread %lu, loop %d - waiting for item... size:%i\n", (long unsigned int)self(), i, m_queue.size());
            item = m_queue.remove();
            //printf("thread %lu, loop %d - got one item -> size:%i\n", (long unsigned int)self(), i, m_queue.size());

						database->consume();

						switch (item->getMessage()->getState()) {
								case (Message::state_t::pending): {
										//printf("Message -> pending\n");
										//item->m_database = new DynamoDB(awsClient, true);
										//item->m_database = new FooDB();
									  //item->m_handler = new MessageHandler(item->m_database);
									  item->m_handler = new MessageHandler(database);
										//msg_handler = new MessageHandler();
								    //item->m_handler->setStream(item->getStream());
								    item->m_handler->setMessage(item->getMessage());
										//msg_handler->getMessage();

				            err = item->m_handler->handler();
										if (err) {
												printf(">>>> Closed\n");
												//stream->setState(TCPStream::connectionClosed);
										}
										//delete msg_handler;
										m_queue.add(item);
								}
								break;
								case (Message::state_t::processing): {
										//printf("Message -> processing\n");
										m_queue.add(item);
								}
								break;
								case (Message::state_t::ready): {
										//printf("Message -> ready\n");
										Protocol::response(item->getStream(), item->getMessage());
										delete item;
								}
								break;
						}

						usleep(10);
						//item->setState(WorkItem::available);
						//feedback.add(item);
						//printf(">>>> feedback: %i\n", feedback.size());
        }

				delete awsClient;

        return NULL;
    }
};

int main(int argc, char** argv)
{

    if ((getenv("AWS_ACCESS_KEY_ID") == NULL) or
        (getenv("AWS_SECRET_ACCESS_KEY") == NULL) or
        (getenv("AWS_DEFAULT_REGION") == NULL) or
        (getenv("AWS_ARN_ENCRYPT_KEY") == NULL)) {
        printf("AWS Environment Variables:\n");
        printf("\texport AWS_ACCESS_KEY_ID=<ACCESS KEY ID>\n");
        printf("\texport AWS_SECRET_KEY_ID=<SECRET KEY ID>\n");
        printf("\texport AWS_DEFAULT_REGION=<AWS DEFAULT REGION>\n");
        printf("\texport AWS_ARN_ENCRYPT_KEY=<AWS ARN ENCRYPT KEY>\n");
        exit(1);
    }

    if (getenv("DEBUG_MODE")) {
        printf("Mode debug!\n");
        DEBUG_MODE=1;
    }

    if ( argc < 2 || argc > 3 ) {
        printf("usage: %s <port> <ip>\n", argv[0]);
        exit(-1);
    }
    //int workers = atoi(argv[1]);
    int workers = 1;
    int port = atoi(argv[1]);
    string ip;
    if (argc == 3) {
        ip = argv[2];
    }

    signal(SIGPIPE, SIG_IGN);

    // Create the queue and consumer (worker) threads
    wqueue<WorkItemMessage*> queue;
    for (int i = 0; i < workers; i++) {
        ConnectionMessageHandler* handler = new ConnectionMessageHandler(queue);
        if (!handler) {
            printf("Could not create ConnectionHandler %d\n", i);
            exit(1);
        }
        handler->start();
    }

    // Create an acceptor then start listening for connections
    WorkItem* item;
    WorkItemMessage* item_message;
		Message *message;

    TCPAcceptor* connectionAcceptor;
    if (ip.length() > 0) {
        connectionAcceptor = new TCPAcceptor(port, (char*)ip.c_str());
    }
    else {
        connectionAcceptor = new TCPAcceptor(port);
    }
    if (!connectionAcceptor || connectionAcceptor->start() != 0) {
        printf("Could not create an connection acceptor\n");
        return 1;
    }

    TCPStream* connection = NULL;
    int num_cli = 0;
    int server_socket = 0;
    int client_socket = 0;

    int max_sd;
    fd_set working_set, readfds;
    //fd_set working_set;

    server_socket = connectionAcceptor->getSD();

    FD_ZERO(&readfds);
    max_sd = server_socket;
    FD_SET(max_sd, &readfds);

    //std::list<WorkItem*> clients;
		//std::vector<WorkItem*> clients;
		std::map<int, WorkItem*> clients;

		int activity;

		pollfd fds[50000];
	  int i, current_size, nfds = 1;

		memset(fds, 0 , sizeof(fds));

		fds[0].fd = server_socket;
	  fds[0].events = POLLIN;

		int num_clients = 0;

    while (1) {
				memcpy(&working_set, &readfds, sizeof(readfds));

				//printf("waiting for clients....\n");

				activity = poll(fds, nfds, 100);
        if (activity < 0) {
            printf("select error");
						break;
        }

				while (feedback.size() > 0) {
						item = feedback.remove();
						printf("added again -> %i %i\n", item->getFds(), item->getStream()->getSD());
						fds[item->getFds()].fd = item->getStream()->getSD();
				}

				if (activity == 0) {
						//printf(">>>>> timeout\n");
						continue;
				}

				current_size = nfds;
		    for (i = 0; i < current_size; i++) {
					  if (fds[i].revents == 0)
				        continue;

						if(fds[i].revents != POLLIN) {
				        printf("  Error! revents = (%i) %d\n", i, fds[i].revents);
								//break;
						}

						if ((fds[i].fd == server_socket) && (fds[i].revents & POLLIN)) {
				        client_socket = connectionAcceptor->accept(connection);
						    if (client_socket == -1) {
							    //printf("Could not accept a connection\n");
									//continue;
									printf("Error, could not accept a new connection\n");
								}

				        if ((connection != NULL) && (client_socket > 0)) {

										/*IMPROVE THIS.....*/
										fds[nfds].fd = client_socket;
					          fds[nfds].events = POLLIN;
								    nfds++;

										item = new WorkItem(connection);
										if (!item) {
												printf("Could not create work item a connection\n");
												continue;
										}

										item->setState(WorkItem::available);
										/*IMPROVE THIS.....*/
										item->setFds(nfds-1);
										clients[client_socket] = item;
										num_clients++;
										printf("New client arrived -> sock:%i ip:%s (size:%li) -> %i (clients:%i)\n", client_socket, connection->getPeerIP().c_str(), clients.size(), item->getState(), num_clients);
						    } else {
										printf("error 2\n");
								}

								continue;
						}

					  /* IMPROVE THIS..... NEW CLIENTS ARE WAITING FOR NEXT POOL */
						if (fds[i].revents & POLLIN) {
								item = (clients[fds[i].fd]);
								connection = item->getStream();
			          client_socket = fds[i].fd;

								message = new Message();
								int err = Protocol::getMessage(connection, message);
								if (err) {
										num_clients--;
                    printf("client disconnected - %s (%i) - (clients:%i)\n", connection->getPeerIP().c_str(), connection->getSD(), num_clients);
										close(fds[i].fd);
								    fds[i].fd = -1;
                    continue;
						    }

								item_message = new WorkItemMessage(connection, message);

							  queue.add(item_message);

                if (connection->getState() != TCPStream::connectionConnected) {
                    printf("client disconnected - %s (%i)\n", connection->getPeerIP().c_str(), connection->getSD());
								    fds[i].fd = -1;
                    continue;
                }
#if 0
							  printf(">>>>> item -> %i-%i\n", fds[i].fd, item->getState());
							  if (item->getState() != WorkItem::available) {
									  printf(">>>>> Already added in queue\n");
									  continue;
							  }
							  /*IMPROVE THIS.....*/
							  printf("Added client on queue\n");
							  item->setState(WorkItem::pending);
#endif
							  //queue.add(clients[fds[i].fd]);
							  //printf("removed -> %i %i\n", item->getFds(), item->getStream()->getSD());
							  //fds[i].fd = -1;
				   }

#if 0
        item = new WorkItem(connection);
        if (!item) {
            printf("Could not create work item a connection\n");
            continue;
        }
        queue.add(item);
#endif
				}

    }

    return 0;
}
