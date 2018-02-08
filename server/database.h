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

#include <aws/kms/KMSClient.h>
#include <aws/kms/model/EncryptRequest.h>
#include <aws/kms/model/EncryptResult.h>
#include <aws/kms/model/DecryptRequest.h>
#include <aws/kms/model/DecryptResult.h>
#include <aws/kms/KMSErrors.h>

class Database;
class DynamoDB;
class FooDB;

#include "aws_client.h"
#include "message.h"

class Database
{
  public:
    virtual int getItem(Message*& message) = 0;
    virtual int putItem(Message*& message) = 0;
};

class FooDB: public Database
{
  public:
    int getItem(Message*& message);
    int putItem(Message*& message);
};

class DynamoDB: public Database
{
    AwsClient *m_awsClient;
    Aws::DynamoDB::DynamoDBClient* m_dynamoClient;
    Aws::KMS::KMSClient* m_kmsClient;
    std::string m_table;
    std::string m_keyid;
  public:
    DynamoDB(AwsClient* awsClient);
    ~DynamoDB();

		void encrypt(std::string plainText, Aws::Utils::ByteBuffer& cipherText);
		void decrypt(Aws::Utils::ByteBuffer cipherText, std::string& message);

    int getItem(Message*& message);
    int putItem(Message*& message);

    void PutItemOutcomeReceived(const Aws::DynamoDB::DynamoDBClient* sender,
                                const Aws::DynamoDB::Model::PutItemRequest& request,
                                const Aws::DynamoDB::Model::PutItemOutcome& outcome,
                                const std::shared_ptr<const  Aws::Client::AsyncCallerContext>& context);
    void GetItemOutcomeReceived(const Aws::DynamoDB::DynamoDBClient* sender,
                                const Aws::DynamoDB::Model::GetItemRequest& request,
                                const Aws::DynamoDB::Model::GetItemOutcome& outcome,
                                const std::shared_ptr<const  Aws::Client::AsyncCallerContext>& context);
};
#endif
