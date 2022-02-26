#include <stdio.h>
#include <math.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

WSADATA wsaData;
char* noiseMethod, * dataBuffer, * IPAddress;
FILE* filePointer;
short channelSenderPort;
struct sockaddr_in senderListenSockAddr, recieverListenSockAddr, senderConnSockAddr, recieverConnSockAddr;
int senderListenSockfd, recieverListenSockfd, senderConnSockfd, recieverConnSockfd;
int retVal = 0, randomSeed = 0, cycleLength = 0, fileLength = 0, bytesRead = 0, bytesWritten = 0, bytesCurrWrite = 0;
int isRandomNoise = 0, numberOfFlippedBits = 0;
double noiseProbability;

void addRandomNoise() {
    srand(randomSeed);
    for (int i = 0; i < sizeof(dataBuffer); i++) {
        double randomDouble = (double)rand() / (double)RAND_MAX;
        int toFlip = randomDouble < noiseProbability;
        if (toFlip > 0) {
            dataBuffer[i] = 1 - dataBuffer[i];
            numberOfFlippedBits++;
        }
    }
}

void addDeterministicNoise() {
    for (int i = cycleLength - 1; i < sizeof(dataBuffer); i += cycleLength) {
        dataBuffer[i] = 1 - dataBuffer[i];
        numberOfFlippedBits++;
    }
}

int main(int argc, char* argv[]) {
    // Parsing arguments and call method functions
    noiseMethod = argv[1];
    if (strcmp(noiseMethod, "-r") == 0) { // Random noise
        sscanf_s(argv[2], "%lf", &noiseProbability);
        noiseProbability /= pow(2, 16);
        sscanf_s(argv[3], "%du", &randomSeed);
        isRandomNoise = 1;
    }
    else if (strcmp(noiseMethod, "-d") == 0) { // Deterministic noise
        sscanf_s(argv[2], "%du", &cycleLength);
    }
    else {
        perror("Not a legal noise flag");
        exit(1);
    }

    // Initializing Winsock
    retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (retVal != NO_ERROR) {
        perror("Error at WSAStartup");
        exit(1);
    }

    // Creating sender socket for listening
    senderListenSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (senderListenSockfd < 0) {
        perror("Can't create socket");
        exit(1);
    }

    // Printing IP and port of sender socket
    int addrLen = sizeof(senderListenSockAddr);
    if (getsockname(senderListenSockfd, (struct sockaddr*)&senderListenSockAddr, &addrLen) == 0) {
        inet_ntop(AF_INET, &senderListenSockAddr.sin_addr, IPAddress, 1024); //TODO change
        printf("sender socket: IP address = %s port = %d\n", IPAddress, ntohs(senderListenSockAddr.sin_port));
    }
    else {
        perror("Error at getsockname of sender socket");
        exit(1);
    }

    // Creating reciever socket for listening
    recieverListenSockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (recieverListenSockfd < 0) {
        perror("Can't create socket");
        exit(1);
    }

    // Printing IP and port of reciever socket
    addrLen = sizeof(recieverListenSockAddr);
    if (getsockname(recieverListenSockfd, (struct sockaddr*)&recieverListenSockAddr, &addrLen) == 0) {
        inet_ntop(AF_INET, &recieverListenSockAddr.sin_addr, IPAddress, 1024); //TODO change
        printf("reciever socket: IP address = %s port = %d\n", IPAddress, ntohs(recieverListenSockAddr.sin_port));
    }
    else {
        perror("Error at getsockname of sender socket");
        exit(1);
    }

    // Listening on sender listening socket
    retVal = listen(senderListenSockfd, 1);
    if (retVal != 0) {
        perror("Listen to sender failed");
        exit(1);
    }

    // Accepting sender connection
    addrLen = sizeof(senderConnSockAddr);
    senderConnSockfd = accept(senderListenSockfd, (struct sockaddr*)&senderConnSockAddr, &addrLen);
    if (senderConnSockfd < 0) {
        perror("Accept sender connection failed");
        exit(1);
    }

    // Listening on reciever listening socket
    retVal = listen(recieverListenSockfd, 1);
    if (retVal != 0) {
        perror("Listen to reciever failed");
        exit(1);
    }

    // Accepting reciever connection
    addrLen = sizeof(recieverConnSockAddr);
    recieverConnSockfd = accept(recieverListenSockfd, (struct sockaddr*)&recieverConnSockAddr, &addrLen);
    if (recieverConnSockfd < 0) {
        perror("Accept reciever connection failed");
        exit(1);
    }

    //////////////
    while (1) { // TODO while sender is not closed
        // TODO read data from socket to buffer
        if (isRandomNoise == 1) {
            addRandomNoise();
        }
        else {
            addDeterministicNoise();
        }
        // TODO writing perturbed data to reciever connection socket
    }
    // TODO add continue

    // Cleaning up Winsock
    retVal = WSACleanup();
    if (retVal != NO_ERROR) {
        perror("Error at WSACleanup");
        exit(1);
    }

    exit(0);
}