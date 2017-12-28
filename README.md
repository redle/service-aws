# README #

### Requirements ###
  - Debian 9.3
  - AWS SDK
  - Google Protobuff

### Compile ###
  make

### Running ###
 * The following enviroment variables are necessary to run the service:
    export AWS_ACCESS_KEY_ID=
    export AWS_SECRET_ACCESS_KEY=
    export AWS_DEFAULT_REGION=
    export AWS_ARN_ENCRYPT_KEY=<ARN KMS ENCRYPT KEY>


  * Service:
    ./service 8000

  * Client Test:
    ./client_test -h 52.67.158.82 -s -k 1 -v "VALUE"
    ./client_test -h 52.67.158.82 -s -g 1 

  * Creating many clients:
    ./create_cli <IP AWS> <number of clients>
    ./create_cli 52.67.158.82 100
