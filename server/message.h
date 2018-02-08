#ifndef __message_h__
#define __message_h__

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "kv.proto.pb.h"

class Message;

#include "thread.h"
#include "wqueue.h"
#include "tcpacceptor.h"
#include "aws_client.h"
#include "database.h"

#define HEADER_SIZE 4

extern int DEBUG_MODE;

enum msg_type_t {
    unknown,
    set,
    get
};
typedef msg_type_t msg_type;

enum msg_error_t {
    ok,
    fail,
    not_found
};
typedef msg_error_t msg_error;

typedef struct {
    std::string key;
    std::string value;
} msg_data;

class Data
{
    std::string m_key;
    std::string m_value;
  public:
    Data(): m_key(""), m_value("") {}
    Data(std::string key, std::string value) {
        m_key = key;
        m_value = value;
    }
    ~Data() {}
    std::string getKey() {
        return m_key;
    }
    void setKey(std::string key) {
        m_key = key;
    }
    std::string getValue() {
        return m_value;
    }
    void setValue(std::string value) {
        m_value = value;
    }
};

class Message {
    TCPStream* m_stream;
    msg_type m_type;
    msg_error m_error;
    Data m_data;
    kv::proto::req_envelope* m_envelope;

    int decodeHeader(char *data, int size);
    size_t getSize();
    int getPayload(int size);
    int decodePayload(char *data, uint32_t size);

  public:
    Message(TCPStream* stream) : m_stream(stream), m_type(msg_type::unknown), m_error(msg_error::fail) {}
    ~Message();
    int unpack();
    int response();
    int pack();
    Data& getData();
    msg_type getType();
    void setError(msg_error );
    void onResponse();
};

class MessageHandler {
    TCPStream* m_stream;
    AwsClient *m_awsClient;
    DynamoDB *m_database;
    //FooDB *m_database;

    int process();
    int sync_process();
  public:
    MessageHandler();
    MessageHandler(TCPStream*& );
    ~MessageHandler();
    void setStream(TCPStream*& );
    int handler();
};
#endif
