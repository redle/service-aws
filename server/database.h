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
    virtual DatabaseTask::state_t handleTask(Message*& message, DatabaseTask*&) = 0;
};

class FooDB: public Database
{
  DatabaseTask* m_queue_item;
  public:
    DatabaseTask* getItem(Message*& message);
    DatabaseTask* putItem(Message*& message);
    void setHandler(MessageHandler* handler) {};
    DatabaseTask::state_t handleTask(DatabaseTask*& task) { return DatabaseTask::state_t::ready; };
};

class DynamoDB: public Database
{
    AwsClient *m_awsClient;
    static std::shared_ptr<Aws::DynamoDB::DynamoDBClient> m_dynamoClient;
    std::string m_table;
    Message* m_message;
    bool m_async;
    static DynamoDB* m_instance;
  public:
    DynamoDB(AwsClient* , bool );
    virtual ~DynamoDB();

    DatabaseTask* getItem(Message*& message);
    DatabaseTask* putItem(Message*& message);

    DatabaseTask::state_t handleTask(Message*& message, DatabaseTask*&);
    static DynamoDB* instance(AwsClient* , bool );
};
#endif
