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
    if (m_envelope) {
        delete m_envelope;
    }
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
              m_data.setKey(req_data->key());
              m_data.setValue(req_data->value());
          }
      }
      break;

      case kv::proto::req_envelope_msg_type::req_envelope_msg_type_get_request_t: {
          m_type = msg_type::get;
          if (m_envelope->has_get_req()) {
              kv::proto::get_request *g_request = m_envelope->mutable_get_req();
              m_data.setKey(g_request->key());
              m_data.setValue("");
          }
      }
      break;

      default:
          m_type = msg_type::unknown;
          m_data.setKey("");
          m_data.setValue("");
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
                req_data->set_key(m_data.getKey());
                req_data->set_value(m_data.getValue());
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

    m_stream->send(pkt, len);

    free(pkt);
		delete coded_output;
		delete envelope;

    return 0;
}

void Message::onResponse() {
		printf("on response");
#if 0
    if (getType() == msg_type::set) {
        err = m_database->putItem(message);
    } else if (getType() == msg_type::get) {
        std::string value;
        err = m_database->getItem(message);

        if (value.size() > 0) {
            message->getData().setValue(value);
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
    delete this;
#endif
		return;
}

int Message::pack() {
    return 0;
}

Data& Message::getData() {
    return m_data;
}

msg_type Message::getType() {
    return m_type;
}

void Message::setError(msg_error error) {
    m_error = error;
}

int MessageHandler::process() {
    int err;
    Message* message = new Message(m_stream);
    err = message->unpack();

    if (err) {
        return 1;
    }
#if 0
    printf("type: %i\n", message->getType());
    printf("key: %s\n", message->getData().getKey().c_str());
    printf("value: %s\n", message->getData().getValue().c_str());
#endif

    if (message->getType() == msg_type::set) {
        err = m_database->putItem(message);
    } else if (message->getType() == msg_type::get) {
        std::string value;
        err = m_database->getItem(message);

        if (value.size() > 0) {
            message->getData().setValue(value);
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

int MessageHandler::sync_process() {
    int err;
    Message* message = new Message(m_stream);
    err = message->unpack();

    if (err) {
        return 1;
    }
#if 0
    printf("type: %i\n", message->getType());
    printf("key: %s\n", message->getData().getKey().c_str());
    printf("value: %s\n", message->getData().getValue().c_str());
#endif

    if (message->getType() == msg_type::set) {
        err = m_database->putItem(message);
    } else if (message->getType() == msg_type::get) {
        std::string value;
        err = m_database->getItem(message);

        if (value.size() > 0) {
            message->getData().setValue(value);
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

MessageHandler::MessageHandler() {
   m_awsClient = new AwsClient();
   m_database = new DynamoDB(m_awsClient);
   //m_database = new FooDB();
}

MessageHandler::MessageHandler(TCPStream*& stream) : MessageHandler() {
		m_stream = stream;
}

MessageHandler::~MessageHandler() {
    delete m_awsClient;
}

void MessageHandler::setStream(TCPStream*& stream) {
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
