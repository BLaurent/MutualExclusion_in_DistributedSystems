#ifndef NODE_H
#define NODE_H

#include<iostream>
#include<stdio.h>
#include<netdb.h>
#include<sys/socket.h>
#include<cstring>
#include<sstream>
#include<arpa/inet.h>
#include<pthread.h>
#include<stdlib.h>
#include<fstream>
#include<cstdlib>
#include<time.h>
#include<sys/time.h>

using namespace std;

#define MAX_NODES 		10
#define HOST_PREFIX 		"net"
#define HOST_SUFFIX 		".utdallas.edu"
#define HOST_RANGE_START 	25
#define PORT_RANGE_START        1126
#define CS_LOG_FILE		"cs_log_file"
#define MAX_CS_ENTRIES		40	
#define MAX_BUFFER_SIZE         128

/***************************************
 * GLOBAL VARIABLE Declaration
 * *************************************/
int sockDesc[MAX_NODES];
int portNums[MAX_NODES];
int myId;
int mySeqNum, highestSeqNum, requestCount, replyCount;
int totalMessages, minMessages, maxMessages;

string hostNames[MAX_NODES];
string nodeDebugFile;

bool deferredReplies[MAX_NODES];
bool receivedReplies[MAX_NODES];
bool activeConnection[MAX_NODES];
bool completionStatus[MAX_NODES];
bool waiting, usingCS, receivedAllReplies, allNodesConnected, exitSession;

bool closeSockets = false;

pthread_t connThread;
pthread_t csThread;
pthread_mutex_t fileMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dataMutex = PTHREAD_MUTEX_INITIALIZER;

sockaddr_in nodeAddress[MAX_NODES];

ofstream csfStream;
ofstream dfStream;

/***************************************
 * ENUM Declaration
 * *************************************/
typedef enum
{
	CS_REQUEST,
	CS_REPLY,
	CS_COMPLETION
}Message;

typedef enum
{
	DEBUG,
	CS_RECORD
}LogType;

/***************************************
 * STRUCTURE Declaration
 * *************************************/
typedef struct
{
	int					sockDesc;
	sockaddr_in				clientAddr;
	int					addrLen;
}Connection;

typedef struct
{
	Message					msgType;
	int					seqId;
	int					nodeId;
}CriticalSectionPacket;

/***************************************
 * FUNCTION Declaration
 * *************************************/
void* processControlMessages(void* ptr);
void* processCriticalSection(void* ptr);
void initializeGlobalData();
string packetToMessage(CriticalSectionPacket* pkt);
CriticalSectionPacket messageToPacket(string msg);
void logger(LogType type, string str);
int uniformDistGenerator(int min, int max);

#endif
