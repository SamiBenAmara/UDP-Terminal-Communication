#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include "list.h"
#include <arpa/inet.h>

#define MSG_MAX_LEN 512

// Pthread variables 
static pthread_t receivePID;
static pthread_t sendPID;
static pthread_t printPID;
static pthread_t keyboardPID;

// Variables that hold arguments from the command line
static char * remoteMachineName;
static int remotePortNumber;
static int myPortNumber;

// Holds remote machine name
static char resultString[512];

// Socket variables
static int socketDescriptor; // Holds result from socket() function

// List variables
static List * inputList;
static List * outputList;

// Message variables 
static char * sentMessage;      // Buffer for message that was typed into the terminal
static char * receivedMessage;  // Buffer for the message that was received from the other terminal
static int globalBytesRx;       // Variable that specifies the size of the received message

static struct addrinfo * result;    // pointer to addrinfo, globalised so I can use it in the delete function

static pthread_mutex_t safeToAccessSend = PTHREAD_MUTEX_INITIALIZER;        // keyboard / send mutex
static pthread_mutex_t safeToAccessPrint = PTHREAD_MUTEX_INITIALIZER;       // print / receive mutex

static pthread_cond_t sendConditionVariable = PTHREAD_COND_INITIALIZER;     // keyboard / send condition variable
static pthread_cond_t printConditionVariable = PTHREAD_COND_INITIALIZER;    // print / receive condition variable

// For use with List_free
static int freeCounter;
static void complexTestFreeFn(void * item)
{
    if (item != NULL) freeCounter++;
}

void closeProgram();

