#include "aws_client.h"

AwsClient* AwsClient::m_instance = 0;

AwsClient::AwsClient(std::string region): m_region(region.c_str()) {
    Aws::InitAPI(m_options);
    m_clientConfig.region = m_region;
}

AwsClient::AwsClient() {
    Aws::InitAPI(m_options);
    char *env_region = getenv("AWS_DEFAULT_REGION");
    m_region = env_region;
    m_clientConfig.region = m_region;
}

AwsClient* AwsClient::instance() {
    if (!m_instance) {
        m_instance = new AwsClient();
    }

    return m_instance;
}

AwsClient::~AwsClient() {
    Aws::ShutdownAPI(m_options);
}

Aws::Client::ClientConfiguration AwsClient::getClientConfig() {
    return m_clientConfig;
}

