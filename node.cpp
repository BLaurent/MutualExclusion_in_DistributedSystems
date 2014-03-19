#include"node.h"

int main(int argc, char** argv)
{
	int 			socketDesc = -1;
	sockaddr_in		address;
	Connection*		conn;
	std::stringstream	sStream;

	//initializing global data
	initializeGlobalData();

	memset(&address, 0, sizeof(sockaddr_in));
	socketDesc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(socketDesc <=0)
	{
		logger(DEBUG, "ERROR: Cannot create server socket \n");
		return -1;
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(portNums[myId]);
	address.sin_addr.s_addr = INADDR_ANY;

	if(0> bind(socketDesc, (struct sockaddr*)&address, sizeof(sockaddr_in)))
	{
		sStream.str(std::string());
		sStream<<portNums[myId];
		logger(DEBUG, "ERROR: Server socket unable to bind on port="+sStream.str()+"\n");
		return -1;
	}
	if(0> listen(socketDesc, 10))
	{
		sStream.str(std::string());
		sStream<<portNums[myId];
		logger(DEBUG, "ERROR: Server socket unable to listen on port="+sStream.str()+"\n");
		return -1;
	}

	pthread_create(&csThread, 0, processCriticalSection, NULL);
	pthread_detach(csThread);

	while(1)
	{
		conn = (Connection*)malloc(sizeof(Connection));
		conn->sockDesc = accept(socketDesc, (struct sockaddr*)&conn->clientAddr, (socklen_t*)&conn->addrLen);
		if(conn->sockDesc <=0)
		{
			free(conn);
			logger(DEBUG, "ERROR: Unable to accept client - "+std::string(inet_ntoa(conn->clientAddr.sin_addr))+"\n");
		}
		else
		{
			logger(DEBUG, "DEBUG: Client accepted with IP - "+std::string(inet_ntoa(conn->clientAddr.sin_addr))+"\n");
			pthread_create(&connThread, 0, processControlMessages, (void*)conn);
			pthread_detach(connThread);
		}
		//if(exitSession)
		//	break;
	}
	close(socketDesc);
	free(conn);

	return 0;
}

void initializeGlobalData()
{
	char hNo[128];
	int hNum;
	std::stringstream ss;
	hostent* host;

	//get id of the node operating 
	gethostname(hNo, sizeof(hNo));
	hNo[0] = hNo[3];
	hNo[1] = hNo[4];
	myId = atoi(hNo) - HOST_RANGE_START;
	cout<<"Host Number="<<atoi(hNo)<<" myId="<<myId<<endl;
	//create debug file name string
	ss.str(std::string());
	ss<<myId;
	nodeDebugFile = "debug_file_node_"+ss.str();

	waiting = false;
	usingCS = false;
	receivedAllReplies = false;
	exitSession = false;
	
	mySeqNum = 0; 
	highestSeqNum = 0;

	for(int i=0; i<MAX_NODES; i++)
	{
		ss.str(std::string());

		if(HOST_RANGE_START + i < 10)
		{
			ss<<0;
		}

		ss<< (HOST_RANGE_START + i);
		hostNames[i] = HOST_PREFIX + ss.str()+ HOST_SUFFIX;
		portNums[i] = PORT_RANGE_START + i;
		cout<<"port["<<i<<"] = "<<portNums[i]<<endl; 
		if(i != myId)
		{
			sockDesc[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if(sockDesc[i] <=0)
			{
				ss.str(std::string());
				ss<<i;
				logger(DEBUG, "ERROR: Cannot create client socket for node="+ss.str()+"\n");
			}

			memset(&nodeAddress[i], 0, sizeof(sockaddr_in));
			nodeAddress[i].sin_family = AF_INET;
			nodeAddress[i].sin_port = htons(portNums[i]);

			host = gethostbyname(hostNames[i].c_str());
			if(!host)
			{
				logger(DEBUG, "ERROR: host="+hostNames[i]+" not found"+"\n");
			}
			else
			{
				memcpy(&nodeAddress[i].sin_addr, host->h_addr_list[0], host->h_length);
				logger(DEBUG,"DEBUG: host="+hostNames[i]+" has IP address="+inet_ntoa(nodeAddress[i].sin_addr)+"\n");
			}

			if(-1 == connect(sockDesc[i], (struct sockaddr*)&nodeAddress[i], sizeof(nodeAddress[i])))
			{
				ss.str(std::string());
				ss<<i;
				logger(DEBUG, "ERROR: Cannot connect to node="+ss.str()+"\n");
			}
			else
			{
				ss.str(std::string());
				ss<<i;
				logger(DEBUG, "DEBUG: Connection Successful to node="+ss.str()+"\n");
				activeConnection[i] = true;
			}
		}

		deferredReplies[i] = false;
		receivedReplies[i] = false;
	}

	return;
}

void* processControlMessages(void* ptr)
{
	logger(DEBUG, "ENTER - processControlMessages thread created() \n");

	Connection* 			con;
	CriticalSectionPacket 		csPacket;
	CriticalSectionPacket		sendPkt;
	std::stringstream 		sStream;
	int 				noBytesRead = 0;
	int           			counter = 0;
	char* 				buffer;
	bool 				myPriority=false;
	string				msg;
	bool				exitCompute=false;

	if(ptr == NULL)
	{
		pthread_exit(0);
	}

	con = (Connection*)ptr;

	while(1)
	{
		sStream.str(std::string());
		buffer = new char[MAX_BUFFER_SIZE];
		noBytesRead = recv(con->sockDesc, buffer, MAX_BUFFER_SIZE, 0);

		buffer[noBytesRead] = '\0';
		sStream<<buffer<<" num bytes read="<<noBytesRead<<"\n";

		if(buffer[0] != '\0')
		{
			logger(DEBUG, "DEBUG: processControlMessages() received following message " + sStream.str());

			csPacket = messageToPacket(buffer);

			switch(csPacket.msgType)
			{
				case CS_REPLY:
					counter = 0;
					pthread_mutex_lock(&dataMutex);
					replyCount++;
					totalMessages++;

					sStream.str(std::string());
					sStream<<csPacket.nodeId<<"\n";
					logger(DEBUG, "processControlMessages(), received reply from node="+ sStream.str());
					
					sStream.str(std::string());

					receivedReplies[csPacket.nodeId] = true;
					if(!receivedAllReplies)
					{
						for(int i=0; i<MAX_NODES; i++)
						{
							if(i != myId && !receivedReplies[i])
								sStream<<i<<" ";
							else if(i != myId && receivedReplies[i])
								counter++;
						}
						if(counter == MAX_NODES-1)
							receivedAllReplies = true;
					}
					sStream<<"\n";
					logger(DEBUG, "DEBUG: processControlMessages() waiting for replies from "+sStream.str());
					pthread_mutex_unlock(&dataMutex);
					break;
				case CS_REQUEST:
					pthread_mutex_lock(&dataMutex);
					while(1)
					{
						if(allNodesConnected)
							break;
					}

					if(csPacket.seqId > highestSeqNum)
						highestSeqNum = csPacket.seqId;

					if((csPacket.seqId > mySeqNum) || (csPacket.seqId == mySeqNum && csPacket.nodeId > myId))
						myPriority = true;

					if(usingCS || (waiting && myPriority))
					{
						sStream.str(std::string());
						sStream<<csPacket.nodeId<<" and seqNum="<<csPacket.seqId<<endl;
						deferredReplies[csPacket.nodeId] = true;
						logger(DEBUG, "DEBUG: processControlMessage() - Case 1, deferring request from node="+sStream.str());
					}
					else if((!usingCS || !waiting) || 
							(waiting && !receivedReplies[csPacket.nodeId] && !myPriority))
					{
						receivedReplies[csPacket.nodeId] = false;
						receivedAllReplies = false;

						sendPkt.msgType = CS_REPLY;
						sendPkt.seqId = 0;
						sendPkt.nodeId = myId;

						msg = packetToMessage(&sendPkt);
						msg +="\0";
						sStream.str(std::string());
						sStream<<csPacket.nodeId<<" message-"<<msg<<" num bytes sent="<<strlen(msg.c_str())+1<<"\n";
						logger(DEBUG, "DEBUG: processControlMessage()- Case 2, sending reply message to node "+sStream.str());

						//Note-Num bytes sent will be less than 56 bytes. Hence not performing check on the return value
						send(sockDesc[csPacket.nodeId], msg.c_str(), strlen(msg.c_str())+1, 0);

					}
					else if(waiting && receivedReplies[csPacket.nodeId] && !myPriority)
					{
						receivedReplies[csPacket.nodeId] = false;
						receivedAllReplies = false;

						sendPkt.msgType = CS_REPLY;
						sendPkt.seqId = 0;
						sendPkt.nodeId = myId;

						msg = packetToMessage(&sendPkt);
						msg +="\0";
						sStream.str(std::string());
						sStream<<csPacket.nodeId<<" message-"<<msg<<" num bytes sent="<<strlen(msg.c_str())+1<<"\n";
						logger(DEBUG, "DEBUG: processControlMessage()- Case 3, sending reply message to node "+sStream.str());

						//Note-Num bytes sent will be less than 56 bytes. Hence not performing check on the return value
						send(sockDesc[csPacket.nodeId], msg.c_str(), strlen(msg.c_str())+1, 0);

						usleep(10000);

						sendPkt.msgType = CS_REQUEST;
						sendPkt.seqId = mySeqNum;
						sendPkt.nodeId = myId;

						requestCount++;
						totalMessages++;

						msg = packetToMessage(&sendPkt);
						msg +="\0";
						sStream.str(std::string());
						sStream<<csPacket.nodeId<<" message-"<<msg<<" num bytes sent="<<strlen(msg.c_str())+1<<"\n";
						logger(DEBUG, "DEBUG: processControlMessage()- Case 3, sending request message to node "+sStream.str());

						//Note-Num bytes sent will be less than 56 bytes. Hence not performing check on the return value
						send(sockDesc[csPacket.nodeId], msg.c_str(), strlen(msg.c_str())+1, 0);
					}
					pthread_mutex_unlock(&dataMutex);
					break;
				case CS_COMPLETION:
					pthread_mutex_lock(&dataMutex);
					if(myId == 0)
					{	
						sStream.str(std::string());
						sStream<<csPacket.nodeId<<"\n";
						logger(DEBUG, "Received Completion message from node "+sStream.str());

						completionStatus[csPacket.nodeId] = true;
						exitCompute=true;

						for(int i=0; i<MAX_NODES; i++)
						{
							if(!completionStatus[i])
							{
								exitCompute=false;
								break;
							}
						}
						if(exitCompute)
						{
						cout<<"exitCompute is true!!"<<endl;
							sendPkt.msgType = CS_COMPLETION;
							sendPkt.seqId = 0;
							sendPkt.nodeId = 0;
							
							msg = packetToMessage(&sendPkt);
							msg +="\0";
							sStream.str(std::string());
							sStream<<" message-"<<msg<<" num bytes sent="<<strlen(msg.c_str())+1<<"\n";
							logger(DEBUG, "DEBUG: processControlMessage()- sending completion message to all nodes "+sStream.str());
							
							exitSession=true;

							for(int i=0; i<MAX_NODES; i++)
							{
								if(i != myId)
								{
									send(sockDesc[i],msg.c_str(), strlen(msg.c_str())+1, 0);
								}
							}
						}

					}
					else if(csPacket.nodeId == 0)
					{
						exitSession=true;
					}
					pthread_mutex_unlock(&dataMutex);
					break;
				default:
					cout<<"Invalid msgType="<<csPacket.msgType<<" received \n";
					break;
			}
		}
		pthread_mutex_lock(&dataMutex);
		if(exitSession)
		{
			for(int i=0; i<MAX_NODES; i++)
			{
				if(i != myId)
				{
					close(sockDesc[i]);
				}
			}
			pthread_mutex_unlock(&dataMutex);
			break;
		}
		pthread_mutex_unlock(&dataMutex);
	}

	con = (Connection*) ptr;
	close(con->sockDesc);
	free(con);
	
	cout<<"EXIT - processControlMessages() thread \n";
	logger(DEBUG, "EXIT - processControlMessages() \n");
	pthread_exit(0);
}

void* processCriticalSection(void* ptr)
{
	logger(DEBUG, "ENTER - processCriticalSection()\n");

	std::stringstream 		ss;
	int 				noCSEntries = 0;
	string 				sendMessage = "";
	CriticalSectionPacket* 		pkt;
	long int			begin, end;
	int				uniNum;
	struct timeval			tv;
	//connect to all the nodes before proceeding further
	pthread_mutex_lock(&dataMutex);
	for(int i=0; i<MAX_NODES; i++)
	{
		if(i != myId)
		{
			while(activeConnection[i] == false)
			{
				if(-1 < connect(sockDesc[i], (struct sockaddr*)&nodeAddress[i], sizeof(nodeAddress[i])))
				{
					ss.str(std::string());
					ss<<i;
					logger(DEBUG, "DEBUG: Connection Successful to node="+ss.str()+"\n");
					activeConnection[i] = true;
				}
			}
		}
	}
	allNodesConnected = true;
	pthread_mutex_unlock(&dataMutex);

	while(noCSEntries < MAX_CS_ENTRIES)
	{
		ss.str(std::string());	
		pthread_mutex_lock(&dataMutex);
		if(myId%2 > 0)
		{
			pthread_mutex_unlock(&dataMutex);
			uniNum = uniformDistGenerator(10,100);
			ss<<"DEBUG - Uniform Random Number Generated is "<<uniNum<<"\n";
			logger(DEBUG, ss.str());
			usleep(uniNum*1000);
		}
		else
		{
			pthread_mutex_unlock(&dataMutex);
			if(noCSEntries < 20)
			{
				uniNum = uniformDistGenerator(10,100);
				ss<<"DEBUG - Uniform Random Number Generated is "<<uniNum<<"\n";
				logger(DEBUG, ss.str());
				usleep(uniNum*1000);
			}
			else
			{
				uniNum = uniformDistGenerator(200,500);
				ss<<"DEBUG - Uniform Random Number Generated is "<<uniNum<<"\n";
				logger(DEBUG, ss.str());
				usleep(uniNum*1000);
			}
		}

		pthread_mutex_lock(&dataMutex);
		waiting = true;
		mySeqNum = highestSeqNum + 1;
		requestCount=0;
		replyCount=0;

		pkt = (CriticalSectionPacket*)malloc(sizeof(CriticalSectionPacket));
		if(pkt != NULL)
		{
			gettimeofday(&tv, NULL);
			begin = tv.tv_sec*1000 + tv.tv_usec/1000;
			pkt->msgType = CS_REQUEST;
			pkt->seqId = mySeqNum;
			pkt->nodeId = myId;

			for(int i = 0; i<MAX_NODES; i++)
			{
				if(i != myId && !receivedReplies[i])
				{
					requestCount++;
					totalMessages++;

					sendMessage = packetToMessage(pkt);
					sendMessage +="\0";
					ss.str(std::string());
					ss<<i<<" message-"<<sendMessage<<" no bytes sent="<<strlen(sendMessage.c_str())+1<<"\n";
					logger(DEBUG, "DEBUG: processCriticalSection(), sending CS Request message to node "+ss.str());

					//Note-Num bytes sent will be less than 56 bytes. Hence not performing check on the return value
					send(sockDesc[i], sendMessage.c_str(), strlen(sendMessage.c_str())+1, 0);
				}
			}
			free(pkt);
			pthread_mutex_unlock(&dataMutex);
		}
		else
		{
			pthread_mutex_unlock(&dataMutex);
			logger(DEBUG, "ERROR: Malloc for CriticalSectionPacket Failed when sending request \n");
		}

		while(1)
		{
			pthread_mutex_lock(&dataMutex);
			if(receivedAllReplies)
			{
				gettimeofday(&tv, NULL);
				end = tv.tv_sec*1000 + tv.tv_usec/1000;
				usingCS = true;
				waiting = false;

				if(minMessages==0)
					minMessages = requestCount+replyCount;
				
				noCSEntries++;
				ss.str(std::string());
				ss<<myId<<"\t CS#"<<noCSEntries<<"\t ENTERING \t Time Elapsed(milliseconds)="<<end-begin<<
					"\t No. Request="<<requestCount<<"\t No. Replies"<<replyCount<<"\n";
				logger(CS_RECORD, ss.str());
				usleep(20000);

				ss.str(std::string());
				ss<<myId<<"\t CS#"<<noCSEntries<<"\t LEAVING \n";
				logger(CS_RECORD, ss.str());

				pthread_mutex_unlock(&dataMutex);
				break;
			}
			else
			{
				pthread_mutex_unlock(&dataMutex);
			}
		}

		pthread_mutex_lock(&dataMutex);    
		usingCS = false;
		pkt = (CriticalSectionPacket*)malloc(sizeof(CriticalSectionPacket));
		if(pkt != NULL)
		{  
			pkt->msgType = CS_REPLY;
			pkt->seqId = 0;
			pkt->nodeId = myId;
			for(int i=0; i<MAX_NODES; i++)
			{
				if(deferredReplies[i] && i != myId)
				{
					deferredReplies[i] = false;
					receivedReplies[i] = false;
					receivedAllReplies = false;

					sendMessage = packetToMessage(pkt);
					sendMessage +="\0";
					ss.str(std::string());
					ss<<i<<" message-"<<sendMessage<<" num bytes sent="<<strlen(sendMessage.c_str())+1<<"\n";
					logger(DEBUG, "DEBUG: processCriticalSection(), sending deferred reply message to node "+ss.str());

					//Note-Num bytes sent will be less than 56 bytes. Hence not performing check on the return value
					send(sockDesc[i], sendMessage.c_str(), strlen(sendMessage.c_str())+1, 0);
				}
			}
			free(pkt);
			pthread_mutex_unlock(&dataMutex);
		}
		else
		{
			pthread_mutex_unlock(&dataMutex);
			logger(DEBUG, "ERROR: Malloc for CriticalSectionPacket during deferred reply processing Failed \n");
		}
		minMessages = std::min(minMessages, (requestCount + replyCount));
		maxMessages = std::max(maxMessages, (requestCount + replyCount));
	}
	
	//Send completion message to node 0 and close all the sockets to other nodes
	pthread_mutex_lock(&dataMutex);
	cout<<"Min Messages = "<<minMessages<<" Max Messages = "<<maxMessages<<" Total Messages = "<<totalMessages<<endl;
	cout<<"Going in Completion Mode \n";
	
	if(myId != 0)
	{
		pkt = (CriticalSectionPacket*)malloc(sizeof(CriticalSectionPacket));
		if(pkt!= NULL)
		{
			pkt->msgType = CS_COMPLETION;
			pkt->seqId = 0;
			pkt->nodeId = myId;

			sendMessage = packetToMessage(pkt);
			sendMessage +="\0";
			ss.str((std::string()));
			ss<<"message-"<<sendMessage<<" num bytes sent="<<strlen(sendMessage.c_str())+1<<"\n";
			logger(DEBUG, "DEBUG: processCriticalSection(), sending completion message to Node 0 "+ss.str());
			
			send(sockDesc[0], sendMessage.c_str(), strlen(sendMessage.c_str())+1, 0);

			free(pkt);
		}
		else
		{
			logger(DEBUG, "ERROR: Malloc for CriticalSectionPacket when sending completion message Failed \n");
		}
	}
	else
	{
		completionStatus[0] = true;
	}
	pthread_mutex_unlock(&dataMutex);
	cout<<"EXIT - processCriticalSection() thread \n";
	logger(DEBUG, "EXIT - processCriticalSection() \n");
	pthread_exit(0);
}

void logger(LogType type, string str)
{
	switch(type)
	{
		case DEBUG:
			pthread_mutex_lock(&fileMutex);
			dfStream.open(nodeDebugFile.c_str(), ios::out | ios::app);
			if(dfStream.is_open())
			{
				dfStream<<str;
				//cout<<str;
				dfStream.close();
			}
			else
			{
				cout<<"Unable to open debug file="<<nodeDebugFile<<endl;
			}
			pthread_mutex_unlock(&fileMutex);
			break;
		case CS_RECORD:
			//pthread_mutex_lock(&fileMutex);
			csfStream.open(CS_LOG_FILE, ios::out | ios::app);
			if(csfStream.is_open())
			{
				cout<<str;
				csfStream<<str;
				csfStream.close();
			}
			//pthread_mutex_unlock(&fileMutex);
			break;
		default:
			cout<<"Invalid Log Type Received"<<endl;
			break;
	}
}

string packetToMessage(CriticalSectionPacket* pkt)
{
	std::stringstream sStream;
	sStream.str(std::string());
	if(pkt)
	{
		sStream<<pkt->msgType<<","<<pkt->seqId<<","<<pkt->nodeId;
	}

	return sStream.str();
}

CriticalSectionPacket messageToPacket(string msg)
{
	CriticalSectionPacket retPkt;
	string token;
	string toks[3];
	std::istringstream ss(msg);
	int i=0;

	while(std::getline(ss, token, ','))
	{
		toks[i] = token;
		i++;
	}

	retPkt.msgType = (Message)atoi(toks[0].c_str());
	retPkt.seqId = atoi(toks[1].c_str());
	retPkt.nodeId = atoi(toks[2].c_str());

	return retPkt;
}

int uniformDistGenerator(int min, int max)
{
	srand(time(NULL));
	return ((rand()/(RAND_MAX +1.0))*(max-min)+min);
}

