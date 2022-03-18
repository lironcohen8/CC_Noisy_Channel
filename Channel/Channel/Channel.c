#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <math.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define originalBlockLength 26
#define encodedBlockLength 31
#define extendedBufferLength 208 // 26 bytes * 8 bits per bytes
#pragma comment(lib, "ws2_32.lib")

WSADATA wsaData;
char* noiseMethod, * dataBuffer, * IPAddress, shouldContinue[4], * hostBuffer;
struct sockaddr_in senderListenSockAddr, receiverListenSockAddr, senderConnSockAddr, receiverConnSockAddr;
struct hostent* hostEntry;
int senderListenSockfd, receiverListenSockfd, senderConnSockfd, receiverConnSockfd;
int retVal = 0, randomSeed = 0, cycleLength = 0, finished = 0;
int bitsRead = 0, bitsCurrRead = 1, bitsWritten = 0, bitsCurrWrite = 0, bitsWrittenTotal = 0;
int isRandomNoise = 0, numberOfFlippedBits = 0;
int addrSize = sizeof(struct sockaddr_in);
double noiseProbability;

void parseArguments(char* argv[]) {
    noiseMethod = argv[1];
    if (strcmp(noiseMethod, "-r") == 0) { // Random noise
        sscanf_s(argv[2], "%lf", &noiseProbability);
        noiseProbability /= pow(2, 16);
        sscanf_s(argv[3], "%du", &randomSeed);
        isRandomNoise = 1;
        srand(randomSeed);
    }
    else if (strcmp(noiseMethod, "-d") == 0) { // Deterministic noise
        sscanf_s(argv[2], "%du", &cycleLength);
    }
    else {
        perror("Not a legal noise flag");
        exit(1);
    }
}

void getIPAddress() {
    // Based on answer from: https://www.geeksforgeeks.org/c-program-display-hostname-ip-address/
    hostBuffer = (char*)calloc(1024, sizeof(char));
    if (hostBuffer == NULL) {
        perror("Allocation for hostname failed");
        exit(1);
    }
    IPAddress = (char*)calloc(1024, sizeof(char));
    if (IPAddress == NULL) {
        perror("Allocation for IP failed");
        exit(1);
    }

    retVal = gethostname(hostBuffer, 1024);
    if (retVal != 0) {
        perror("Get hostname failed");
        exit(1);
    }

    hostEntry = gethostbyname(hostBuffer);

    IPAddress = inet_ntoa(*((struct in_addr*)hostEntry->h_addr_list[0]));
}

void initSenderSocket() {
    // Creating sender socket for listening
    senderListenSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (senderListenSockfd < 0) {
        perror("Can't create socket");
        exit(1);
    }

    // Creating sender address struct and getting IP address
    memset(&senderListenSockAddr, 0, addrSize);
    getIPAddress();
    senderListenSockAddr.sin_family = AF_INET;
    inet_pton(AF_INET, (PCSTR)IPAddress, &(senderListenSockAddr.sin_addr.s_addr));
    senderListenSockAddr.sin_port = htons(0); // Will be changed by bind()

    // Binding sender socket to port
    retVal = bind(senderListenSockfd, (struct sockaddr*)&senderListenSockAddr, addrSize);
    if (retVal != 0) {
        perror("Bind failed");
        exit(1);
    }

    // Listening on sender listening socket
    retVal = listen(senderListenSockfd, 1);
    if (retVal != 0) {
        perror("Listen to sender failed");
        exit(1);
    }

    // Printing IP and port of sender socket
    if (getsockname(senderListenSockfd, (struct sockaddr*)&senderListenSockAddr, &addrSize) == 0) {
        printf("sender socket: IP address = %s port = %d\n", IPAddress, ntohs(senderListenSockAddr.sin_port));
    }
    else {
        perror("Error at getsockname of sender socket");
        exit(1);
    }
}

void initReceiverSocket() {
    // Creating receiver socket for listening
    receiverListenSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (receiverListenSockfd < 0) {
        perror("Can't create socket");
        exit(1);
    }

    // Creating receiver address struct and getting IP address
    memset(&receiverListenSockAddr, 0, addrSize);
    receiverListenSockAddr.sin_family = AF_INET;
    inet_pton(AF_INET, (PCSTR)IPAddress, &(receiverListenSockAddr.sin_addr.s_addr));
    receiverListenSockAddr.sin_port = htons(0); // Will be changed by bind()

    // Binding receiver socket to port
    retVal = bind(receiverListenSockfd, (struct sockaddr*)&receiverListenSockAddr, addrSize);
    if (retVal != 0) {
        perror("Bind failed");
        exit(1);
    }

    // Listening on receiver listening socket
    retVal = listen(receiverListenSockfd, 1);
    if (retVal != 0) {
        perror("Listen to receiver failed");
        exit(1);
    }

    // Printing IP and port of receiver socket
    if (getsockname(receiverListenSockfd, (struct sockaddr*)&receiverListenSockAddr, &addrSize) == 0) {
        printf("receiver socket: IP address = %s port = %d\n", IPAddress, ntohs(receiverListenSockAddr.sin_port));
    }
    else {
        perror("Error at getsockname of receiver socket");
        exit(1);
    }
}

