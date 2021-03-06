#define _CRT_SECURE_NO_WARNINGS // Used for enabling usage of functions like fopen in vs
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define originalBlockLength 26
#define encodedBlockLength 31
#define extendedBufferLength 208 // 26 bytes * 8 bits per bytes
#pragma comment(lib, "ws2_32.lib") // Used to link the library Ws2_32.lib for windows socketing

WSADATA wsaData;
char* fileName, * channelReceiverIPString, * encodedBitsFileBuffer, * decodedBitsFileBuffer, * sectionFileBuffer, * bytesFileBuffer;
FILE* filePointer;
short channelreceiverPort;
struct sockaddr_in channelAddr;
int sockfd, retVal, finished = 0, bytesWritten = 0, bytesWrittenTotal = 0, bitsRead = 0, bitsCurrRead = 1, bitsReadTotal = 0, bitsCorrectedTotal = 0;
int blockLength = 0;

void connectToSocket() {
    // Creating socket 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Can't create socket");
        exit(1);
    }

    // Creating channel address struct
    channelAddr.sin_family = AF_INET;
    inet_pton(AF_INET, (PCSTR)channelReceiverIPString, &(channelAddr.sin_addr.s_addr));
    channelAddr.sin_port = htons(channelreceiverPort);

    // Connecting to server
    retVal = connect(sockfd, (struct sockaddr*)&channelAddr, sizeof(channelAddr));
    if (retVal < 0) {
        perror("Can't connect to channel server");
        exit(1);
    }
}

void createSectionBuffers() {
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

void createBlockBuffers() {
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
}

void readBlockFromSocket() {
    // Reading block from socket
    bitsRead = 0;
    bitsCurrRead = 1;
    while (bitsRead < encodedBlockLength) {
        bitsCurrRead = recv(sockfd, *((&encodedBitsFileBuffer) + bitsRead), encodedBlockLength - bitsRead, 0);
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
        bitsReadTotal += encodedBlockLength;
    }
}

int IsCheckBitWrong(int number) {
    // Comparing parity bits andn their corresponding calculations
    int sum = 0;
    char checkBit = encodedBitsFileBuffer[number - 1];
    encodedBitsFileBuffer[number - 1] = '0';
    for (int i = number - 1; i < encodedBlockLength; i += (2 * number)) {
        for (int j = 0; j < number; j++) {
            if (encodedBitsFileBuffer[i + j] == '\0') {
                break;
            }
            int bitResult = (encodedBitsFileBuffer[i + j]) - '0';
            sum += bitResult;
        }
    }
    encodedBitsFileBuffer[number - 1] = checkBit;
    return checkBit != ((sum % 2 == 0) ? '0' : '1');
}

void copyToDecodedBuffer() {
    // Coping from encodedBitsFileBuffer to relevant places in decodedBitsFileBuffer
    int decodedIndex = 0;
    for (int encodedIndex = 0; encodedIndex < encodedBlockLength; encodedIndex++) {
        if (encodedIndex != 0 && encodedIndex != 1 && encodedIndex != 3 && encodedIndex != 7 && encodedIndex != 15) {
            decodedBitsFileBuffer[decodedIndex] = encodedBitsFileBuffer[encodedIndex];
            decodedIndex++;
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
    // Checking parity bits and correcting if needed
    int errorIndex = 0;
    for (int i = 0; i < 5; i++) {
        int power = ((int)(pow(2, i)));
        errorIndex += (power * IsCheckBitWrong(power));
    }
    if (errorIndex != 0) {
        encodedBitsFileBuffer[errorIndex - 1] = flipBit(encodedBitsFileBuffer[errorIndex - 1]);
        bitsCorrectedTotal++;
    }
    copyToDecodedBuffer();
}

void writeBlockToSectionBuffer(int startIndexInSection) {
    // Writing from decodedBitsFileBuffer to sectionFileBuffer
    for (int i = startIndexInSection; i < (startIndexInSection + originalBlockLength); i++) {
        sectionFileBuffer[i] = decodedBitsFileBuffer[i % originalBlockLength];
    }
}

void translateSectionFromCharBitsToBytes() {
    // Based on answer from: https://www.dreamincode.net/forums/topic/134396-how-to-convert-a-char-to-its-8-binary-bits-in-c/
    bytesFileBuffer = (char*)calloc(originalBlockLength, sizeof(char));
    if (bytesFileBuffer == NULL) {
        perror("Can't allocate memory for buffer");
        exit(1);
    }
    for (int i = 0; i < originalBlockLength; i++) {
        for (int j = 0; j < 8; j++) {
            if (sectionFileBuffer[(8 * i) + j] == '\0') {
                break;
            }
            int bitResult = sectionFileBuffer[(8 * i) + j] == '1' ? 1 : 0;
            bytesFileBuffer[i] += (bitResult << (7 - j));
        }
    }
}

void writeSectionToFile() {
    blockLength = originalBlockLength;
    if (bytesFileBuffer[originalBlockLength - 1] == '\0') {
        blockLength = strlen(bytesFileBuffer);
    }
    // Writing decoded block to file
    bytesWritten = fwrite(bytesFileBuffer, 1, blockLength, filePointer);
    if (bytesWritten != blockLength) { // There was an error
        perror("Couldn't write block to file");
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
    channelReceiverIPString = argv[1];
    sscanf_s(argv[2], "%hu", &channelreceiverPort);

    // Initializing Winsock
    retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (retVal != 0) {
        perror("Error at WSAStartup");
        exit(1);
    }

    // Ask user to enter file name
    fileName = (char*)calloc(MAX_PATH, sizeof(char));
    if (fileName == NULL) {
        perror("Can't allocate memory for file name");
        exit(1);
    }
    printf("enter file name:\n");
    retVal = scanf("%s", fileName);

    while (strcmp(fileName, "quit") != 0) {
        // Opening file
        filePointer = fopen(fileName, "w");
        if (filePointer == NULL) {
            perror("Can't open file");
            exit(1);
        }

        // Creating socket and connecting to it
        connectToSocket();

        // Creating buffers
        createBlockBuffers();
        createSectionBuffers();

        // Reading file content from socket
        while (1) {
            for (int i = 0; i < extendedBufferLength; i += originalBlockLength) {
                readBlockFromSocket();
                if (finished == 1) {
                    break;
                }
                hummingDecode();
                writeBlockToSectionBuffer(i);
                createBlockBuffers(); // changing block buffers values to 0
            }
            if (sectionFileBuffer[0] != '\0') {
                translateSectionFromCharBitsToBytes();
                writeSectionToFile();
                createSectionBuffers(); // changing block buffers values to 0
            }
            if (finished == 1) {
                break;
            }
        }

        // Closing socket and file
        closesocket(sockfd);
        fclose(filePointer);

        // Printing messages
        printf("received: %d bytes\n", bitsReadTotal / 8);
        printf("wrote: %d bytes\n", bytesWrittenTotal);
        printf("corrected %d errors\n", bitsCorrectedTotal);

        // Getting new fileName and initializing parameters
        printf("enter file name:\n");
        retVal = scanf("%s", fileName);
        finished = 0;
        bitsReadTotal = 0;
        bytesWrittenTotal = 0;
        bitsCorrectedTotal = 0;
        blockLength = 0;
    }

    // Cleaning up Winsock
    retVal = WSACleanup();
    if (retVal != 0) {
        perror("Error at WSACleanup");
        exit(1);
    }

    exit(0);
}