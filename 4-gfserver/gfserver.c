
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h> 
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>

#include "gfserver.h"


#define BUFFSIZE     4096
#define PATHSIZE  	256
#define HEADERSIZE	MAX_REQUEST_LEN

typedef unsigned char  BOOL;
enum
{
	FALSE = 0,
	TRUE
};

static char dataBuffer[BUFFSIZE] = {0};

typedef ssize_t (*rxHandleFuncPtr)(gfcontext_t*, char*, void*);

static const char CMD[]			= "GETFILE ";
static const char STAT_OK[]	 	= "OK ";
static const char STAT_FILE[]	= "FILE_NOT_FOUND";
static const char STAT_ERROR[] 	= "ERROR";
static const char HEAD_END[]   	= "\r\n\r\n";

static const int  CMD_LEN  	   	 = 8; 
static const int  HEAD_END_LEN   = 4; 
static const int  STAT_ERROR_LEN = 5; 
static const int  STAT_FILE_LEN  = 14; 
static const int  STAT_OK_LEN    = 3; 

// NOTE: this struct is used as an opaque pointer at the API/interface level. 
// This struct is a "handle".
// Purpose: contains server-specific parameters, independent of the request
struct gfserver_t
{
	uint16_t 		port;	
	int				maxPending;					
	rxHandleFuncPtr	handleFunc;
	void*			handleArg;
};

// NOTE: this struct is used as an opaque pointer at the API/interface level. 
// This struct is a "handle".
// Purpose: contains contextual information for a particular connection and request
struct gfcontext_t
{
	int 		clientSockFD;
	char		reqPath[PATHSIZE];	// the path of the file that is requested from the server
	int			pathLen;			// length of the reqPath string	
	gfstatus_t  reqStatus;			//	the status of the current request
};
static gfcontext_t contextStruct = {0};


//----------- Function Prototypes --------------//
static int  gfs_SetUpTCPConnection(gfserver_t* gfs);
static void gfs_parseRxHeader(void* buffer, size_t buffLen, gfcontext_t* gfc);
static int  gfs_getRequest( char* buffPtr, int buffSize, int socket );


//------------------ gfs_parseRxHeader ----------------------//
static void gfs_parseRxHeader(void* buffer, size_t buffLen, gfcontext_t* gfc)
{
	char* curStr;
	const char  key[]  = " \r";

	// initialize to default case
	gfc->pathLen   = 0;
	gfc->reqStatus = GF_FILE_NOT_FOUND;	

	fprintf(stderr, " rxHead: %s ", (char*)buffer);

	// "GETFILE"
	curStr = strtok( buffer, key );
	if (curStr == NULL)
	{		
		return;
	}
	else if ( strcmp(curStr, "GETFILE") != 0)
	{
		return;
	}

	// method = "GET"
	curStr = strtok( NULL, key );
	if (curStr == NULL)
	{
		return;
	}	
	else if ( strcmp(curStr, "GET") != 0)
	{
		return;
	}

	// get the requested file path
	curStr = strtok( NULL, key );
	if (curStr == NULL)
	{
		return;
	}	
	// check for proper format; must have "\" in front
	if (curStr[0] != 0x2F)	// Note: ox2F is ASCII for "\"
	{
		return;
	}

	gfc->reqStatus = GF_OK;	
	
	gfc->pathLen = strlen(curStr);
	memset(gfc->reqPath, 0, PATHSIZE); 
	memcpy(gfc->reqPath, curStr, gfc->pathLen);
	//printf("len: %d\n", gfc->pathLen);
	//printf("reqPath: %s\n", gfc->reqPath);
}


//------------------ gfs_SetUpTCPConnection ----------------------//
static int gfs_SetUpTCPConnection(gfserver_t* gfs)
{
	struct sockaddr_in servAddr;
	int socketFD = 0;
	int status   = 0;   
	int yes      = 1;
    
    //---- create server socket file descriptor
    socketFD = socket(AF_INET, SOCK_STREAM, 0 );
    if (socketFD < 0)
    {
       fprintf(stderr, "%s @ %d: socket() failed\n", __FILE__, __LINE__);
       return -1;              
    }
    
    //---- set socket options --> allow for socket reuse    
    status = setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if (status < -1)
	{
        fprintf(stderr, "%s @ %d: setsockopt() failed\n", __FILE__, __LINE__);
        return -1;       
	}
    
	//---- server address
	bzero( (char*)&servAddr, sizeof(servAddr) );
	servAddr.sin_family         = AF_INET;
	servAddr.sin_port           = htons(gfs->port);
	servAddr.sin_addr.s_addr    = htonl(INADDR_ANY);
    
	//---- bind socket to server address
	status = bind(socketFD, (struct sockaddr*)&servAddr, sizeof(servAddr) );
	if (status < 0)
	{
       fprintf(stderr, "%s @ %d: bind() failed, port %d\n", __FILE__, __LINE__, gfs->port);
       return -1;            
	}
    
	return socketFD;
}


