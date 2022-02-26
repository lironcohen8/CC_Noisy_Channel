#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

WSADATA wsaData;
char* fileName, * channelSenderIPString, * originalFileBuffer, * encodedFileBuffer;
FILE* filePointer;
short channelSenderPort;
struct sockaddr_in channelAddr;
int sockfd, retVal, fileLength = 0, bytesRead = 0, bytesWritten = 0, bytesCurrWrite = 0;

void read26BitsFromFile() {
    // Reading block from file
    bytesRead = fread(originalFileBuffer, 1, 26, filePointer);
    if (bytesRead != 26) { // There was an error
        perror("Couldn't read block from file");
        exit(1);
    }
}

void hummingEncode() { // TODO add later
}

void write31BitsToSocket() {
    // Writing encoded block to channel socket
    bytesWritten = 0;
    bytesCurrWrite = 1;
    while (bytesWritten < 31) {
        bytesCurrWrite = send(sockfd, *((&encodedFileBuffer) + bytesWritten), 31 - bytesWritten, 0);
        bytesWritten += bytesCurrWrite;
    }
    if (bytesCurrWrite < 0 || bytesWritten != 31) { // There was an error TODO change
        perror("Couldn't write encoded block to socket");
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

        // Creating buffer for file content TODO understand how to calloc 26 bits
        originalFileBuffer = (char*)calloc(26, sizeof(char));
        if (originalFileBuffer == NULL) {
            perror("Can't allocate memory for buffer");
            exit(1);
        }

        // Creating buffer for encoded file content TODO understand how to calloc 31 bits
        encodedFileBuffer = (char*)calloc(31, sizeof(char));
        if (encodedFileBuffer == NULL) {
            perror("Can't allocate memory for buffer");
            exit(1);
        }

        // Reading file content to buffer
        // TODO add counter
        while (feof(filePointer) == 0) {
            read26BitsFromFile();
            hummingEncode();
            write31BitsToSocket();
        }

        // Closing socket
        closesocket(sockfd);

        // Closing file
        fclose(filePointer);

        printf("file length: %d bytes\n", fileLength);
        printf("sent: %d bytes\n", bytesWritten);
    }

    // Cleaning up Winsock
    retVal = WSACleanup();
    if (retVal != NO_ERROR) {
        perror("Error at WSACleanup");
        exit(1);
    }

    exit(0);
}