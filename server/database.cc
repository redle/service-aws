#include <future>
#include "database.h"

int FooDB::getItem(Message*& message) {
    message->setValue("TEST FIXED\n");
    message->setError(msg_error::ok);
    message->setState(Message::state_t::ready);
    return 0;
}

int FooDB::putItem(Message*& message) {
    message->setError(msg_error::ok);
    message->setState(Message::state_t::ready);
    return 0;
}

DynamoDB::DynamoDB(AwsClient* awsClient, bool async = true) {
    m_awsClient = awsClient;
    m_table = "data";
    m_async = async;
    char *env_arnkey = getenv("AWS_ARN_ENCRYPT_KEY");
    m_keyid = env_arnkey;
    m_dynamoClient = new Aws::DynamoDB::DynamoDBClient(m_awsClient->getClientConfig());
    //m_kmsClient =  new Aws::KMS::KMSClient(m_awsClient->getClientConfig());

    pthread_mutex_init(&m_mutex, NULL);
}

DynamoDB::~DynamoDB() {
    delete m_dynamoClient;
    //delete m_kmsClient;
}

void DynamoDB::PutItemOutcomeReceived(const Aws::DynamoDB::DynamoDBClient* sender,
                                      const Aws::DynamoDB::Model::PutItemRequest& request,
                                      const Aws::DynamoDB::Model::PutItemOutcome& outcome,
                                      const std::shared_ptr<const  Aws::Client::AsyncCallerContext>& context) {
    AWS_UNREFERENCED_PARAM(sender);
    AWS_UNREFERENCED_PARAM(request);
    AWS_UNREFERENCED_PARAM(context);

    msg_error err;

    err = msg_error::ok;

    if (!outcome.IsSuccess()) {
        std::cout << outcome.GetError().GetMessage() << std::endl;
        err = msg_error::fail;
    }

    std::cout << "ASYNC: put item done!" << std::endl;

		printf(">>>>>> PutItemOutcomeReceived\n");

    m_message->setError(err);
    m_message->setState(Message::state_t::ready);
}

