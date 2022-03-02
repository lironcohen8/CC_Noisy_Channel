#include <stdio.h>
#include "winsock2.h"
#include "ws2tcpip.h"
#define originalBlockLength 26
#define encodedBlockLength 31
#define extendedBufferLength 208 // 26 bytes * 8 bits per bytes
#pragma comment(lib, "ws2_32.lib")

WSADATA wsaData;
char* fileName, * channelSenderIPString, * encodedBitsFileBuffer, * decodedBitsFileBuffer, * rawBytesFileBuffer;
FILE* filePointer;
short channelSenderPort;
struct sockaddr_in channelAddr;
int sockfd, retVal, bytesWritten = 0, bytesWrittenTotal = 0, bitsRead = 0, bitsCurrRead = 0, bitsCorrected = 0, bitsReadTotal = 0;

void connectToSocket() {
    // Creating socket 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Can't create socket");
        exit(1);
    }

    // Creating channel address struct
    channelAddr.sin_family = AF_INET;
    inet_pton(AF_INET, (PCSTR)channelSenderIPString, &(channelAddr.sin_addr.s_addr));
    /*channelAddr.sin_addr.s_addr = inet_addr(channelSenderIPString);
    if (channelAddr.sin_addr.s_addr == INADDR_NONE) {
        perror("Can't convert channel IP string to long");
        exit(1);
    }*/
    channelAddr.sin_port = htons(channelSenderPort);

    // Connecting to server
    retVal = connect(sockfd, (struct sockaddr*)&channelAddr, sizeof(channelAddr));
    if (retVal < 0) {
        perror("Can't connect to channel server");
        exit(1);
    }
}

void createBuffers() {
    // Creating buffer for encoded noised content - 31 bytes (bit chars)
    encodedBitsFileBuffer = (char*)calloc(encodedBlockLength, sizeof(char));
    if (encodedBitsFileBuffer == NULL) {
        perror("Can't allocate memory for buffer");
        exit(1);
    }

    // Creating buffer for decoded content - 26 bytes (bit chars)
    decodedBitsFileBuffer = (char*)calloc(originalBlockLength, sizeof(char));
    if (decodedBitsFileBuffer == NULL) {
        perror("Can't allocate memory for buffer");
        exit(1);
    }

    // Creating buffer for file content -  TODO decide size because 26 bits are not full bytes
    rawBytesFileBuffer = (char*)calloc(originalBlockLength, sizeof(char));
    if (rawBytesFileBuffer == NULL) {
        perror("Can't allocate memory for buffer");
        exit(1);
    }
}

void readBlockFromSocket() {
    // Reading block from socket
    bitsRead = 0;
    bitsCurrRead = 1;
    while (bitsRead < encodedBlockLength) {
        bitsCurrRead = recv(sockfd, *((&decodedBitsFileBuffer) + bitsRead), encodedBlockLength - bitsRead, 0);
        bitsRead += bitsCurrRead;
    }
    if (bitsCurrRead < 0 || bitsRead != encodedBlockLength) { // There was an error
        perror("Couldn't read encoded block from socket");
        exit(1);
    }
    bitsReadTotal += bitsRead;
}

void copyToDecodedBuffer() {
    int originalIndex = 0;
    for (int encodedIndex = 2; encodedIndex < 32; encodedIndex++) {
        if (encodedIndex != 3 && encodedIndex != 7 && encodedIndex != 15) {
            decodedBitsFileBuffer[originalIndex] = encodedBitsFileBuffer[encodedIndex];
            originalIndex++;
        }
    }
}

int VerifyCheckbit(int number) {
    int result = 0;
    for (int i = number - 1; i < 32; i += (2 * number)) {
        for (int j = 0; j < number; j++) {
            result ^= encodedBitsFileBuffer[i + j];
        }
    }
    result ^= encodedBitsFileBuffer[number - 1];
    return result;
}

void hummingDecode() {
    int errorIndex = 0;
    for (int i = 1; i < 5; i++) {
        int power = (int)(pow(2, i));
        errorIndex += (power * VerifyCheckbit(power));
    }
    if (errorIndex != 0) {
        encodedBitsFileBuffer[errorIndex] = 1 - encodedBitsFileBuffer[errorIndex];
        bitsCorrected++;
    }
    copyToDecodedBuffer();
}

void writeBlockToFile() {
    // Writing decoded block to file
    bytesWritten = fwrite(encodedBitsFileBuffer, 1, originalBlockLength, filePointer);
    if (bytesWritten != originalBlockLength) { // There was an error
        perror("Couldn't read block from file");
        exit(1);
    }
    bytesWrittenTotal += bytesWritten;
}

int main(int argc, char* argv[]) {
    // Checking number of arguments
    if (argc != 3) {
        perror("Number of cmd args is not 2");
        exit(1);
    }

    // Parsing arguments
    channelSenderIPString = argv[1];
    sscanf_s(argv[2], "%hu", &channelSenderPort);

    // Initializing Winsock
    retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (retVal != NO_ERROR) {
        perror("Error at WSAStartup");
        exit(1);
    }

    // Creating socket and connecting to it
    connectToSocket();

    // Ask user to enter file name
    printf("enter file name:\n");
    sscanf_s("%s", fileName);

    while (strcmp(fileName, "quit") != 0) {
        // Opening file
        fopen_s(&filePointer, fileName, "r");
        if (filePointer == NULL) {
            perror("Can't open file");
            exit(1);
        }

        // Creating buffers
        createBuffers();

        // Reading file content to buffer
        // TODO add counter
        while (feof(filePointer) == 0) {
            readBlockFromSocket();
            hummingDecode();
            writeBlockToFile();
        }

        // Closing socket and file
        closesocket(sockfd);
        fclose(filePointer);

        // Printing messages
        printf("received: %d bytes\n", bitsReadTotal / 8);
        printf("wrote: %d bytes\n", bytesWritten);
        printf("corrected %d errors\n", bitsCorrected);
    }

    // Cleaning up Winsock
    retVal = WSACleanup();
    if (retVal != NO_ERROR) {
        perror("Error at WSACleanup");
        exit(1);
    }

    exit(0);
}