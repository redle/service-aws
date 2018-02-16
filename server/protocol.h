#ifndef __protocol_h__
#define __protocol_h__

#include <iostream>
#include <string>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "kv.proto.pb.h"

class Protocol;

#include "tcpacceptor.h"

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

#include "message.h"

class Protocol {
    static size_t getSize(TCPStream* );

  public:
    static int getMessage(TCPStream* stream, Message*& message);
    static int response(TCPStream* stream, Message* message);
};

#endif
