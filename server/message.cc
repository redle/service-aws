#include "message.h"

Message::~Message() {
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

void Message::setState(state_t state ) {
    m_state = state;
}

Message::state_t Message::getState() {
    return m_state;
}

MessageHandler::MessageHandler(Database* database) {
   m_database = database;
}

MessageHandler::MessageHandler() {
   m_awsClient = AwsClient::instance();
   //m_database = new DynamoDB(m_awsClient, true);
   m_database = DynamoDB::instance(m_awsClient, true);
}

MessageHandler::~MessageHandler() {
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

void MessageHandler::setStream(TCPStream* stream) {
	  m_stream = stream;
}

int MessageHandler::handler() {
    int err = 0;

    if (m_message->getState() == Message::state_t::pending) {
        m_message->setState(Message::state_t::processing);

        if (m_message->getType() == msg_type::set) {
            m_queue_item = m_database->putItem(m_message);
        } else if (m_message->getType() == msg_type::get) {
            m_queue_item = m_database->getItem(m_message);
        }

    } else if (m_message->getState() == Message::state_t::processing) {
        DatabaseTask::state_t state;

        state = m_database->consume(m_message, m_queue_item);

        if (state == DatabaseTask::state_t::ready) {
				    Protocol::response(m_stream, m_message);
	          m_message->setState(Message::state_t::ready);
            //printf("Message -> ready - key: %s\n", m_message->getKey().c_str());
            #if 0
            printf("type: %i\n", m_message->getType());
            printf("key: %s\n", m_message->getKey().c_str());
            printf("value: %s\n", m_message->getValue().c_str());
            #endif
        }
    }

	  return err;
}

