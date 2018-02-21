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
#include "encrypt.h"

extern int DEBUG_MODE;

class Message {
  public:
    enum state_t {
        pending = 0,
        databasing = 1,
        encrypting = 2,
        ready = 3
    };

  private:
    msg_type m_type;
    msg_error m_error;

    std::string m_key;
    std::string m_value;
    state_t m_state;

  public:
    Message() : m_type(msg_type::unknown), m_error(msg_error::fail), m_state(state_t::pending) {}
    ~Message();
    void setError(msg_error );
    msg_error getError();
    bool isValid();

    void setType(msg_type );
    msg_type getType();
    void setKey(std::string );
    std::string getKey();
    void setValue(std::string );
    std::string getValue();
    void setState(state_t state );
    state_t getState();
};

class MessageHandler {
    TCPStream* m_stream;
    Database* m_database;
    Message* m_message;
    DatabaseTask* m_db_task;
    EncryptTask* m_encrypt_task;
    AwsClient* m_awsClient;
    Encrypt* m_encrypt;
    bool m_encryption;
  public:
    MessageHandler();
    MessageHandler(Database* );
    ~MessageHandler();
    void setStream(TCPStream* );
    int handler();
    int getSD() { return m_stream->getSD(); };
    Message* getMessage() { return m_message; };
    void setMessage(Message*& message);
    void setEncryption(bool encryption) { m_encryption = encryption; };
};
#endif
