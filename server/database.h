#ifndef __database_h__
#define __database_h__

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/core/client/AsyncCallerContext.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/ListTablesRequest.h>
#include <aws/dynamodb/model/ListTablesResult.h>
#include <aws/dynamodb/model/AttributeDefinition.h>
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/PutItemResult.h>
#include <aws/dynamodb/model/GetItemRequest.h>
#include <iostream>
#include <list>
#include <memory>
#include <chrono>

#include <aws/kms/KMSClient.h>
#include <aws/kms/model/EncryptRequest.h>
#include <aws/kms/model/EncryptResult.h>
#include <aws/kms/model/DecryptRequest.h>
#include <aws/kms/model/DecryptResult.h>
#include <aws/kms/KMSErrors.h>

class Database;
class DynamoDB;
class FooDB;
class DatabaseTask;

#include "aws_client.h"
#include "message.h"

class DatabaseTask
{
  public:
		enum type_t {
				put = 0,
				get = 1
		};
    enum state_t {
        wait = 0,
        ready = 1,
        error = 2
    };

    DatabaseTask() {}
    ~DatabaseTask() {}

		type_t getType() { return m_type; }
		void setType(type_t type) { m_type = type; }

    Aws::DynamoDB::Model::PutItemOutcomeCallable putItemOutcome;
		Aws::DynamoDB::Model::GetItemOutcomeCallable getItemOutcome;

	private:
		type_t m_type;
		Message* m_message;

};

class Database
{
  public:
    virtual ~Database() {};
    virtual DatabaseTask* getItem(Message*& ) = 0;
    virtual DatabaseTask* putItem(Message*& ) = 0;
    virtual DatabaseTask::state_t consume(Message*& message, DatabaseTask*&) = 0;
};

class FooDB: public Database
{
  DatabaseTask* m_queue_item;
  public:
    DatabaseTask* getItem(Message*& message);
    DatabaseTask* putItem(Message*& message);
    void setHandler(MessageHandler* handler) {};
    DatabaseTask::state_t consume(DatabaseTask*& item) { return DatabaseTask::state_t::ready; };
};

class DynamoDB: public Database
{
    AwsClient *m_awsClient;
    //Aws::DynamoDB::DynamoDBClient* m_dynamoClient;
    static std::shared_ptr<Aws::DynamoDB::DynamoDBClient> m_dynamoClient;
    Aws::KMS::KMSClient* m_kmsClient;
    std::string m_table;
    std::string m_keyid;

    Message* m_message;

    bool m_async;

    pthread_mutex_t  m_mutex;
    static DynamoDB* m_instance;
  public:
    DynamoDB(AwsClient* , bool );
    virtual ~DynamoDB();

		void encrypt(std::string plainText, Aws::Utils::ByteBuffer& cipherText);
		void decrypt(Aws::Utils::ByteBuffer cipherText, std::string& message);

    DatabaseTask* getItem(Message*& message);
    DatabaseTask* putItem(Message*& message);
    virtual int putItemHandle(Message*& message);

    DatabaseTask::state_t consume(Message*& message, DatabaseTask*&);
    static DynamoDB* instance(AwsClient* , bool );

};
#endif
