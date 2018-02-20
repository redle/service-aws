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
    m_clientConfig.endpointOverride = "";
    m_clientConfig.scheme = Aws::Http::Scheme::HTTPS;
    m_clientConfig.connectTimeoutMs = 30000;
    m_clientConfig.requestTimeoutMs = 30000;
    //m_clientConfig.readRateLimiter = Aws::MakeShared<Aws::Utils::RateLimits::DefaultRateLimiter<>>("MCAWSSERVICE", 200000);
    //m_clientConfig.writeRateLimiter = Aws::MakeShared<Aws::Utils::RateLimits::DefaultRateLimiter<>>("MCAWSSERVICE", 200000);
    m_clientConfig.httpLibOverride = Aws::Http::TransferLibType::DEFAULT_CLIENT;
    m_clientConfig.executor = Aws::MakeShared<Aws::Utils::Threading::PooledThreadExecutor>("MCAWSSERVICE", 128);
}

AwsClient* AwsClient::instance() {
    if (!m_instance) {
        m_instance = new AwsClient();
    }

    return m_instance;
}

AwsClient::~AwsClient() {
    Aws::ShutdownAPI(m_options);

    if (m_instance)
        delete m_instance;
}

Aws::Client::ClientConfiguration AwsClient::getClientConfig() {
    return m_clientConfig;
}

