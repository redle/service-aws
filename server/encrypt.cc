#include "encrypt.h"

KMSEncrypt* KMSEncrypt::m_instance = 0;
std::shared_ptr<Aws::KMS::KMSClient> KMSEncrypt::m_kmsClient = 0;

KMSEncrypt::KMSEncrypt() {
    char *env_arnkey = getenv("AWS_ARN_ENCRYPT_KEY");
    m_keyid = env_arnkey;

    m_awsClient = new AwsClient();
    m_kmsClient = Aws::MakeShared<Aws::KMS::KMSClient>("MCAWSSERVICE", m_awsClient->getClientConfig());
}

EncryptTask* KMSEncrypt::encrypt(std::string plainText) {
    Aws::Utils::ByteBuffer cipherText;
    Aws::Utils::ByteBuffer message((unsigned char*) plainText.c_str(), plainText.length());

    Aws::KMS::Model::EncryptRequest encryptRequest;
    encryptRequest.SetKeyId(m_keyid.c_str());
    encryptRequest.SetPlaintext(message);

    EncryptTask* task = new EncryptTask();
    task->encryptOutcome = m_kmsClient->EncryptCallable(encryptRequest);
    task->setType(EncryptTask::type_t::encrypt);

    return task;
}

EncryptTask* KMSEncrypt::decrypt(std::string message) {
    Aws::Utils::ByteBuffer cipherText;
    cipherText = Aws::Utils::HashingUtils::Base64Decode(message.c_str());

    Aws::KMS::Model::DecryptRequest decryptRequest;
    decryptRequest.SetCiphertextBlob(cipherText);

    EncryptTask* task = new EncryptTask();
    task->decryptOutcome = m_kmsClient->DecryptCallable(decryptRequest);
    task->setType(EncryptTask::type_t::decrypt);

    return task;
}

EncryptTask::state_t KMSEncrypt::handleTask(Message*& message, EncryptTask*& task) {
  //printf("DB Task Consume\n");
    std::future_status status;

    if (task->getType() == EncryptTask::type_t::encrypt)
      status = task->encryptOutcome.wait_for(std::chrono::seconds(0));
    else
      status = task->decryptOutcome.wait_for(std::chrono::seconds(0));

    if (status == std::future_status::deferred) {
        //std::cout << "deferred\n";
        return EncryptTask::state_t::wait;
    } else if (status == std::future_status::timeout) {
        //std::cout << "timeout\n";
        return EncryptTask::state_t::wait;
    } else if (status == std::future_status::ready) {
        //std::cout << "ready!\n";
        switch (task->getType()) {
          case EncryptTask::type_t::encrypt: {
            message->setError(msg_error::ok);

            Aws::KMS::Model::EncryptOutcome encryptOutcome = task->encryptOutcome.get();

            if (!encryptOutcome.IsSuccess()) {
              std::cout << "ERR: encrypt key:" + message->getKey() << " - " << encryptOutcome.GetError().GetMessage() << std::endl;
              message->setError(msg_error::fail);
            }

            Aws::KMS::Model::EncryptResult result = encryptOutcome.GetResult();
            Aws::Utils::ByteBuffer cipherBlob = result.GetCiphertextBlob();
            Aws::String cipherText = Aws::Utils::HashingUtils::Base64Encode(cipherBlob);

            message->setValue(cipherText.c_str());
          }
          break;

          case EncryptTask::type_t::decrypt: {
            Aws::KMS::Model::DecryptOutcome decryptOutcome = task->decryptOutcome.get();
            if (!decryptOutcome.IsSuccess()) {
              std::cout << "ERR: decrypt key:" + message->getKey() << " - " << decryptOutcome.GetError().GetMessage() << std::endl;
              message->setError(msg_error::fail);
            }


            Aws::KMS::Model::DecryptResult result = decryptOutcome.GetResult();
            Aws::Utils::ByteBuffer plaintext = result.GetPlaintext();

            //I didnt find a better way to do this!!!!
            char msg[plaintext.GetLength() + 1];
            for (unsigned i = 0; i < plaintext.GetLength(); ++i) {
                msg[i] = (char) plaintext[i];
            }
            msg[plaintext.GetLength()] = '\0';

            message->setError(msg_error::ok);
            message->setValue(msg);
          }
          break;
        }
    }
    delete task;

    return EncryptTask::state_t::ready;
}

KMSEncrypt* KMSEncrypt::instance() {
    if (!m_instance) {
        m_instance = new KMSEncrypt();
    }

    return m_instance;
}
