#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define originalBlockLength 26
#define encodedBlockLength 31
#define extendedBufferLength 208 // 26 bytes * 8 bits per bytes
#pragma comment(lib, "ws2_32.lib")

WSADATA wsaData;
char* fileName, * channelSenderIPString, * rawBytesFileBuffer, * originalBitsFileBuffer, * encodedBitsFileBuffer;
FILE* filePointer;
short channelSenderPort;
struct sockaddr_in channelAddr;
int sockfd, retVal, bytesRead = 0, bytesReadTotal = 0, bitsWritten = 0, bitsCurrWrite = 0, bitsWrittenTotal = 0;

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
    // Creating buffer for raw file - 26 bytes
    rawBytesFileBuffer = (char*)calloc(originalBlockLength, sizeof(char));
    if (rawBytesFileBuffer == NULL) {
        perror("Can't allocate memory for buffer");
        exit(1);
    }

    // Creating buffer for original bits - 26*8 bytes (208 bit chars)
    originalBitsFileBuffer = (char*)calloc(extendedBufferLength, sizeof(char));
    if (originalBitsFileBuffer == NULL) {
        perror("Can't allocate memory for buffer");
        exit(1);
    }

    // Creating buffer for encoded bits - 31 bytes (bit chars)
    encodedBitsFileBuffer = (char*)calloc(encodedBlockLength, sizeof(char));
    if (encodedBitsFileBuffer == NULL) {
        perror("Can't allocate memory for buffer");
        exit(1);
    }
}

void readSectionFromBuffer() {
    // Reading 26 bytes from file
    bytesRead = fread(rawBytesFileBuffer, 1, originalBlockLength, filePointer);
    if (bytesRead != originalBlockLength) { // There was an error
        perror("Couldn't read block from file");
        exit(1);
    }
    bytesReadTotal += bytesRead;
}

void translateSectionFromBytesToCharBits() {
    for (int i = 0; i < originalBlockLength; i++) {
        _itoa_s(rawBytesFileBuffer[i], &(originalBitsFileBuffer[8 * i]), 8, 2);
    }    
}

void copyToEncodedBuffer(int startIndexInSection) {
    int originalIndex = startIndexInSection;
    for (int encodedIndex = 2; encodedIndex < encodedBlockLength; encodedIndex++) {
        if (encodedIndex != 3 && encodedIndex != 7 && encodedIndex != 15) {
            encodedBitsFileBuffer[encodedIndex] = originalBitsFileBuffer[originalIndex];
            originalIndex++;
        }
    }
}

void generateParityBit(int number) {
    int result = 0;
    for (int i = number - 1; i < encodedBlockLength; i += (2*number)) {
        for (int j = 0; j < number; j++) {
            result ^= encodedBitsFileBuffer[i + j];
        }
    }
    encodedBitsFileBuffer[number - 1] = result;
}

void hummingEncode() {
    for (int i = 1; i < 5; i++) {
        generateParityBit((int)(pow(2, i)));
    }
}

void writeBlockToSocket() {
    // Writing encoded block to channel socket
    bitsWritten = 0;
    bitsCurrWrite = 1;
    while (bitsWritten < encodedBlockLength) {
        bitsCurrWrite = send(sockfd, *((&encodedBitsFileBuffer) + bitsWritten), encodedBlockLength - bitsWritten, 0);
        bitsWritten += bitsCurrWrite;
    }
    if (bitsCurrWrite < 0 || bitsWritten != encodedBlockLength) { // There was an error
        perror("Couldn't write encoded block to socket");
        exit(1);
    }
    bitsWrittenTotal += bitsWritten;
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
    fileName = (char*)calloc(MAX_PATH, sizeof(char));
    if (fileName == NULL) {
        perror("Can't allocate memory for file name");
        exit(1);
    }
    printf("enter file name:\n");
    sscanf_s("%s", fileName, sizeof(fileName));

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
        while (feof(filePointer) == 0) {
            readSectionFromBuffer();
            translateSectionFromBytesToCharBits();
            for (int i = 0; i < extendedBufferLength; i+= originalBlockLength) {
                hummingEncode(i);
                writeBlockToSocket();
            }
        }

        // Closing socket and files
        closesocket(sockfd);
        fclose(filePointer);

        // Printing messages
        printf("file length: %d bytes\n", bytesReadTotal);
        printf("sent: %d bytes\n", bitsWrittenTotal / 8);
    }

    // Cleaning up Winsock
    retVal = WSACleanup();
    if (retVal != NO_ERROR) {
        perror("Error at WSACleanup");
        exit(1);
    }

    exit(0);
}