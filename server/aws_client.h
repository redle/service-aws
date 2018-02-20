#ifndef __aws_client_h__
#define __aws_client_h__

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/core/client/AsyncCallerContext.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/client/CoreErrors.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/http/HttpTypes.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/core/utils/memory/AWSMemory.h>
#include <aws/core/utils/UnreferencedParam.h>
#include <aws/core/utils/Outcome.h>
#include <aws/core/utils/memory/stl/AWSSet.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>
#include <aws/core/utils/ratelimiter/DefaultRateLimiter.h>
#include <aws/core/utils/threading/Executor.h>

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
    static AwsClient* m_instance;
  public:
    AwsClient(std::string region);
    AwsClient();
    ~AwsClient();
    static AwsClient* instance();
    Aws::Client::ClientConfiguration getClientConfig();
};

#endif
