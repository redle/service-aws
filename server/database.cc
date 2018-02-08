#include "database.h"

int FooDB::getItem(Message*& message) {
    message->getData().setValue("TEST FIXED\n");
    return 0;
}

int FooDB::putItem(Message*& message) {
    return 0;
}

DynamoDB::DynamoDB(AwsClient* awsClient) {
    m_awsClient = awsClient;
    m_table = "data";
    char *env_arnkey = getenv("AWS_ARN_ENCRYPT_KEY");
    m_keyid = env_arnkey;
    m_dynamoClient = new Aws::DynamoDB::DynamoDBClient(m_awsClient->getClientConfig());
    m_kmsClient =  new Aws::KMS::KMSClient(m_awsClient->getClientConfig());


    auto getItemHandler = std::bind(&DynamoDB::GetItemOutcomeReceived, this, std::placeholders::_1, std::placeholders::_2,
												        std::placeholders::_3, std::placeholders::_4);

}

DynamoDB::~DynamoDB() {
    delete m_dynamoClient;
    delete m_kmsClient;
}

void DynamoDB::PutItemOutcomeReceived(const Aws::DynamoDB::DynamoDBClient* sender,
                                      const Aws::DynamoDB::Model::PutItemRequest& request,
                                      const Aws::DynamoDB::Model::PutItemOutcome& outcome,
                                      const std::shared_ptr<const  Aws::Client::AsyncCallerContext>& context) {
    AWS_UNREFERENCED_PARAM(sender);
    AWS_UNREFERENCED_PARAM(request);
    AWS_UNREFERENCED_PARAM(context);

    //putItemResultsFromCallbackTest.push_back(outcome);
		printf(">>>>>> PutItemOutcomeReceived\n");
}

void DynamoDB::GetItemOutcomeReceived(const Aws::DynamoDB::DynamoDBClient* sender,
                            const Aws::DynamoDB::Model::GetItemRequest& request,
                            const Aws::DynamoDB::Model::GetItemOutcome& outcome,
                            const std::shared_ptr<const  Aws::Client::AsyncCallerContext>& context) {
    AWS_UNREFERENCED_PARAM(sender);
    AWS_UNREFERENCED_PARAM(request);
    AWS_UNREFERENCED_PARAM(context);

    //getItemResultsFromCallbackTest.push_back(outcome);
}

void DynamoDB::encrypt(std::string plainText, Aws::Utils::ByteBuffer& cipherText) {
#if 0
		printf(">>>> (%li)", plainText.length());
    for (unsigned i = 0; i < plainText.length(); ++i) {
				printf("%02x ", (unsigned char) plainText[i]);
		}
		printf("\n");
#endif
		Aws::Utils::ByteBuffer message((unsigned char*) plainText.c_str(), plainText.length());

    Aws::KMS::Model::EncryptRequest encryptRequest;
    encryptRequest.SetKeyId(m_keyid.c_str());
    encryptRequest.SetPlaintext(message);

    Aws::KMS::Model::EncryptOutcome output;
    output = m_kmsClient->Encrypt(encryptRequest);

    Aws::KMS::Model::EncryptResult result = output.GetResult();

    cipherText = result.GetCiphertextBlob();

#if 0
		std::cout << "Error:" << output.GetError().GetMessage() << endl;
	  printf(">>>> (%li)", cipherText.GetLength());
		for (unsigned i = 0; i < cipherText.GetLength(); ++i) {
				printf("%02x <%c> ", (unsigned char) cipherText[i], (char) cipherText[i]);
		}
		printf("\n");
#endif
}

void DynamoDB::decrypt(Aws::Utils::ByteBuffer cipherText, std::string& message) {
    Aws::KMS::Model::DecryptRequest decryptRequest;
    decryptRequest.SetCiphertextBlob(cipherText);

    Aws::KMS::Model::DecryptOutcome decrypt_output;
    decrypt_output = m_kmsClient->Decrypt(decryptRequest);
    //std::cout << "Error:" << decrypt_output.GetError().GetMessage() << endl;

    Aws::Utils::ByteBuffer plaintext;
    Aws::KMS::Model::DecryptResult result_decrypt = decrypt_output.GetResult();

    plaintext = result_decrypt.GetPlaintext();

		//I didnt find a better way to do this!!!!
		char msg[plaintext.GetLength() + 1];
		for (unsigned i = 0; i < plaintext.GetLength(); ++i) {
				msg[i] = (char) plaintext[i];
		}
		msg[plaintext.GetLength()] = '\0';

		message = msg;
}

int DynamoDB::getItem(Message*& message) {
    std::string key = message->getData().getKey();
		std::string value = message->getData().getValue();

    Aws::DynamoDB::Model::GetItemRequest req;
    Aws::DynamoDB::Model::AttributeValue haskKey;

    haskKey.SetS(key.c_str());

    req.AddKey("key", haskKey);
    req.SetTableName(m_table.c_str());

    value.clear();

    const Aws::DynamoDB::Model::GetItemOutcome& result = m_dynamoClient->GetItem(req);
    if (!result.IsSuccess()) {
        //std::cout << "Failed to get item: " << result.GetError().GetMessage() << endl;
        return -1;
    }

    const Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& item = result.GetResult().GetItem();
    if (item.size() == 0) {
        //std::cout << "No item found with the key " << key << std::endl;
        return 1;
    }
#if 0
		Aws::Utils::ByteBuffer cipherText = item.find("value")->second.GetB();
		decrypt(cipherText, value);
#endif
		//value = item.find("value")->second.GetS();
    return 0;
}

int DynamoDB::putItem(Message*& message) {

    std::string key = message->getData().getKey();
		std::string value = message->getData().getValue();

    const Aws::String skey(key.c_str());
    const Aws::String svalue(value.c_str());
    Aws::Utils::ByteBuffer cipherText;

    Aws::DynamoDB::Model::PutItemRequest pir;
    pir.SetTableName(m_table.c_str());

		//encrypt(value, cipherText);

    Aws::DynamoDB::Model::AttributeValue akey;
    Aws::DynamoDB::Model::AttributeValue aencrypt;

    akey.SetS(skey);
    //aencrypt.SetB(cipherText);
    aencrypt.SetS(value.c_str());

    pir.AddItem("key", akey);
    pir.AddItem("value", aencrypt);

    auto putItemHandler = std::bind(&DynamoDB::PutItemOutcomeReceived, this, std::placeholders::_1, std::placeholders::_2,
																std::placeholders::_3, std::placeholders::_4);

    m_dynamoClient->PutItemAsync(pir, putItemHandler);
#if 0
    const Aws::DynamoDB::Model::PutItemOutcome result = m_dynamoClient->PutItem(pir);
    if (!result.IsSuccess()) {
        std::cout << result.GetError().GetMessage() << std::endl;
        return -1;
    }
#endif
    //std::cout << "put item done!" << std::endl;

    return 0;
}
