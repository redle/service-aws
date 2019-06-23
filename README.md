# README #

There is a Cloud Formation Template onto folder CloudFormation, 
that can used to create all infrastructure AWS demanded to run 
the service. 

This template install every package required and download service 
and AWS libs onto folder home of user admin (/home/admin). Besides, 
the file .bash_profile is populate with enviroment variable that 
are required to run properly the service.

Sometimes the Cloud Formation creation is failed the get of package 
from S3, because the DNS is not aplied in /etc/resolv.conf.

Tarball with files: 
https://s3-sa-east-1.amazonaws.com/cf-templates-1xfihe3xbn354-sa-east-1/service-0.0.1.tar

The following environment variable are required:
   - AWS_ACCESS_KEY_ID 
   - AWS_SECRET_ACCESS_KEY 
   - AWS_ARN_ENCRYPT_KEY
   - AWS_REGION

These values are request during process of CloudFormation.

After completed CloudFormation creation, onto /home/admin all files 
required to execute the service (service, client_test, libs aws). See 
instructions on topic "Running".

### Running ###
  Service:
     DEBUG_MODE=1 ./service 8000
     ./service 8000

  Client Test:
    ./client_test -h <HOST IP> -s -k 1 -v "VALUE"
    ./client_test -h <HOST IP> -g -k 1 
    ./client_test -h <HOST IP> -t

  Creating many clients:
    ./create_cli <IP AWS> <number of clients>
    ./create_cli 52.67.158.82 100

### Requirements ###
  Linux Debian Stretch 9.3
  AWS SDK
  Google Protobuff

### Preparing enviroment ####
apt-get update
apt-get upgrade
apt-get install cmake build-essential git
apt-get install zlib1g-dev libssl-dev libcurl4-openssl-dev libprotobuf-dev

* Install AWS SDK
git clone https://github.com/aws/aws-sdk-cpp.git

mkdir aws-sdk-build
cd aws-sdk-build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_ONLY="ec2;dynamodb;kms" ../aws-sdk-cpp
make -j 4
make install


### Compiling Service AWS ###
# Get code
 git clone https://redle@bitbucket.org/redle/service-aws.git

* Compile service and client_test
make

* Send binary service lib to AWS instance:

* Copy following libs from PC to AWS Instance and put inside libs/ in home directory 
   /usr/local/lib/libaws-cpp-sdk-core.so
   /usr/local/lib/libaws-cpp-sdk-dynamodb.so
   /usr/local/lib/libaws-cpp-sdk-ec2.so
   /usr/local/lib/libaws-cpp-sdk-kms.so

# AWS Server machine:
apt-get install libcurl3 libprotobuf-dev

The following enviroment variables are necessary to run the service:
    export AWS_ACCESS_KEY_ID=
    export AWS_SECRET_ACCESS_KEY=
    export AWS_DEFAULT_REGION=
    export AWS_ARN_ENCRYPT_KEY=<ARN KMS ENCRYPT KEY>
    export LD_LIBRARY_PATH=lib/

Create file .bash_profile and inside file fill with properly values.

