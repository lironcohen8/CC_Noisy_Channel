#include <stdio.h>
#include "winsock2.h"
#include "ws2tcpip.h"
#pragma comment(lib, "ws2_32.lib")

WSADATA wsaData;
char* fileName, * channelSenderIPString, * encodedFileBuffer, * decodedFileBuffer;
FILE* filePointer;
short channelSenderPort;
struct sockaddr_in channelAddr;
int sockfd, retVal, fileLength = 0, bytesWritten = 0, bytesRead = 0, bytesCurrRead = 0, bitsFixed = 0;

void read31BitsFromSocket() {
    // Reading block from socket
    bytesRead = 0;
    bytesCurrRead = 1;
    while (bytesRead < 31) {
        bytesCurrRead = recv(sockfd, *((&decodedFileBuffer) + bytesRead), 31 - bytesRead, 0);
        bytesRead += bytesCurrRead;
    }
    if (bytesCurrRead < 0 || bytesRead != 31) { // There was an error TODO change
        perror("Couldn't read encoded block from socket");
        exit(1);
    }
}

void copyToDecodedBuffer() {
    int originalIndex = 0;
    for (int encodedIndex = 2; encodedIndex < 32; encodedIndex++) {
        if (encodedIndex != 3 && encodedIndex != 7 && encodedIndex != 15) {
            decodedFileBuffer[originalIndex] = encodedFileBuffer[encodedIndex];
            originalIndex++;
        }
    }
}

int VerifyCheckbit(int number) {
    int result = 0;
    for (int i = number - 1; i < 32; i += (2 * number)) {
        for (int j = 0; j < number; j++) {
            result ^= encodedFileBuffer[i + j];
        }
    }
    result ^= encodedFileBuffer[number - 1];
    return result;
}

void hummingDecode() {
    int errorIndex = 0;
    for (int i = 1; i < 5; i++) {
        int power = (int)(pow(2, i));
        errorIndex += (power * VerifyCheckbit(power));
    }
    if (errorIndex != 0) {
        encodedFileBuffer[errorIndex] = 1 - encodedFileBuffer[errorIndex];
        bitsFixed++;
    }
    copyToDecodedBuffer();
}

void write26BitsToFile() {
    // Writing decoded block to file
    bytesWritten = fwrite(encodedFileBuffer, 1, 26, filePointer);
    if (bytesWritten != 26) { // There was an error
        perror("Couldn't read block from file");
        exit(1);
    }
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

        // Creating buffer for noise channel content TODO should it be 31 bits?
        encodedFileBuffer = (char*)calloc(31, sizeof(char));
        if (encodedFileBuffer == NULL) {
            perror("Can't allocate memory for buffer");
            exit(1);
        }

        // Creating buffer for decoded file content TODO should it be 26 bits?
        decodedFileBuffer = (char*)calloc(26, sizeof(char));
        if (decodedFileBuffer == NULL) {
            perror("Can't allocate memory for buffer");
            exit(1);
        }

        // Reading file content to buffer
        // TODO add counter
        while (feof(filePointer) == 0) {
            read31BitsFromSocket();
            hummingDecode();
            write26BitsToFile();
        }

        // Closing socket
        closesocket(sockfd);

        // Closing file
        fclose(filePointer);

        printf("received: %d bytes\n", bytesRead);
        printf("wrote: %d bytes\n", bytesWritten);
        printf("corrected %d errors\n", bitsFixed);



        // Cleaning up Winsock
        retVal = WSACleanup();
        if (retVal != NO_ERROR) {
            perror("Error at WSACleanup");
            exit(1);
        }

        exit(0);
    }
}