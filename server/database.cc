#include <future>
#include "database.h"

DatabaseTask* FooDB::getItem(Message*& message) {
    message->setValue("TEST FIXED\n");
    message->setError(msg_error::ok);
    message->setState(Message::state_t::ready);
    return m_queue_item;
}

DatabaseTask* FooDB::putItem(Message*& message) {
    message->setError(msg_error::ok);
    message->setState(Message::state_t::ready);
    return m_queue_item;
}

DynamoDB* DynamoDB::m_instance = 0;
std::shared_ptr<Aws::DynamoDB::DynamoDBClient> DynamoDB::m_dynamoClient = 0;

DynamoDB::DynamoDB(AwsClient* awsClient, bool async = true) {
    m_awsClient = awsClient;
    m_table = "data";
    m_async = async;
    char *env_arnkey = getenv("AWS_ARN_ENCRYPT_KEY");
    m_keyid = env_arnkey;
    //m_dynamoClient = new Aws::DynamoDB::DynamoDBClient(m_awsClient->getClientConfig());
    m_dynamoClient = Aws::MakeShared<Aws::DynamoDB::DynamoDBClient>("MCAWSSERVICE", m_awsClient->getClientConfig());


    pthread_mutex_init(&m_mutex, NULL);
}

DynamoDB::~DynamoDB() {
    //delete m_dynamoClient;
}

void DynamoDB::encrypt(std::string plainText, Aws::Utils::ByteBuffer& cipherText) {
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

void DynamoDB::decrypt(Aws::Utils::ByteBuffer cipherText, std::string& message) {
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

DatabaseTask* DynamoDB::getItem(Message*& message) {
    m_message = message;
    std::string key = message->getKey();
		std::string value;

    Aws::DynamoDB::Model::GetItemRequest getItemRequest;
    Aws::DynamoDB::Model::AttributeValue haskKey;

    haskKey.SetS(key.c_str());

    getItemRequest.AddKey("key", haskKey);
    getItemRequest.SetTableName(m_table.c_str());

    DatabaseTask* item = new DatabaseTask();
    item->getItemOutcome = m_dynamoClient->GetItemCallable(getItemRequest);
		item->setType(DatabaseTask::type_t::get);

		return item;
}

DatabaseTask* DynamoDB::putItem(Message*& message) {

    m_message = message;

    std::string key = message->getKey();
		std::string value = message->getValue();

    const Aws::String skey(key.c_str());
    const Aws::String svalue(value.c_str());
    Aws::Utils::ByteBuffer cipherText;

    Aws::DynamoDB::Model::PutItemRequest putItemRequest;
    putItemRequest.SetTableName(m_table.c_str());

		//encrypt(value, cipherText);

    Aws::DynamoDB::Model::AttributeValue akey;
    Aws::DynamoDB::Model::AttributeValue aencrypt;

    akey.SetS(skey);
    //aencrypt.SetB(cipherText);
    aencrypt.SetS(value.c_str());

    putItemRequest.AddItem("key", akey);
    putItemRequest.AddItem("value", aencrypt);

    DatabaseTask* item = new DatabaseTask();
    item->putItemOutcome = m_dynamoClient->PutItemCallable(putItemRequest);
		item->setType(DatabaseTask::type_t::put);

    return item;
}

DatabaseTask::state_t DynamoDB::consume(Message*& message, DatabaseTask*& item) {

		//printf("DB Task Consume\n");

    std::future_status status;

		if (item->getType() == DatabaseTask::type_t::put)
        status = item->putItemOutcome.wait_for(std::chrono::seconds(0));
    else
		    status = item->getItemOutcome.wait_for(std::chrono::seconds(0));

    if (status == std::future_status::deferred) {
        //std::cout << "deferred\n";
        return DatabaseTask::state_t::wait;
    } else if (status == std::future_status::timeout) {
        //std::cout << "timeout\n";
        return DatabaseTask::state_t::wait;
    } else if (status == std::future_status::ready) {
        //std::cout << "ready!\n";
        switch (item->getType()) {
		        case DatabaseTask::type_t::put: {
				        message->setError(msg_error::ok);

				        Aws::DynamoDB::Model::PutItemOutcome putItemOutcome = item->putItemOutcome.get();

                if (!putItemOutcome.IsSuccess()) {
                    std::cout << "ERROR: putItem key:" + message->getKey() << " - " << putItemOutcome.GetError().GetMessage() << std::endl;
								    message->setError(msg_error::fail);
                }
            }
				    break;

				    case DatabaseTask::type_t::get: {
				        msg_error err;
						    err = msg_error::not_found;
						    std::string value;

						    value.clear();

				        Aws::DynamoDB::Model::GetItemOutcome getItemOutcome = item->getItemOutcome.get();

                if (!getItemOutcome.IsSuccess()) {
                    std::cout << "ERROR: getItem key:" + message->getKey() << " - " << getItemOutcome.GetError().GetMessage() << std::endl;
								    message->setError(msg_error::fail);
                }

						    const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& tuple = getItemOutcome.GetResult().GetItem();
						    if (tuple.size() > 0) {
						        err = msg_error::ok;
								    value = tuple.find("value")->second.GetS().c_str();
						    }

						    #if 0
						    Aws::Utils::ByteBuffer cipherText = item.find("value")->second.GetB();
						    decrypt(cipherText, value);
						    #endif
						    message->setValue(value);
						    message->setError(err);
            }
				    break;
        }
    }

    delete item;

		return DatabaseTask::state_t::ready;
}

int DynamoDB::putItemHandle(Message*& message) {
    //std::future<int> resultFromDB = std::async(std::launch::async, DynamoDB::putItem, message);
    return 0;
}

DynamoDB* DynamoDB::instance(AwsClient* awsClient, bool async = true) {
    if (!m_instance) {
        m_instance = new DynamoDB(awsClient, async);
    }

    return m_instance;
}
