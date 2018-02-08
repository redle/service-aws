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

wqueue<WorkItem*> feedback;


class ConnectionHandler : public Thread
{
    wqueue<WorkItem*>& m_queue;
    TCPStream* m_stream;

  public:
    ConnectionHandler(wqueue<WorkItem*>& queue) : m_queue(queue) {}

    void* run() {
				int err;
				MessageHandler *msg_handler = new MessageHandler();

        for (int i = 0;; i++) {
            printf("thread %lu, loop %d - waiting for item... size:%i\n", (long unsigned int)self(), i, m_queue.size());
            WorkItem* item = m_queue.remove();
						item->setState(WorkItem::processing);
            printf("thread %lu, loop %d - got one item -> size:%i\n", (long unsigned int)self(), i, m_queue.size());
            TCPStream* stream = item->getStream();
            msg_handler->setStream(stream);

            err = msg_handler->handler();
						if (err) {
								printf(">>>> Closed\n");
								stream->setState(TCPStream::connectionClosed);
						}
						item->setState(WorkItem::available);
						feedback.add(item);
						printf(">>>> feedback: %i\n", feedback.size());
        }
        return NULL;
    }
};

class ConnectionSingleHandler : public Thread
{
    TCPStream* m_stream;

  public:
    ConnectionSingleHandler(TCPStream* stream) : m_stream(stream) {
        //printf("Create MessageHandler\n");
    }

    void* run() {
        MessageHandler msg_handler(m_stream);
        int err;
        while (1) {
            //printf("thread %lu - waiting for data...\n", (long unsigned int)self());
            err = msg_handler.handler();
            if (err) {
                printf("client disconnected - %s\n", m_stream->getPeerIP().c_str());
                break;
            }

        }
        delete m_stream;
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
    wqueue<WorkItem*> queue;
    for (int i = 0; i < workers; i++) {
        ConnectionHandler* handler = new ConnectionHandler(queue);
        if (!handler) {
            printf("Could not create ConnectionHandler %d\n", i);
            exit(1);
        }
        handler->start();
    }

    // Create an acceptor then start listening for connections
    WorkItem* item;

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

		pollfd fds[5000];
	  int i, current_size, nfds = 1;

		memset(fds, 0 , sizeof(fds));

		fds[0].fd = server_socket;
	  fds[0].events = POLLIN;

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
					  if(fds[i].revents == 0)
				        continue;

						if(fds[i].revents != POLLIN) {
				        printf("  Error! revents = %d\n", fds[i].revents);
								break;
						}

						if ((fds[i].fd == server_socket) && (fds[i].revents & POLLIN)) {
				        client_socket = connectionAcceptor->accept(connection);
						    if (client_socket == -1) {
							    //printf("Could not accept a connection\n");
									//continue;
									printf("Error, could not accept a new connection\n");
								}

				        if ((connection != NULL) && (client_socket > 0)) {
						        printf("client: %i - %s\n", ++num_cli, connection->getPeerIP().c_str());


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
										printf("New client arrived sock:%i (size:%i) -> %i\n", client_socket, clients.size(), item->getState());
						    }

								continue;
						}

					  /* IMPROVE THIS..... NEW CLIENTS ARE WAITING FOR NEXT POOL */
						if (fds[i].revents & POLLIN) {
								item = (clients[fds[i].fd]);
								connection = item->getStream();
			          client_socket = fds[i].fd;

                if (connection->getState() != TCPStream::connectionConnected) {
                    printf("client disconnected - %s (%i)\n", connection->getPeerIP().c_str(), connection->getSD());
								    fds[i].fd = -1;
                    continue;
                }

							  printf(">>>>> item -> %i-%i\n", fds[i].fd, item->getState());
							  if (item->getState() != WorkItem::available) {
									  printf(">>>>> Already added in queue\n");
									  continue;
							  }

							  /*IMPROVE THIS.....*/
							  printf("Added client on queue\n");
							  item->setState(WorkItem::pending);
							  queue.add(clients[fds[i].fd]);
							  printf("removed -> %i %i\n", item->getFds(), item->getStream()->getSD());
							  fds[i].fd = -1;
				   }
#if 0
        client_socket = connectionAcceptor->accept(connection);
        ConnectionSingleHandler* single_handler = new ConnectionSingleHandler(connection);
        if (!single_handler) {
            printf("Could not create ConnectionSingleHandler\n");
            continue;
        }
        single_handler->start();
#endif

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
