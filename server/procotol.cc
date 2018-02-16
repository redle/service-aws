#include "protocol.h"

size_t Protocol::getSize(TCPStream* stream) {
    size_t size;
    char data[HEADER_SIZE];

    size = stream->receive(data, HEADER_SIZE, MSG_PEEK);
    if (size != HEADER_SIZE) {
        return 0;
    }

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

    if (len == 0) {
        return 0;
    }

    return len;
}

int Protocol::getMessage(TCPStream* stream, Message*& message) {
    uint32_t size;

    if (!message) {
        return 1;
    }

    size = Protocol::getSize(stream);
    if (size <= 0) {
        return 1;
    }

    size += HEADER_SIZE;
    uint32_t len;
    char data[size];

    //printf("++++ size: %i\n", size);
    len = stream->receive(data, size);
    //printf(">>>> size: %i len:%i\n", size, len);
    if (len != size) {
        return 1;
    }

    kv::proto::req_envelope* envelope;
    kv::proto::req_envelope payload;
#if 0
    printf("payload raw: ");
    int i;
    for (i=0; i < size; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
#endif
    google::protobuf::io::ArrayInputStream ais(data, size);
    google::protobuf::io::CodedInputStream coded_input(&ais);
    coded_input.ReadVarint32(&size);

    google::protobuf::io::CodedInputStream::Limit msgLimit = coded_input.PushLimit(size);

    payload.ParseFromCodedStream(&coded_input);
    coded_input.PopLimit(msgLimit);

    if (DEBUG_MODE)
        cout << "Message is " << payload.DebugString() << endl;

    envelope = new kv::proto::req_envelope();
    envelope->CopyFrom(payload);

    if (envelope == NULL) {
        return 1;
    }

    switch (envelope->type()) {
      case kv::proto::req_envelope_msg_type::req_envelope_msg_type_set_request_t: {
          message->setType(msg_type::set);

          if (envelope->has_set_req()) {
              kv::proto::set_request *s_request = envelope->mutable_set_req();
              kv::proto::data *req_data = s_request->mutable_req();
              message->setKey(req_data->key());
              message->setValue(req_data->value());
          }
      }
      break;

      case kv::proto::req_envelope_msg_type::req_envelope_msg_type_get_request_t: {
          message->setType(msg_type::get);
          if (envelope->has_get_req()) {
              kv::proto::get_request *g_request = envelope->mutable_get_req();
              message->setKey(g_request->key());
              message->setValue("");
          }
      }
      break;

      default:
          message->setType(msg_type::unknown);
          message->setKey("");
          message->setValue("");
      break;
    }

    delete envelope;

    return 0;
}

int Protocol::response(TCPStream* stream, Message* message) {

    if (message->getType() == msg_type::unknown) {
        return 1;
    }

    kv::proto::req_envelope *envelope = new kv::proto::req_envelope();

    switch(message->getType()) {
        case msg_type::set: {
            kv::proto::set_response *response = new kv::proto::set_response();

            switch (message->getError()) {
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
            if (message->getError() == msg_error::ok) {
                req_data->set_key(message->getKey());
                req_data->set_value(message->getValue());
                response->set_allocated_req(req_data);
            }

            switch (message->getError()) {
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

    stream->send(pkt, len);

    free(pkt);
		delete coded_output;
		delete envelope;

    return 0;
}
