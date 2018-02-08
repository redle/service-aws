/*
   TCPStream.h

   TCPStream class definition. TCPStream provides methods to trasnfer
   data between peers over a TCP/IP connection.

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

#include "tcpstream.h"

TCPStream::TCPStream(int sd, struct sockaddr_in* address) : m_sd(sd) {
    char ip[50];
    inet_ntop(PF_INET, (struct in_addr*)&(address->sin_addr.s_addr), ip, sizeof(ip)-1);
    m_peerIP = ip;
    m_peerPort = ntohs(address->sin_port);
    m_state = connectionConnected;
}

TCPStream::~TCPStream()
{
    close(m_sd);
}

ssize_t TCPStream::send(const char* buffer, size_t len)
{
    return ::send(m_sd, (void *) buffer, len, MSG_NOSIGNAL);
}

ssize_t TCPStream::receive(char* buffer, size_t len, int flags, int timeout)
{

    if (timeout <= 0) return ::recv(m_sd, (void *)buffer, len, flags);

    if (waitForReadEvent(timeout) == true)
    {
      return ::recv(m_sd, (void *)buffer, len, flags);
    }
    return connectionTimedOut;

}

string TCPStream::getPeerIP()
{
    return m_peerIP;
}

int TCPStream::getPeerPort()
{
    return m_peerPort;
}

bool TCPStream::waitForReadEvent(int timeout)
{
    fd_set sdset;
    struct timeval tv;

    tv.tv_sec = timeout;
    tv.tv_usec = 0;
    FD_ZERO(&sdset);
    FD_SET(m_sd, &sdset);
    int ret = select(m_sd+1, &sdset, NULL, NULL, &tv);

    //if (FD_ISSET(m_sd, &sdset)) {
    //    printf("something happened\n");
    //}

    if (ret > 0)
    {
        return true;
    }
    return false;
}

int TCPStream::getSD()
{
    return m_sd;
}

int	TCPStream::setState(int state) {
    m_state = state;
}

int	TCPStream::getState() {
    return m_state;
}
