/*
   main.cpp

   Multithreaded work queue based example server in C++.

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

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <getopt.h>
#include "thread.h"
#include "wqueue.h"
#include "tcpacceptor.h"

using std::cout;
using std::endl;
using std::string;

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "kv.proto.pb.h"

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/ListTablesRequest.h>
#include <aws/dynamodb/model/ListTablesResult.h>
#include <aws/dynamodb/model/AttributeDefinition.h>
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/PutItemResult.h>
#include <aws/dynamodb/model/GetItemRequest.h>
#include <iostream>

#include <aws/kms/KMSClient.h>
#include <aws/kms/model/EncryptRequest.h>
#include <aws/kms/model/EncryptResult.h>
#include <aws/kms/model/DecryptRequest.h>
#include <aws/kms/model/DecryptResult.h>
#include <aws/kms/KMSErrors.h>

#define HEADER_SIZE 4

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

class AwsClient
{
    Aws::Client::ClientConfiguration m_clientConfig;
    Aws::String m_region;
    Aws::SDKOptions m_options;
  public:
    AwsClient(std::string region): m_region(region.c_str()) {
        Aws::InitAPI(m_options);
        m_clientConfig.region = m_region;
    };
    AwsClient() {
        Aws::InitAPI(m_options);
        char *env_region = getenv("AWS_DEFAULT_REGION");
        m_region = env_region;
        m_clientConfig.region = m_region;
    };
    ~AwsClient() {
        Aws::ShutdownAPI(m_options);
    }
    Aws::Client::ClientConfiguration getClientConfig() {
        return m_clientConfig;
    }
};

class Database
{
    AwsClient *m_awsClient;
    Aws::DynamoDB::DynamoDBClient* m_dynamoClient;
    Aws::KMS::KMSClient* m_kmsClient;
    std::string m_table;
    std::string m_keyid;
  public:
    Database(AwsClient* awsClient) {
        m_awsClient = awsClient;
        m_table = "data";
        char *env_arnkey = getenv("AWS_ARN_ENCRYPT_KEY");
        //m_keyid = "arn:aws:kms:sa-east-1:813614406545:key/67c05a98-3c86-40b3-81ab-0e330d38a7d2";
        m_keyid = env_arnkey;
        m_dynamoClient = new Aws::DynamoDB::DynamoDBClient(m_awsClient->getClientConfig());
        m_kmsClient =  new Aws::KMS::KMSClient(m_awsClient->getClientConfig());
    }
    ~Database() {
        delete m_dynamoClient;
        delete m_kmsClient;
    }

		void encrypt(std::string plainText, Aws::Utils::ByteBuffer& cipherText) {
#if 0
				printf(">>>> (%li)", plainText.length());
		    for (unsigned i = 0; i < plainText.length(); ++i) {
						printf("%02x ", (unsigned char) plainText[i]);
				}
				printf("\n");
#endif
				Aws::Utils::ByteBuffer message((unsigned char*) plainText.c_str(), plainText.length());

        Aws::KMS::Model::EncryptRequest encryptRequest;
        encryptRequest.SetKeyId(m_keyid.c_str());
        encryptRequest.SetPlaintext(message);

        Aws::KMS::Model::EncryptOutcome output;
        output = m_kmsClient->Encrypt(encryptRequest);

        Aws::KMS::Model::EncryptResult result = output.GetResult();

        cipherText = result.GetCiphertextBlob();

#if 0
				std::cout << "Error:" << output.GetError().GetMessage() << endl;
			  printf(">>>> (%li)", cipherText.GetLength());
				for (unsigned i = 0; i < cipherText.GetLength(); ++i) {
						printf("%02x <%c> ", (unsigned char) cipherText[i], (char) cipherText[i]);
				}
				printf("\n");
#endif
		}

		void decrypt(Aws::Utils::ByteBuffer cipherText, std::string& message) {
        Aws::KMS::Model::DecryptRequest decryptRequest;
        decryptRequest.SetCiphertextBlob(cipherText);

        Aws::KMS::Model::DecryptOutcome decrypt_output;
        decrypt_output = m_kmsClient->Decrypt(decryptRequest);
        //std::cout << "Error:" << decrypt_output.GetError().GetMessage() << endl;

        Aws::Utils::ByteBuffer plaintext;
        Aws::KMS::Model::DecryptResult result_decrypt = decrypt_output.GetResult();

        plaintext = result_decrypt.GetPlaintext();

				//I didnt find a better way to do this!!!!
				char msg[plaintext.GetLength() + 1];
				for (unsigned i = 0; i < plaintext.GetLength(); ++i) {
						msg[i] = (char) plaintext[i];
				}
				msg[plaintext.GetLength()] = '\0';

				message = msg;
		}

    int getItem(std::string key, std::string& value) {
//value = "TEST FIXED\n";
//return 0;
        Aws::DynamoDB::Model::GetItemRequest req;
        Aws::DynamoDB::Model::AttributeValue haskKey;

        haskKey.SetS(key.c_str());

        req.AddKey("key", haskKey);
        req.SetTableName(m_table.c_str());

        value.clear();

        const Aws::DynamoDB::Model::GetItemOutcome& result = m_dynamoClient->GetItem(req);
        if (!result.IsSuccess()) {
            //std::cout << "Failed to get item: " << result.GetError().GetMessage() << endl;
            return -1;
        }

        const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& item = result.GetResult().GetItem();
        if (item.size() == 0) {
            //std::cout << "No item found with the key " << key << std::endl;
            return 1;
        }

				Aws::Utils::ByteBuffer cipherText = item.find("value")->second.GetB();
				decrypt(cipherText, value);

        return 0;
    }

    int putItem(std::string key, std::string value) {
//return 0;
        const Aws::String skey(key.c_str());
        const Aws::String svalue(value.c_str());
        Aws::Utils::ByteBuffer cipherText;

        Aws::DynamoDB::Model::PutItemRequest pir;
        pir.SetTableName(m_table.c_str());

				encrypt(value, cipherText);

        Aws::DynamoDB::Model::AttributeValue akey;
        Aws::DynamoDB::Model::AttributeValue aencrypt;

        akey.SetS(skey);
        aencrypt.SetB(cipherText);

        pir.AddItem("key", akey);
        pir.AddItem("value", aencrypt);

        const Aws::DynamoDB::Model::PutItemOutcome result = m_dynamoClient->PutItem(pir);
        if (!result.IsSuccess()) {
            std::cout << result.GetError().GetMessage() << std::endl;
            return -1;
        }

        //std::cout << "put item done!" << std::endl;

        return 0;
    }
};

class Message
{
    TCPStream* m_stream;
    msg_type m_type;
    msg_error m_error;
    Data m_data;
    kv::proto::req_envelope* m_envelope;

    int decodeHeader(char *data, int size) {
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

    int decodePayload(char *data, uint32_t size) {
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

        //cout << "Message is " << payload.DebugString() << endl;

        m_envelope = new kv::proto::req_envelope();
        m_envelope->CopyFrom(payload);

        return 0;
    }

    size_t getSize() {
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

    int getPayload(int size) {
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

  public:
    Message(TCPStream* stream) : m_stream(stream), m_type(msg_type::unknown), m_error(msg_error::fail) {}
    ~Message() {
        if (m_envelope) {
            delete m_envelope;
        }
    }
    int unpack() {
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

    int response() {

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

        //cout << "Response is : " << envelope->DebugString();

        m_stream->send(pkt, len);

        free(pkt);

        return 0;
    }

    int pack() {
        return 0;
    }

    Data& getData() {
        return m_data;
    }

    msg_type getType() {
        return m_type;
    }

    void setError(msg_error error) {
        m_error = error;
    }
};

class MessageHandler
{
    TCPStream* m_stream;
    AwsClient *m_awsClient;
    Database *m_database;

    int process() {
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
            err = m_database->putItem(message->getData().getKey(), message->getData().getValue());
        } else if (message->getType() == msg_type::get) {
            std::string value;
            err = m_database->getItem(message->getData().getKey(), value);

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

  public:
    MessageHandler(TCPStream* stream) : m_stream(stream) {
       m_awsClient = new AwsClient();
       m_database = new Database(m_awsClient);
    }
    ~MessageHandler() {
        delete m_awsClient;
    }
    int handler() {
        int err;
        //bool ready;

        //printf("Message Handler process!!!\n");
        while(1) {
            //ready = m_stream->waitForReadEvent(1);
            //if (ready) {
                err = process();
                //printf("> ip:%s err:%i\n", m_stream->getPeerIP().c_str(), err);
                if (err) {
                    return 1;
                }
            //}
            //sleep(600);
        }
    }

};

class WorkItem
{
    TCPStream* m_stream;

  public:
    WorkItem(TCPStream* stream) : m_stream(stream) {}
    ~WorkItem() { delete m_stream; }

    TCPStream* getStream() { return m_stream; }
};

class ConnectionHandler : public Thread
{
    wqueue<WorkItem*>& m_queue;
    TCPStream* m_stream;

  public:
    ConnectionHandler(wqueue<WorkItem*>& queue) : m_queue(queue) {}

    void* run() {
        for (int i = 0;; i++) {
            printf("thread %lu, loop %d - waiting for item...\n",
                   (long unsigned int)self(), i);
            WorkItem* item = m_queue.remove();
            printf("thread %lu, loop %d - got one item\n",
                   (long unsigned int)self(), i);
            TCPStream* stream = item->getStream();

            MessageHandler msg_handler(stream);
            msg_handler.handler();
        }
        return NULL;
    }
};

class ConnectionSingleHandler : public Thread
{
    TCPStream* m_stream;

  public:
    ConnectionSingleHandler(TCPStream* stream) : m_stream(stream) {
        printf("Create MessageHandler\n");
    }

    void* run() {
        MessageHandler msg_handler(m_stream);
printf("MessageHandler run\n");
        int err;
        while (1) {
            //printf("thread %lu - waiting for data...\n", (long unsigned int)self());
            err = msg_handler.handler();
            if (err) {
                printf("client disconnected - %s\n", m_stream->getPeerIP().c_str());
                break;
            }

        }
        delete m_stream;
        return NULL;
    }
};

int main(int argc, char** argv)
{

    if ((getenv("AWS_ACCESS_KEY_ID") == NULL) or
        (getenv("AWS_SECRET_ACCESS_KEY") == NULL) or
        (getenv("AWS_DEFAULT_REGION") == NULL) or
        (getenv("AWS_ARN_ENCRYPT_KEY") == NULL)) {
        printf("AWS Environment Variables:\n");
        printf("\texport AWS_ACCESS_KEY_ID=<ACCESS KEY ID>\n");
        printf("\texport AWS_SECRET_KEY_ID=<SECRET KEY ID>\n");
        printf("\texport AWS_DEFAULT_REGION=<AWS DEFAULT REGION>\n");
        printf("\texport AWS_ARN_ENCRYPT_KEY=<AWS ARN ENCRYPT KEY>\n");
        exit(1);
    }

    if ( argc < 2 || argc > 3 ) {
        printf("usage: %s <port> <ip>\n", argv[0]);
        exit(-1);
    }
    //int workers = atoi(argv[1]);
    int port = atoi(argv[1]);
    string ip;
    if (argc == 3) {
        ip = argv[2];
    }

    signal(SIGPIPE, SIG_IGN);

#if 0
    // Create the queue and consumer (worker) threads
    wqueue<WorkItem*>  queue;
    for (int i = 0; i < workers; i++) {
        ConnectionHandler* handler = new ConnectionHandler(queue);
        if (!handler) {
            printf("Could not create ConnectionHandler %d\n", i);
            exit(1);
        }
        handler->start();
    }

    // Create an acceptor then start listening for connections
    WorkItem* item;
#endif

    TCPAcceptor* connectionAcceptor;
    if (ip.length() > 0) {
        connectionAcceptor = new TCPAcceptor(port, (char*)ip.c_str());
    }
    else {
        connectionAcceptor = new TCPAcceptor(port);
    }
    if (!connectionAcceptor || connectionAcceptor->start() != 0) {
        printf("Could not create an connection acceptor\n");
        return 1;
    }

    int num_cli = 0;
    while (1) {
        TCPStream* connection = connectionAcceptor->accept();
        if (!connection) {
            printf("Could not accept a connection\n");
            continue;
        }

        printf("client: %i - %s\n", ++num_cli, connection->getPeerIP().c_str());
        ConnectionSingleHandler* single_handler = new ConnectionSingleHandler(connection);
        if (!single_handler) {
            printf("Could not create ConnectionSingleHandler\n");
            continue;
        }
        single_handler->start();
#if 0
        item = new WorkItem(connection);
        if (!item) {
            printf("Could not create work item a connection\n");
            continue;
        }
        queue.add(item);
#endif
    }

    return 0;
}