//------------ gfs_abort -------------//
void gfs_abort(gfcontext_t* ctx)
{
	close(ctx->clientSockFD);
}


//-------------- gfserver_create --------------//
gfserver_t* gfserver_create()
{
	gfserver_t* gfs = malloc( sizeof(gfserver_t) );
	memset( gfs, 0, sizeof(gfserver_t) );

	return gfs;
}


//------------ gfs_send -------------//
ssize_t gfs_send(gfcontext_t* ctx, void* data, size_t len)
{
	char* buffPtr = (char*)data;

	int bytesSent = send(ctx->clientSockFD, buffPtr, len, 0);

   return bytesSent;
}


//------------ gfs_sendheader -------------//
ssize_t gfs_sendheader(gfcontext_t* ctx, gfstatus_t status, size_t file_len)
{
	ctx->reqStatus = status;

	// build the response command, "GETFILE <status> <fileLength> <end>
	int  headIdx = 0;
	char reqHeader[HEADERSIZE] = {0};
	strcpy( &(reqHeader[headIdx]), CMD);
	headIdx += CMD_LEN;

	int statusLen = STAT_OK_LEN;
	const char* statStr = STAT_OK;
	if (status == GF_FILE_NOT_FOUND)
	{
		statStr   = STAT_FILE;
		statusLen = STAT_FILE_LEN;
	}
	else if (status == GF_ERROR)
	{
		statStr   = STAT_ERROR;
		statusLen = STAT_ERROR_LEN;
	}
	strcpy( &(reqHeader[headIdx]), statStr );
	headIdx += statusLen;

	// add the file length
	if (status == GF_OK)
	{
		char fileLenStr[64] = {0};
		sprintf( fileLenStr, "%lu", file_len);
		int fileLenStrLen = strlen(fileLenStr);
		strcpy( &(reqHeader[headIdx]), fileLenStr );
		headIdx += fileLenStrLen;	
	}

	strcpy( &(reqHeader[headIdx]), HEAD_END );
	headIdx += HEAD_END_LEN;
	
	//fprintf(stderr, " txHead: %s ", reqHeader);
	// transmit the request
	send(ctx->clientSockFD, &(reqHeader[0]), headIdx, 0);


   return 0;
}


//------------ gfserver_serve -------------//
void gfserver_serve(gfserver_t* gfs)
{

	int servSockFD = gfs_SetUpTCPConnection(gfs);
	if (servSockFD < 0)
	{
	   exit(1);
	}  

	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);

	// can't fail for a valid socket
	listen(servSockFD, gfs->maxPending);

	while(1)
	{
		// blocks until a client connects
		contextStruct.clientSockFD = accept(servSockFD, (struct sockaddr*)&clientAddr, &clientLen);
		if (contextStruct.clientSockFD < 0)
		{
			fprintf(stderr, "%s @ %d: accept() failed\n", __FILE__, __LINE__);
		}

		// received the request
		char* buffPtr = &dataBuffer[0];
		int 	size 	  = gfs_getRequest( buffPtr, BUFFSIZE, contextStruct.clientSockFD );

		//int size = recv( contextStruct.clientSockFD, buffPtr, BUFFSIZE, 0 );
		//fprintf(stderr, " size: %d\n", size );

		// parse the request
		gfs_parseRxHeader(buffPtr, size, &contextStruct);
		fprintf(stderr, " status: %d\n", contextStruct.reqStatus );
		if (contextStruct.reqStatus == GF_FILE_NOT_FOUND)
		{
			fprintf(stderr, " malformed request ");
			gfs_sendheader(&contextStruct, contextStruct.reqStatus, 0);	
		}
		else
		{
			// get the data and send the response
			int status = gfs->handleFunc( &contextStruct, contextStruct.reqPath, gfs->handleArg );
			if (status < 0)
			{
				fprintf(stderr, " file handler failed ");
			}
		}
	}
}


//------------ gfs_getRequest -------------//
// NOTE: this function makes sure an entire request is received, before progressing to the Rx header parser
static int gfs_getRequest( char* buffPtr, int buffSize, int socket )
{
	int   size = 0;
	char* ch; 

	while (1)
	{
		size = recv( socket, buffPtr, buffSize, 0 );
		// determine if the whole request has been received
		ch = strchr(buffPtr, '\r');
		if (ch != NULL)
		{
			break;
		}
		buffPtr += size;
	}

	return size;
}



//------------ gfserver_set_handlerarg -------------//
void gfserver_set_handlerarg(gfserver_t* gfs, void* arg)
{
	gfs->handleArg = arg;
}


//------------ gfserver_set_handler -------------//
void gfserver_set_handler(gfserver_t* gfs, ssize_t (*handler)(gfcontext_t*, char*, void*))
{
	gfs->handleFunc = handler;
}


//------------ gfserver_set_maxpending -------------//
void gfserver_set_maxpending(gfserver_t* gfs, int max_npending)
{
	gfs->maxPending = max_npending;
}


//------------ gfserver_set_port -------------//
void gfserver_set_port(gfserver_t* gfs, unsigned short port)
{
	gfs->port = port;
}