void DynamoDB::GetItemOutcomeReceived(const Aws::DynamoDB::DynamoDBClient* sender,
                            const Aws::DynamoDB::Model::GetItemRequest& request,
                            const Aws::DynamoDB::Model::GetItemOutcome& outcome,
                            const std::shared_ptr<const  Aws::Client::AsyncCallerContext>& context) {
    msg_error err;

    AWS_UNREFERENCED_PARAM(sender);
    AWS_UNREFERENCED_PARAM(request);
    AWS_UNREFERENCED_PARAM(context);

    err = msg_error::not_found;
    std::string value;

    value.clear();

    const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& item = outcome.GetResult().GetItem();
    if (item.size() > 0) {
        err = msg_error::ok;
		    value = item.find("value")->second.GetS().c_str();
    }

#if 0
		Aws::Utils::ByteBuffer cipherText = item.find("value")->second.GetB();
		decrypt(cipherText, value);
#endif

    m_message->setValue(value);
    m_message->setError(err);
    m_message->setState(Message::state_t::ready);
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

int DynamoDB::getItem(Message*& message) {
    m_message = message;
    std::string key = message->getKey();
		std::string value;

    Aws::DynamoDB::Model::GetItemRequest getItemRequest;
    Aws::DynamoDB::Model::AttributeValue haskKey;

    haskKey.SetS(key.c_str());

    getItemRequest.AddKey("key", haskKey);
    getItemRequest.SetTableName(m_table.c_str());

#if 0
    if (m_async) {
        auto getItemHandler = std::bind(&DynamoDB::GetItemOutcomeReceived, this, std::placeholders::_1, std::placeholders::_2,
                                        std::placeholders::_3, std::placeholders::_4);
        m_dynamoClient->GetItemAsync(getItemRequest, getItemHandler);

        return 0;
    }
#endif

    QueueItem* item = new QueueItem();
    item->setMessage(message);

    item->getItemOutcome = m_dynamoClient->GetItemCallable(getItemRequest);
		item->setType(QueueItem::type_t::get);

    pthread_mutex_lock(&m_mutex);
    m_queue.push_back(item);
    pthread_mutex_unlock(&m_mutex);

		return 0;
#if 0
    /* Sync */
    value.clear();
    const Aws::DynamoDB::Model::GetItemOutcome& result = m_dynamoClient->GetItem(getItemRequest);
    if (!result.IsSuccess()) {
        //std::cout << "Failed to get item: " << result.GetError().GetMessage() << endl;
        return -1;
    }

    const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& item = result.GetResult().GetItem();
    if (item.size() == 0) {
        //std::cout << "No item found with the key " << key << std::endl;
        return 1;
    }
#endif

#if 0
		Aws::Utils::ByteBuffer cipherText = item.find("value")->second.GetB();
		decrypt(cipherText, value);
#endif

#if 0
		value = item.find("value")->second.GetS().c_str();
		message->setValue(value.c_str());
#endif

    return 0;
}

int DynamoDB::putItem(Message*& message) {

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

    QueueItem* item = new QueueItem();
    item->setMessage(message);

    item->putItemOutcome = m_dynamoClient->PutItemCallable(putItemRequest);
		item->setType(QueueItem::type_t::put);

    pthread_mutex_lock(&m_mutex);
    m_queue.push_back(item);
    pthread_mutex_unlock(&m_mutex);

    return 0;

#if 0
    if (m_async) {
        auto putItemHandler = std::bind(&DynamoDB::PutItemOutcomeReceived, this, std::placeholders::_1, std::placeholders::_2,
														            std::placeholders::_3, std::placeholders::_4);

        m_dynamoClient->PutItemAsync(putItemRequest, putItemHandler);

        return 0;
    }
#endif

    /* Sync */
    const Aws::DynamoDB::Model::PutItemOutcome result = m_dynamoClient->PutItem(putItemRequest);
    if (!result.IsSuccess()) {
        std::cout << result.GetError().GetMessage() << std::endl;
        return -1;
    }
    std::cout << "SYNC: put item done!" << std::endl;

    return 0;
}

void DynamoDB::consume() {

		//printf("Queue consume size:%li\n", m_queue.size());

    if (m_queue.size() == 0) {
        return;
    }

    QueueItem* item;

    Aws::List<QueueItem*>::iterator it = m_queue.begin();

    while ( it != m_queue.end()) {
        //pthread_mutex_lock(&m_mutex);
        item = *it;
//        item = m_queue.front();
//        m_queue.pop_front();
        //item = m_queue.front();
        //m_queue.pop_front();
        //pthread_mutex_unlock(&m_mutex);

        std::future_status status;

		    if (item->getType() == QueueItem::type_t::put)
				    status = item->putItemOutcome.wait_for(std::chrono::seconds(0));
		    else
				    status = item->getItemOutcome.wait_for(std::chrono::seconds(0));

        if (status == std::future_status::deferred) {
            //std::cout << "deferred\n";
				    //m_queue.push_back(item);
            //break;
            continue;
        } else if (status == std::future_status::timeout) {
            //std::cout << "timeout\n";
				    //m_queue.push_back(item);
            //break;
            continue;
        } else if (status == std::future_status::ready) {
            //std::cout << "ready!\n";

				    switch (item->getType()) {
						    case QueueItem::type_t::put: {
								    item->getMessage()->setError(msg_error::ok);
								    item->getMessage()->setState(Message::state_t::ready);

				            Aws::DynamoDB::Model::PutItemOutcome putItemOutcome = item->putItemOutcome.get();

                    if (!putItemOutcome.IsSuccess()) {
                        std::cout << "ERROR: putItem key:" + item->getMessage()->getKey() << " - " << putItemOutcome.GetError().GetMessage() << std::endl;
								        item->getMessage()->setError(msg_error::fail);
                    }
                }
						    break;

						    case QueueItem::type_t::get: {
								    msg_error err;
								    err = msg_error::not_found;
						        std::string value;

						        value.clear();

				            Aws::DynamoDB::Model::GetItemOutcome getItemOutcome = item->getItemOutcome.get();

                    if (!getItemOutcome.IsSuccess()) {
                        std::cout << "ERROR: getItem key:" + item->getMessage()->getKey() << " - " << getItemOutcome.GetError().GetMessage() << std::endl;
								        item->getMessage()->setError(msg_error::fail);
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

						        item->getMessage()->setValue(value);
						        item->getMessage()->setError(err);
						        item->getMessage()->setState(Message::state_t::ready);
                }
						    break;
				    }
            m_queue.erase(it++);
				    delete item;
//            continue;
          break;
        }
        ++it;
    }
		return;

    msg_error err;
    err = msg_error::ok;
#if 0
    if (!outcome.IsSuccess()) {
        std::cout << outcome.GetError().GetMessage() << std::endl;
        err = msg_error::fail;
    }

    std::cout << "ASYNC: put item done!" << std::endl;

		printf(">>>>>> PutItemOutcomeReceived\n");
#endif
    m_message->setError(err);
    m_message->setState(Message::state_t::ready);

}

int DynamoDB::putItemHandle(Message*& message) {
    //std::future<int> resultFromDB = std::async(std::launch::async, DynamoDB::putItem, message);
    return 0;
}
