#include "message.h"

int Message::decodeHeader(char *data, int size) {
    google::protobuf::uint32 len;
    google::protobuf::io::ArrayInputStream ais(data, HEADER_SIZE);
    google::protobuf::io::CodedInputStream coded_input(&ais);
    coded_input.ReadVarint32(&len);
#if 0
    cout << "payload size is: " << size << endl;
    printf("head raw: ");
    int i;
    for (i=0; i < 4; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
#endif
    return len;
}

int Message::decodePayload(char *data, uint32_t size) {
    kv::proto::req_envelope payload;
#if 0
    printf("payload raw: ");
    int i;
    for (i=0; i < size; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
#endif
    google::protobuf::io::ArrayInputStream ais(data,size);
    google::protobuf::io::CodedInputStream coded_input(&ais);
    coded_input.ReadVarint32(&size);

    google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(size);

    payload.ParseFromCodedStream(&coded_input);
    coded_input.PopLimit(msgLimit);

    if (DEBUG_MODE)
        cout << "Message is " << payload.DebugString() << endl;

    m_envelope = new kv::proto::req_envelope();
    m_envelope->CopyFrom(payload);

    return 0;
}

size_t Message::getSize() {
    size_t size;
    char data[HEADER_SIZE];

    size = m_stream->receive(data, HEADER_SIZE, MSG_PEEK);
    if (size != HEADER_SIZE) {
        return 0;
    }

    size = decodeHeader(data, HEADER_SIZE);
    if (size == 0) {
        return 0;
    }

    return size;
}

int Message::getPayload(int size) {
    size += HEADER_SIZE;
    int len;
    char data[size];

    //printf("++++ size: %i\n", size);
    len = m_stream->receive(data, size);
    //printf(">>>> size: %i len:%i\n", size, len);
    if (len != size) {
        return 1;
    }

    int err;
    err = decodePayload(data, size);
    if (err) {
        return 1;
    }

    return 0;
}

Message::~Message() {
}

int Message::unpack() {
    size_t size;

    size = getSize();
    if (size <= 0) {
        return 1;
    }

    int err;
    err = getPayload(size);
    if (err == 1) {
        return 1;
    }

    if (m_envelope == NULL) {
        return 1;
    }

    switch (m_envelope->type()) {
      case kv::proto::req_envelope_msg_type::req_envelope_msg_type_set_request_t: {
          m_type = msg_type::set;

          if (m_envelope->has_set_req()) {
              kv::proto::set_request *s_request = m_envelope->mutable_set_req();
              kv::proto::data *req_data = s_request->mutable_req();
              m_key = req_data->key();
              m_value = req_data->value();
          }
      }
      break;

      case kv::proto::req_envelope_msg_type::req_envelope_msg_type_get_request_t: {
          m_type = msg_type::get;
          if (m_envelope->has_get_req()) {
              kv::proto::get_request *g_request = m_envelope->mutable_get_req();
              m_key = g_request->key();
              m_value = "";
          }
      }
      break;

      default:
          m_type = msg_type::unknown;
          m_key = "";
          m_value = "";
      break;
    }

    return 0;
}

int Message::response() {

    if (m_type == msg_type::unknown) {
        return 1;
    }

    kv::proto::req_envelope *envelope = new kv::proto::req_envelope();

    switch(m_type) {
        case msg_type::set: {
            kv::proto::set_response *response = new kv::proto::set_response();

            switch (m_error) {
                case msg_error::ok:
                    response->set_error(kv::proto::set_response_error_t::set_response_error_t_ok);
                break;
                case msg_error::fail:
                    response->set_error(kv::proto::set_response_error_t::set_response_error_t_internal);
                break;
                case msg_error::not_found:
                break;
            }

            envelope->set_type(kv::proto::req_envelope_msg_type::req_envelope_msg_type_set_response_t);
            envelope->set_allocated_set_resp(response);
						//delete response;
        }
        break;

        case msg_type::get: {
            kv::proto::get_response *response = new kv::proto::get_response();

            kv::proto::data *req_data = new kv::proto::data();
            if (m_error == msg_error::ok) {
                req_data->set_key(m_key);
                req_data->set_value(m_value);
                response->set_allocated_req(req_data);
            }

            switch (m_error) {
                case msg_error::ok:
                    response->set_error(kv::proto::get_response_error_t::get_response_error_t_ok);
                break;
                case msg_error::fail:
                    response->set_error(kv::proto::get_response_error_t::get_response_error_t_internal);
                break;
                case msg_error::not_found:
                    response->set_error(kv::proto::get_response_error_t::get_response_error_t_not_found);
                break;
            }

            envelope->set_type(kv::proto::req_envelope_msg_type::req_envelope_msg_type_get_response_t);
            envelope->set_allocated_get_resp(response);
		        //delete response;
        }
        break;
        case msg_type::unknown:
        break;
    }

    int len;
    len = envelope->ByteSize() + HEADER_SIZE;
    char *pkt = (char*) malloc(len);

    if (pkt == NULL) {
        perror("malloc");
        return 1;
    }

    google::protobuf::io::ArrayOutputStream aos(pkt, len);
    google::protobuf::io::CodedOutputStream *coded_output = new google::protobuf::io::CodedOutputStream(&aos);
    coded_output->WriteVarint32(envelope->ByteSize());
    envelope->SerializeToCodedStream(coded_output);

    if (DEBUG_MODE)
        cout << "Response is : " << envelope->DebugString();

    printf("socket:%i\n", m_stream->getSD());

    m_stream->send(pkt, len);

    free(pkt);
		delete coded_output;
		delete envelope;

    return 0;
}

bool Message::isValid() {
    return m_valid;
}

msg_type Message::getType() {
    return m_type;
}

msg_error Message::getError() {
    return m_error;
}

void Message::setError(msg_error error) {
    m_error = error;
}

void Message::setType(msg_type type) {
    m_type = type;
}

void Message::setKey(std::string key) {
    m_key = key;
}

void Message::setValue(std::string value) {
    m_value = value;
}

std::string Message::getKey() {
    return m_key;
}

std::string Message::getValue() {
    return m_value;
}

MessageHandler::MessageHandler(Database* database) {
   m_database = database;
   database->setHandler(this);
}

MessageHandler::MessageHandler() {
   m_awsClient = AwsClient::instance();
   m_database = new DynamoDB(m_awsClient, true);
   m_database->setHandler(this);
}

MessageHandler::~MessageHandler() {
    delete m_database;
    delete m_message;
}

Message* MessageHandler::getMessage() {
#if 0
    int err;
    Message* message = new Message();
    Protocol* protocol = new Protocol(m_stream);

    err = protocol->getMessage();
    if (err) {
        return NULL;
    }

    printf("type: %i\n", protocol->getData().getType());
    printf("key: %s\n", protocol->getData().getKey().c_str());
    printf("value: %s\n", protocol->getData().getValue().c_str());

    message->setType(protocol->getData().getType());
    message->setKey(protocol->getData().getKey());
    message->setValue(protocol->getData().getValue());
#endif
    return 0;
}

void MessageHandler::setMessage(Message*& message) {
    m_message = message;
}

int MessageHandler::process() {
    int err;

    printf("type: %i\n", m_message->getType());
    printf("key: %s\n", m_message->getKey().c_str());
    printf("value: %s\n", m_message->getValue().c_str());

    if (m_message->getType() == msg_type::set) {
        err = m_database->putItem(m_message);
    } else if (m_message->getType() == msg_type::get) {
        std::string value;
        err = m_database->getItem(m_message);

        if (value.size() > 0) {
            m_message->setValue(value);
            err = 0;
        } else {
            err = 1;
        }
    }
#if 0
    switch (err) {
        case -1:
            m_message->setError(msg_error::fail);
        break;

        case 1:
            m_message->setError(msg_error::not_found);
        break;

        case 0:
        default:
            m_message->setError(msg_error::ok);
        break;
    }

    message->response();
    delete message;
#endif
    return 0;
}

int MessageHandler::sync_process() {
    int err;
    Message* message = new Message(m_stream);
    err = message->unpack();

    if (err) {
        return 1;
    }
#if 0
    printf("type: %i\n", message->getType());
    printf("key: %s\n", message->getKey().c_str());
    printf("value: %s\n", message->getValue().c_str());
#endif

    if (message->getType() == msg_type::set) {
        err = m_database->putItem(message);
    } else if (message->getType() == msg_type::get) {
        std::string value;
        err = m_database->getItem(message);

        if (value.size() > 0) {
            message->setValue(value);
            err = 0;
        } else {
            err = 1;
        }
    }

    switch (err) {
        case -1:
            message->setError(msg_error::fail);
        break;

        case 1:
            message->setError(msg_error::not_found);
        break;

        case 0:
        default:
            message->setError(msg_error::ok);
        break;
    }

    message->response();
    delete message;

    return 0;
}

void MessageHandler::setStream(TCPStream* stream) {
	  m_stream = stream;
}

int MessageHandler::handler() {
    int err;

    while(1) {
        err = process();
        if (err) {
            return 1;
        }
				return 0;
    }
}

void MessageHandler::onResponse(msg_error err) {
    printf("MessageHandler::onResponse -> putItem %i\n", getSD());

    std::string key = m_message->getKey();
    std::string value = m_message->getValue();

    printf(">>>>>>>>>>>>>> key:%s value:%s\n", key.c_str(), value.c_str());

    m_message->setError(err);
    Protocol::response(m_stream, m_message);

    delete this;

    return;
}

void MessageHandler::onResponse(std::string value, msg_error err) {
    printf("MessageHandler::onResponse -> getItem %i\n", getSD());

    std::string key = m_message->getKey();
    m_message->setValue(value.c_str());

    printf(">>>>>>>>>>>>>> key:%s value:%s\n", key.c_str(), value.c_str());

    m_message->setError(err);
    Protocol::response(m_stream, m_message);

    delete this;

    return;
}
