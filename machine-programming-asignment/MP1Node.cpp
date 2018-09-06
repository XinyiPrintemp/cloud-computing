/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
    return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}



void MP1Node::addNodeToMemberList(int id, short port, long heartbeat, long timestamp) {
    Address newNodeAddress = getNodeAddress(id, port);
    MemberListEntry* newEntry = new MemberListEntry(id, port, heartbeat, timestamp);
    memberNode->memberList.push_back(*newEntry);
    #ifdef DEBUGLOG
        log->logNodeAdd(&memberNode->addr, &newNodeAddress);
    #endif
    delete newEntry;
}

MemberListEntry* MP1Node::getNodeInMemberListTable(int id){
     MemberListEntry* entry = NULL;
    
    for(auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it) {  
        if(it->id == id) {
            entry = &(*it);
            break;
        }
    }
    
    return entry;
}


void MP1Node::sendJOINREPMsg(Address *destinationAddr) {
    size_t memberListEntrySize = sizeof(int) + sizeof(short) + sizeof(long) + sizeof(long);

    size_t msgsize = sizeof(MessageHdr) + sizeof(int) + (memberNode->memberList.size() * memberListEntrySize);  //why sizeof(int)
    
    MessageHdr* msg = (MessageHdr*) malloc(msgsize * sizeof(char));  //important
    msg -> msgType = JOINREP;

    serializeMemberListTableForJOINREPMessageSending(msg);

    emulNet->ENsend(&memberNode->addr, destinationAddr, (char*)msg, msgsize);

    free(msg);  //important
}

void MP1Node::sendHEARTBEATMsg(Address *destinationAddr) {
    size_t msgsize = sizeof(MessageHdr)  + sizeof(destinationAddr->addr) + sizeof(long) + 1;
    
    MessageHdr* msg = (MessageHdr*) malloc(msgsize * sizeof(char));  
    msg -> msgType = HEARBEAT;
    memcpy((char*)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
    memcpy((char*)(msg+1) + sizeof(&memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));
    

    emulNet->ENsend(&memberNode->addr, destinationAddr, (char*)msg, msgsize);

    free(msg); 
}


/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	//MessageHdr* msg = (MessageHdr *)data;
    
    MessageHdr*  msg = (MessageHdr*) malloc(size * sizeof(char));
    memcmp(msg, data, sizeof(MessageHdr));
    
    if(msg->msgType == JOINREQ) {
        //parse msg 
        int id; short port; long heartbeat;
        memcpy(&id, data + sizeof(MessageHdr), sizeof(int));
        memcpy(&port, data + sizeof(MessageHdr) + sizeof(int), sizeof(short));
        memcpy(&heartbeat, data + sizeof(MessageHdr) + sizeof(int) + sizeof(short), sizeof(long));

        addNodeToMemberList(id, port, heartbeat, memberNode->timeOutCounter);

        Address newNodeAddress = getNodeAddress(id, port);

        sendJOINREPMsg(&newNodeAddress);

    }
    else if(msg->msgType == JOINREP) {
        deserializeMemberListTableForJOINREPMessageReceiving(data);
    }
    else if(msg->msgType == HEARBEAT) {
        // Read message data
        int id;
        short port;
        long heartbeat;
        memcpy(&id, data + sizeof(MessageHdr), sizeof(int));
        memcpy(&port, data + sizeof(MessageHdr) + sizeof(int), sizeof(short));
        memcpy(&heartbeat, data + sizeof(MessageHdr) + sizeof(int) + sizeof(short), sizeof(long));

        MemberListEntry*  entry = getNodeInMemberListTable(id);
        if(entry == NULL){
            addNodeToMemberList(id, port, heartbeat, memberNode->timeOutCounter);
        }
        else{
            entry->setheartbeat(heartbeat);
            entry->settimestamp(memberNode->timeOutCounter);
        }
    }
    return  true;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

	/*
	 * Your code goes here
	 */
     if(memberNode->pingCounter == 0) {
        memberNode->heartbeat++;
        
        // Send heartbeat messages to all nodes
        for(auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it) {  
            Address nodeAddress = getNodeAddress(it->id, it->getport());
           
            if(!isAddressEqualToNodeAddress(&nodeAddress)) {
                sendHEARTBEATMsg(&nodeAddress);
            }
        }
        
        // Reset ping counter
        memberNode->pingCounter = TFAIL;
    }
    else {
        memberNode->pingCounter--;
    }
    for(auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); ++it) {  
        Address nodeAddress = getNodeAddress(it->id, it->getport());
        if(!isAddressEqualToNodeAddress(&nodeAddress)) {
            if(memberNode->timeOutCounter - it->timestamp > TREMOVE) {
                memberNode->memberList.erase(it);
                #ifdef DEBUGLOG
                log->logNodeRemove(&memberNode->addr, &nodeAddress);
                #endif

                //break;
            }
        }
    }
    
    memberNode->timeOutCounter ++;

    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

Address MP1Node::getNodeAddress(int id, short port) {
    Address nodeaddr;

    memset(&nodeaddr, 0, sizeof(Address));
    *(int*)(&nodeaddr.addr) = id;
    *(short*)(&nodeaddr.addr[4]) = port;

    return nodeaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

bool MP1Node::isAddressEqualToNodeAddress(Address *address) {
    return (memcmp((char*)&(memberNode->addr.addr), (char*)&(address->addr), sizeof(memberNode->addr.addr)) == 0); 
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}

void MP1Node::serializeMemberListTableForJOINREPMessageSending(MessageHdr *msg) {
    int numItems = memberNode->memberList.size();
    memcpy((char *)(msg + 1), &numItems, sizeof(int));

    int offsize = sizeof(int);

    for (auto it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++) {
        memcpy((char*)(msg + 1 + offsize), &it->id, sizeof(int));
        offsize += sizeof(int);

        memcpy((char*)(msg + 1 + offsize), &it->port, sizeof(short));
        offsize += sizeof(short);

        memcpy((char*)(msg + 1 + offsize), &it->heartbeat, sizeof(long));
        offsize += sizeof(long);

        memcpy((char*)(msg + 1 + offsize), &it->timestamp, sizeof(long));
        offsize += sizeof(int);
    }
}

void MP1Node::deserializeMemberListTableForJOINREPMessageReceiving(char *data) {
    // Read message data
    int numberOfItems;
    memcpy(&numberOfItems, data + sizeof(MessageHdr), sizeof(int));
        
    // Deserialize member list entries
    int offset = sizeof(int);
        
    for(int i = 0; i < numberOfItems; i++) {           
        int id;
        short port;
        long heartbeat;
        long timestamp;
            
        memcpy(&id, data + sizeof(MessageHdr) + offset, sizeof(int));
        offset += sizeof(int);
        
        memcpy(&port, data + sizeof(MessageHdr) + offset, sizeof(short));
        offset += sizeof(short);
            
        memcpy(&heartbeat, data + sizeof(MessageHdr) + offset, sizeof(long));
        offset += sizeof(long);
            
        memcpy(&timestamp, data + sizeof(MessageHdr) + offset, sizeof(long));
        offset += sizeof(long);
             
        // Create and insert new entry
        addNodeToMemberList(id, port, heartbeat, timestamp);
    }
}
