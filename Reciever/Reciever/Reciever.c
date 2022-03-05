#include <stdio.h>
#include <math.h>
#include "winsock2.h"
#include "ws2tcpip.h"
#define originalBlockLength 26
#define encodedBlockLength 31
#define extendedBufferLength 208 // 26 bytes * 8 bits per bytes
#pragma comment(lib, "ws2_32.lib")

WSADATA wsaData;
char* fileName, * channelSenderIPString, * encodedBitsFileBuffer, * decodedBitsFileBuffer, * sectionFileBuffer , * bytesFileBuffer;
FILE* filePointer;
short channelSenderPort;
struct sockaddr_in channelAddr;
int sockfd, retVal, bytesWritten = 0, bytesWrittenTotal = 0, bitsRead = 0, bitsCurrRead = 0, bitsReadTotal = 0, bitsCorrectedTotal = 0;
FD_SET sockfdSet;

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

    // Creating buffer for section file content -  26 bytes * 8 bits per bytes (26 char bytes)
    sectionFileBuffer = (char*)calloc(extendedBufferLength, sizeof(char));
    if (sectionFileBuffer == NULL) {
        perror("Can't allocate memory for buffer");
        exit(1);
    }

    // Creating buffer for file content -  26 bytes
    bytesFileBuffer = (char*)calloc(originalBlockLength, sizeof(char));
    if (bytesFileBuffer == NULL) {
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

int verifyCheckbit(int number) {
    int result = 0;
    for (int i = number - 1; i < 32; i += (2 * number)) {
        for (int j = 0; j < number; j++) {
            result ^= encodedBitsFileBuffer[i + j];
        }
    }
    result ^= encodedBitsFileBuffer[number - 1];
    return result;
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

char flipBit(char bit) {
    if (bit == '0')
        return '1';
    else
        return '0';
}

void hummingDecode() {
    int errorIndex = 0;
    for (int i = 1; i < 5; i++) {
        int power = (int)(pow(2, i));
        errorIndex += (power * verifyCheckbit(power));
    }
    if (errorIndex != 0) {
        encodedBitsFileBuffer[errorIndex] = flipBit(encodedBitsFileBuffer[errorIndex]);
        bitsCorrectedTotal++;
    }
    copyToDecodedBuffer();
}

void writeBlockToSectionBuffer(int startIndexInSection) {
    for (int i = startIndexInSection; i < originalBlockLength; i++) {
        sectionFileBuffer[i] = decodedBitsFileBuffer[i % originalBlockLength];
    }
}

void translateSectionFromCharBitsToBytes() {
    for (int i = 0; i < extendedBufferLength; i+=8) {
        bytesFileBuffer[i] = (int)(strtol(&(sectionFileBuffer[8*i]), 0, 2));
    }
}

void writeSectionToFile() {
    // Writing decoded block to file
    bytesWritten = fwrite(bytesFileBuffer, 1, extendedBufferLength, filePointer);
    if (bytesWritten != extendedBufferLength) { // There was an error
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
        fopen_s(&filePointer, fileName, "w");
        if (filePointer == NULL) {
            perror("Can't open file");
            exit(1);
        }

        // Creating buffers
        createBuffers();

        FD_SET(sockfd, &sockfdSet);

        // Reading file content from socket
        while (select(0, &sockfdSet, NULL, NULL, NULL) > 0) {
            for (int i = 0; i < extendedBufferLength; i += originalBlockLength) {
                readBlockFromSocket();
                hummingDecode();
                writeBlockToSectionBuffer(i);
            }
            translateSectionFromCharBitsToBytes();
            writeSectionToFile();
        }

        // Closing socket and file
        closesocket(sockfd);
        fclose(filePointer);

        // Printing messages
        printf("received: %d bytes\n", bitsReadTotal / 8);
        printf("wrote: %d bytes\n", bytesWrittenTotal);
        printf("corrected %d errors\n", bitsCorrectedTotal);
    }

    // Cleaning up Winsock
    retVal = WSACleanup();
    if (retVal != NO_ERROR) {
        perror("Error at WSACleanup");
        exit(1);
    }

    exit(0);
}