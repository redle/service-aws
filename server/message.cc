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
   m_encrypt = KMSEncrypt::instance();
   m_encryption = true;
}

MessageHandler::~MessageHandler() {
}

void MessageHandler::setMessage(Message*& message) {
    m_message = message;
}

void MessageHandler::setStream(TCPStream* stream) {
    m_stream = stream;
}

int MessageHandler::handler() {
    int err = 0;

    switch (m_message->getState()) {
        case Message::state_t::pending: {

            if (m_message->getType() == msg_type::set) {
                if (m_encryption) {
                    m_encrypt_task = m_encrypt->encrypt(m_message->getValue());
                    m_message->setState(Message::state_t::encrypting);
                } else {
                    m_db_task = m_database->putItem(m_message);
                    m_message->setState(Message::state_t::databasing);
                }
            } else if (m_message->getType() == msg_type::get) {
                m_db_task = m_database->getItem(m_message);
                m_message->setState(Message::state_t::databasing);
            }
        }
        break;

        case Message::state_t::encrypting: {
            //printf("Message -> encrypting (%s)\n", m_message->getKey().c_str());

            EncryptTask::state_t state = m_encrypt->handleTask(m_message, m_encrypt_task);

            if (state != EncryptTask::state_t::ready)
                break;

            switch (m_message->getType()) {
                case msg_type::set:
                    m_message->setState(Message::state_t::databasing);
                    m_db_task = m_database->putItem(m_message);
                break;

                case msg_type::get:
                    Protocol::response(m_stream, m_message);
                    m_message->setState(Message::state_t::ready);
                break;

                case msg_type::unknown:
                default:
                break;
            }
        }
        break;

        case Message::state_t::databasing: {
            //printf("Message -> databasing (%s)\n", m_message->getKey().c_str());

            DatabaseTask::state_t state = m_database->handleTask(m_message, m_db_task);
            if (state != DatabaseTask::state_t::ready)
                break;

            if ((m_message->getType() == msg_type::set) || (m_message->getError() == msg_error::not_found)) {
                Protocol::response(m_stream, m_message);
                m_message->setState(Message::state_t::ready);
            } else {
                m_encrypt_task = m_encrypt->decrypt(m_message->getValue());
                m_message->setState(Message::state_t::encrypting);
            }
        }
        break;

        case Message::state_t::ready:
        default:
        break;
    }

    return err;
}