// Thread that receives data from other terminal
void * receiveThread(void * unused)
{
    // Basic setup and error checking 
    // Used help and code from Beej's guide, and the workshop code 
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    
    hints.ai_socktype = SOCK_DGRAM; 
    hints.ai_flags = AI_PASSIVE;    

    int error = getaddrinfo(remoteMachineName, NULL, &hints, &result); // Took help from citation (1) in the readme.txt

    if (error != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        exit(EXIT_FAILURE);
    }

    // Get the IP address from the name
    void * ptr = &((struct sockaddr_in*) result->ai_addr)->sin_addr; // Took help from citation (1) in the readme.txt
    inet_ntop(result->ai_family, ptr, resultString, MSG_MAX_LEN);    // Took help from citation (1) in the readme.txt

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;                     
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(myPortNumber);                     
    memset(sin.sin_zero, '\0', sizeof sin.sin_zero);
  
    // Create the socket for UDP
    // Used help and code from Beej's guide and workshop code 
    socketDescriptor = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (socketDescriptor == -1) {
        printf("socket function failed\n");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the port (PORT) that we specify
    if (bind (socketDescriptor, (struct sockaddr*) &sin, sizeof(sin)) == -1) {
        printf("bind function failed\n");
        exit(EXIT_FAILURE);
    }
  
    printf("\nConnecting to port %d at %s...\nInitial setup complete\n", remotePortNumber,resultString);
    printf("Starting while loop...\n");
    
    while (1) {
        
        // Used code from the workshops for sinRemote, recvfrom, and terminateIdx
        struct sockaddr_in sinRemote;
        unsigned int sin_len = sizeof(sinRemote);

        int bytesRx = recvfrom(socketDescriptor, receivedMessage, MSG_MAX_LEN, 0, (struct sockaddr *) &sinRemote, &sin_len);

        if (bytesRx == -1) {
            printf("recvFrom failed\n");
            exit(EXIT_FAILURE);
        }

        int terminateIdx = (bytesRx < MSG_MAX_LEN) ? bytesRx : MSG_MAX_LEN - 1;
	    receivedMessage[terminateIdx] = 0;

        globalBytesRx = bytesRx;

        // Critical section
        pthread_mutex_lock(&safeToAccessPrint);
        {
            if (List_count(outputList) == LIST_MAX_NUM_NODES) {
                printf("Ouput list is full\n");
                exit(EXIT_FAILURE);
            }
            List_prepend(outputList, receivedMessage);
        }
        pthread_mutex_unlock(&safeToAccessPrint);
        pthread_cond_signal(&printConditionVariable);
    }
}

// Thread that prints the output to the screen
void * printThread(void * unused)
{
    char message[MSG_MAX_LEN];
    while(1) {
        
        // Critical section
        pthread_mutex_lock(&safeToAccessPrint);
        {
            pthread_cond_wait(&printConditionVariable, &safeToAccessPrint);

            strcpy(message, List_curr(outputList));
            if (List_trim(outputList) == NULL) {
                printf("List trim failed\n");
                exit(EXIT_FAILURE);
            }
        }
        pthread_mutex_unlock(&safeToAccessPrint);

        if (write(1, message, globalBytesRx) < 0) return NULL;

        if (strcmp(message, "!") == 0 || strcmp(message, "!\n") == 0 || strcmp(message, "\n!") == 0) closeProgram();

        // Reset global variable memory and local variable memory 
        memset(message, 0, MSG_MAX_LEN);
        memset(receivedMessage, 0, MSG_MAX_LEN);
    }
}

// Thread that sends data to other termianl
void * sendThread(void * unused)
{  
    char message[MSG_MAX_LEN];
    while (1) {

        // Critical section
        pthread_mutex_lock(&safeToAccessSend);
        {
            pthread_cond_wait(&sendConditionVariable, &safeToAccessSend);

            strcpy(message, List_curr(inputList));   
            if (List_trim(inputList) == NULL) {
                printf("List trim failed\n");
                exit(EXIT_FAILURE);
            }
        }
        pthread_mutex_unlock(&safeToAccessSend);       

        // Used workshop code for sinRemote and sendTo
        struct sockaddr_in sinRemote;
        memset(&sinRemote, 0, sizeof(sinRemote));
        unsigned int sin_len = sizeof(sinRemote);

        sinRemote.sin_addr.s_addr = inet_addr(resultString);  
        sinRemote.sin_port = htons(remotePortNumber);            

        if (sendto(socketDescriptor, message, strlen(message), 0, (struct sockaddr *) &sinRemote, sin_len) < 0) {
            printf("SendTo failed\n");
            exit(EXIT_FAILURE);
        }

        if (strcmp(message, "!") == 0 || strcmp(message, "!\n") == 0 || strcmp(message, "\n!") == 0) closeProgram();

        // Reset global variable memory and local variable memory
        memset(message, 0, MSG_MAX_LEN);
        memset(sentMessage, 0, MSG_MAX_LEN);
    }
}

// Thread that takes in keyboard input
void * keyboardThread(void * unused)
{
    while(1) {

        // Read message into sendMessage
        if (read(0, sentMessage, MSG_MAX_LEN) < 0) exit(EXIT_FAILURE);

        // Critical section
        pthread_mutex_lock(&safeToAccessSend);
        {
            if (List_count(inputList) == LIST_MAX_NUM_NODES) {
                printf("Input list is full\n");
                exit(EXIT_FAILURE);
            }
            List_prepend(inputList, sentMessage);                
        }
        pthread_mutex_unlock(&safeToAccessSend);
        pthread_cond_signal(&sendConditionVariable);
    }
}

// Cancel threads, destory mutexes and cond variables, free memory
void closeProgram()
{
    pthread_cancel(receivePID);
    pthread_cancel(sendPID);
    pthread_cancel(printPID);
    pthread_cancel(keyboardPID);

    pthread_mutex_destroy(&safeToAccessSend);
    pthread_mutex_destroy(&safeToAccessPrint);

    pthread_cond_destroy(&sendConditionVariable);
    pthread_cond_destroy(&printConditionVariable);

    free(sentMessage);
    free(receivedMessage);

    freeaddrinfo(result);

    List_free(inputList, complexTestFreeFn);
    List_free(outputList, complexTestFreeFn);

    close(socketDescriptor);
}

// Setup threads and variables 
int main(int argCount, char ** args)
{
    freeCounter = 0;

    inputList = List_create();
    outputList = List_create();

    myPortNumber = atoi(args[1]);
    remoteMachineName = args[2];
    remotePortNumber = atoi(args[3]);

    sentMessage = calloc(MSG_MAX_LEN, sizeof(char));
    receivedMessage = calloc(MSG_MAX_LEN, sizeof(char));

    // Used code from workshops 
    pthread_create(&keyboardPID, NULL, keyboardThread, NULL);

    pthread_create(&receivePID, NULL, receiveThread, NULL);

    pthread_create(&printPID, NULL, printThread, NULL);

    pthread_create(&sendPID, NULL, sendThread, NULL);

    pthread_join(receivePID, NULL);
    pthread_join(sendPID, NULL);
    pthread_join(keyboardPID, NULL);
    pthread_join(printPID, NULL);
}