void acceptConnections() {
    // Accepting sender connection
    senderConnSockfd = accept(senderListenSockfd, (struct sockaddr*)&senderConnSockAddr, &addrSize);
    if (senderConnSockfd < 0) {
        perror("Accept sender connection failed");
        exit(1);
    }

    // Accepting receiver connection
    receiverConnSockfd = accept(receiverListenSockfd, (struct sockaddr*)&receiverConnSockAddr, &addrSize);
    if (receiverConnSockfd < 0) {
        perror("Accept receiver connection failed");
        exit(1);
    }
}

void createBuffer() {
    // Creating buffer for data - 31 bytes (bit chars)
    dataBuffer = (char*)calloc(encodedBlockLength, sizeof(char));
    if (dataBuffer == NULL) {
        perror("Can't allocate memory for buffer");
        exit(1);
    }
}

void readOriginalDataFromSocket() {
    // Reading block from socket
    bitsRead = 0;
    bitsCurrRead = 1;
    while (bitsRead < encodedBlockLength) {
        bitsCurrRead = recv(senderConnSockfd, *((&dataBuffer) + bitsRead), encodedBlockLength - bitsRead, 0);
        bitsRead += bitsCurrRead;
        if (bitsCurrRead == 0) {
            finished = 1;
            break;
        }
    }
    if (finished == 0) {
        if (bitsCurrRead < 0 || bitsRead != encodedBlockLength) { // There was an error
            perror("Couldn't read encoded block from socket");
            exit(1);
        }
    }
}

char flipBit(char bit) {
    if (bit == '0')
        return '1';
    else
        return '0';
}

void addRandomNoise() {
    // Adding random noise using the noise probabilty from cmd args.
    for (int i = 0; i < encodedBlockLength; i++) {
        double randomDouble = (double)rand() / (double)RAND_MAX;
        int toFlip = randomDouble < noiseProbability;
        if (toFlip > 0) {
            dataBuffer[i] = flipBit(dataBuffer[i]);
            numberOfFlippedBits++;
        }
    }
}

void addDeterministicNoise() {
    // Adding deterministic noise using the cycle length from cmd args.
    for (int i = cycleLength - 1; i < encodedBlockLength; i += cycleLength) {
        dataBuffer[i] = flipBit(dataBuffer[i]);
        numberOfFlippedBits++;
    }
}

void writeNoisedDataToSocket() {
    // Writing encoded block to channel socket
    bitsWritten = 0;
    bitsCurrWrite = 1;
    while (bitsWritten < encodedBlockLength) {
        bitsCurrWrite = send(receiverConnSockfd, *((&dataBuffer) + bitsWritten), encodedBlockLength - bitsWritten, 0);
        bitsWritten += bitsCurrWrite;
    }
    if (bitsCurrWrite < 0 || bitsWritten != encodedBlockLength) { // There was an error
        perror("Couldn't write noised block to socket");
        exit(1);
    }
    bitsWrittenTotal += bitsWritten;
}

int main(int argc, char* argv[]) {
    // Parsing arguments according to selected noise method
    parseArguments(argv);

    // Initializing Winsock
    retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (retVal != 0) {
        perror("Error at WSAStartup");
        exit(1);
    }

    // Initializing listen sockets
    initSenderSocket();
    initReceiverSocket();

    do {
        // Accepting sender and receiver Connections
        acceptConnections();

        // Creating buffer for data
        createBuffer();

        // Reading data from socket and adding noise
        while (1) {
            readOriginalDataFromSocket();
            if (finished == 1) {
                break;
            }
            else if (isRandomNoise == 1) {
                addRandomNoise();
            }
            else {
                addDeterministicNoise();
            }
            writeNoisedDataToSocket();
        }

        // closing connections sockets
        closesocket(senderConnSockfd);
        closesocket(receiverConnSockfd);

        // Printing message
        printf("retransmitted %d bytes, flipped %d bits\n", bitsWrittenTotal / 8, numberOfFlippedBits);
        printf("continue? (yes/no)\n");

        // Getting user's answer and initializing parameters
        retVal = scanf("%s", shouldContinue);
        finished = 0;
        bitsWrittenTotal = 0;
        numberOfFlippedBits = 0;

    } while (strcmp(shouldContinue, "yes") == 0); // Continue as long as the user wants to

    // Cleaning up Winsock
    retVal = WSACleanup();
    if (retVal != 0) {
        perror("Error at WSACleanup");
        exit(1);
    }

    exit(0);
}