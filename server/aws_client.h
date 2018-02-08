#ifndef __aws_client_h__
#define __aws_client_h__

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

class AwsClient
{
    Aws::Client::ClientConfiguration m_clientConfig;
    Aws::String m_region;
    Aws::SDKOptions m_options;
  public:
    AwsClient(std::string region);
    AwsClient();
    ~AwsClient();
    Aws::Client::ClientConfiguration getClientConfig();
};
#endif
