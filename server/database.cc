#include <future>
#include "database.h"

DynamoDB* DynamoDB::m_instance = 0;
std::shared_ptr<Aws::DynamoDB::DynamoDBClient> DynamoDB::m_dynamoClient = 0;

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

DynamoDB::DynamoDB(AwsClient* awsClient, bool async = true) {
    m_awsClient = awsClient;
    m_table = "data";
    m_async = async;
    //m_dynamoClient = new Aws::DynamoDB::DynamoDBClient(m_awsClient->getClientConfig());
    m_dynamoClient = Aws::MakeShared<Aws::DynamoDB::DynamoDBClient>("MCAWSSERVICE", m_awsClient->getClientConfig());
}

DynamoDB::~DynamoDB() {
    //delete m_dynamoClient;
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

    DatabaseTask* task = new DatabaseTask();
    task->getItemOutcome = m_dynamoClient->GetItemCallable(getItemRequest);
    task->setType(DatabaseTask::type_t::get);

    return task;
}

DatabaseTask* DynamoDB::putItem(Message*& message) {

    m_message = message;

    std::string key = message->getKey();
    std::string value = message->getValue();

    const Aws::String skey(key.c_str());
    const Aws::String svalue(value.c_str());

    Aws::DynamoDB::Model::PutItemRequest putItemRequest;
    putItemRequest.SetTableName(m_table.c_str());

    Aws::DynamoDB::Model::AttributeValue akey;
    Aws::DynamoDB::Model::AttributeValue avalue;

    akey.SetS(skey);
    avalue.SetS(value.c_str());

    putItemRequest.AddItem("key", akey);
    putItemRequest.AddItem("value", avalue);

    DatabaseTask* task = new DatabaseTask();
    task->putItemOutcome = m_dynamoClient->PutItemCallable(putItemRequest);
    task->setType(DatabaseTask::type_t::put);

    return task;
}

DatabaseTask::state_t DynamoDB::handleTask(Message*& message, DatabaseTask*& task) {
  //printf("DB Task Consume\n");
    std::future_status status;

    if (task->getType() == DatabaseTask::type_t::put)
      status = task->putItemOutcome.wait_for(std::chrono::seconds(0));
    else
      status = task->getItemOutcome.wait_for(std::chrono::seconds(0));

    if (status == std::future_status::deferred) {
        //std::cout << "deferred\n";
        return DatabaseTask::state_t::wait;
    } else if (status == std::future_status::timeout) {
        //std::cout << "timeout\n";
        return DatabaseTask::state_t::wait;
    } else if (status == std::future_status::ready) {
        //std::cout << "ready!\n";
        switch (task->getType()) {
          case DatabaseTask::type_t::put: {
            message->setError(msg_error::ok);

            Aws::DynamoDB::Model::PutItemOutcome putItemOutcome = task->putItemOutcome.get();

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

            Aws::DynamoDB::Model::GetItemOutcome getItemOutcome = task->getItemOutcome.get();

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

    delete task;

    return DatabaseTask::state_t::ready;
}

DynamoDB* DynamoDB::instance(AwsClient* awsClient, bool async = true) {
    if (!m_instance) {
        m_instance = new DynamoDB(awsClient, async);
    }

    return m_instance;
}
