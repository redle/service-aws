#ifndef __message_h__
#define __message_h__

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "kv.proto.pb.h"

class Message;
class MessageHandler;

#include "protocol.h"
#include "thread.h"
#include "wqueue.h"
#include "tcpacceptor.h"
#include "aws_client.h"
#include "database.h"

extern int DEBUG_MODE;

class Message {
    TCPStream* m_stream;
    msg_type m_type;
    msg_error m_error;
    kv::proto::req_envelope* m_envelope;
    bool m_valid;

    int decodeHeader(char *data, int size);
    size_t getSize();
    int getPayload(int size);
    int decodePayload(char *data, uint32_t size);

    std::string m_key;
    std::string m_value;
  public:
    Message(TCPStream* stream) : m_stream(stream), m_type(msg_type::unknown), m_error(msg_error::fail), m_valid(false) {}
    Message() : m_type(msg_type::unknown), m_error(msg_error::fail), m_valid(false) {}
    ~Message();
    int unpack();
    int response();
    void setError(msg_error );
    msg_error getError();
    bool isValid();

    void setType(msg_type type);
    void setKey(std::string key);
    void setValue(std::string value);

    msg_type getType();
    std::string getKey();
    std::string getValue();
};

class MessageHandler {
    TCPStream* m_stream;
    Database *m_database;
    Message *m_message;
    AwsClient *m_awsClient;

    int process();
    int sync_process();
  public:
    MessageHandler();
    MessageHandler(Database* );
    ~MessageHandler();
    void setStream(TCPStream* );
    int handler();
    void onResponse(msg_error );
    void onResponse(std::string , msg_error );
    int getSD() { return m_stream->getSD(); };
    Message* getMessage();
    void setMessage(Message*& message);
};
#endif
