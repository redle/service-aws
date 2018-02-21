#ifndef __encrypt_h__
#define __encrypt_h__

#include <iostream>
#include <aws/core/utils/HashingUtils.h>
#include <aws/core/utils/StringUtils.h>
#include <aws/kms/KMSClient.h>
#include <aws/kms/model/EncryptRequest.h>
#include <aws/kms/model/EncryptResult.h>
#include <aws/kms/model/DecryptRequest.h>
#include <aws/kms/model/DecryptResult.h>
#include <aws/kms/KMSErrors.h>

class EncryptTask;
class Encrypt;
class KMSEncrypt;

#include "aws_client.h"
#include "message.h"

class EncryptTask
{
  public:
   enum type_t {
     encrypt = 0,
     decrypt = 1
   };
    enum state_t {
        wait = 0,
        ready = 1,
        error = 2
    };

    EncryptTask() {}
    ~EncryptTask() {}

    type_t getType() { return m_type; }
    void setType(type_t type) { m_type = type; }

    Aws::KMS::Model::EncryptOutcomeCallable encryptOutcome;
    Aws::KMS::Model::DecryptOutcomeCallable decryptOutcome;

  private:
    type_t m_type;
};

class Encrypt {
  public:
    virtual EncryptTask* encrypt(std::string ) = 0;
    virtual EncryptTask* decrypt(std::string ) = 0;
    virtual EncryptTask::state_t handleTask(Message*& , EncryptTask*& ) = 0;
};

class KMSEncrypt : public Encrypt {
    AwsClient* m_awsClient;
    //Aws::KMS::KMSClient* m_kmsClient;
    static std::shared_ptr<Aws::KMS::KMSClient> m_kmsClient;
    std::string m_keyid;
    static KMSEncrypt* m_instance;
  public:
    KMSEncrypt();
    EncryptTask* encrypt(std::string );
    EncryptTask* decrypt(std::string );
    EncryptTask::state_t handleTask(Message*& , EncryptTask*& );
    static KMSEncrypt* instance();
};
#endif
