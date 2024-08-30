#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h> 
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>

#include "gfclient.h"

typedef unsigned char BOOL;
enum
{
	FALSE = 0,
	TRUE
};

typedef void (*writeFuncPtr)(void*, size_t, void*);
typedef void (*headerFuncPtr)(void*, size_t, void*);

#define BUFFSIZE    4096
#define HOSTSIZE	128
#define PATHSIZE  	256
#define HEADERSIZE	128

static const char STAT_OK[]	   	 = "OK";
static const char STAT_FILE[]	 = "FILE_NOT_FOUND";
static const char STAT_ERROR[]   = "ERROR";
static const char STAT_INVALID[] = "INVALID";

// unchanging arguments for the client request
static const char REQ_CMD[]    = "GETFILE GET ";
static const char HEAD_END[]   = "\r\n\r\n";
static const int  REQ_CMD_LEN  = 12; 
static const int  HEAD_END_LEN = 4; 

static char dataBuffer[BUFFSIZE] 	 	= {0};

typedef struct gfchead_t
{
	gfstatus_t 	responseStatus;		
	int 		fileLenBytes;
} gfchead_t;
static gfchead_t headStruct = {GF_INVALID, 0};


// NOTE: this struct is used as an opaque pointer at the API/interface level. 
// This struct is a "handle".
struct gfcrequest_t
{
	char	   		server[HOSTSIZE];		// the server name, i.e. "localhost"
	char		    reqPath[PATHSIZE];		// the path of the file that is requested from the server
	int				pathLen;				// lenght of the reqPath string	
	uint16_t 		port;						
	FILE*			writeFile;				// file handle for the file to be written upon server response
	writeFuncPtr	writeFunc;				// function pointer for writing "writeFile"
	int				rxBytes;				// actual number of bytes received (excluding the header)
	char			header[HEADERSIZE];		// buffer storing the response header
	headerFuncPtr	headerFunc;				// function pointer for header parsing
	int				headerLen;				// length of the header
	gfchead_t*		gfcHead;	
};


//-------- Static Function Prototypes ----------//
static int  gfc_SetUpTCPConnection(gfcrequest_t* gfr);
static void gfc_sendHeader(gfcrequest_t* gfr, int socket);
static void gfc_parseRxHeader(void* buffer, size_t buffLen, void* headerArg);
static int  gfc_extractHeader(gfcrequest_t *gfr, char* buffPtr, int rxSize);


//------------- SetUpTCPConnection -------------//
static int gfc_SetUpTCPConnection(gfcrequest_t* gfr)
{
    int socketFD     = 0;
    int status       = 0;
    
    struct sockaddr_in  servAddr;   // server address
    struct hostent*     server;     // host computer properties
    
    //---- socket creation
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0)
    {
        fprintf(stderr, "%s @ %d: socket() failed\n", __FILE__, __LINE__);
        return -1;              
    }	

    //---- server information creation
    server = gethostbyname(gfr->server);
    if (server == NULL)
    {
        fprintf(stderr, "%s @ %d: gethostbyname() failed\n", __FILE__, __LINE__);
        return -1;               
    }   

    //---- server address
    bzero( (char*)&servAddr, sizeof(servAddr) );
    bcopy( (char*)server->h_addr, (char*)&servAddr.sin_addr.s_addr, server->h_length );
    servAddr.sin_family         = AF_INET;
    servAddr.sin_port           = htons(gfr->port);  

    status = connect( socketFD, (struct sockaddr*)&servAddr, sizeof(servAddr) );
    if (status < 0)
    {
        fprintf(stderr, "%s @ %d: connect() failed\n", __FILE__, __LINE__);
        return -1;                       
    }
    
    return socketFD;    
}


//------------ gfc_cleanup -------------//
void gfc_cleanup(gfcrequest_t *gfr)
{
	free(gfr);
}


//------------ gfc_create -------------//
gfcrequest_t *gfc_create()
{
	gfcrequest_t* gfr = malloc( sizeof(gfcrequest_t) );
	memset( gfr, 0, sizeof(gfcrequest_t) );

	return gfr;
}


//------------ gfc_get_bytesreceived -------------//
size_t gfc_get_bytesreceived(gfcrequest_t *gfr)
{
    return (gfr->rxBytes);
}


//----------- gfc_get_filelen ---------//
size_t gfc_get_filelen(gfcrequest_t *gfr)
{
    return (gfr->gfcHead->fileLenBytes);
}


//----------- gfc_get_status ---------//
gfstatus_t gfc_get_status(gfcrequest_t *gfr)
{
    return (gfr->gfcHead->responseStatus);
}


//----------- gfc_global_init ---------//
void gfc_global_init()
{
}


//----------- gfc_global_cleanup ---------//
void gfc_global_cleanup()
{
}


static void gfc_sendHeader(gfcrequest_t* gfr, int socket)
{
	// build the request command, "GETFILE GET <pathToFile>
	int  headIdx = 0;
	char reqHeader[HEADERSIZE] = {0};
	strcpy( &(reqHeader[headIdx]), REQ_CMD);
	headIdx += REQ_CMD_LEN;
	strcpy( &(reqHeader[headIdx]), gfr->reqPath );
	headIdx += gfr->pathLen;
	strcpy( &(reqHeader[headIdx]), HEAD_END );
	headIdx += HEAD_END_LEN;
	
	// transmit the request
	send(socket, &(reqHeader[0]), headIdx, 0);
}


//-------------- gfc_parseRxHeader --------------//
// makes use of strtok for separating strings based upon separator string
// parses a "GETFILE <status> <fileLength>\r\n\r\n string
// if status is FILE_NOT_FOUND or ERROR, no fileLength is sent
static void gfc_parseRxHeader(void* buffer, size_t buffLen, void* headerArg)
{
	char* curStr;
	const char  key[]  = " \r\n";

	gfchead_t* head = (gfchead_t*)headerArg;

	// initialize to default invalid case
	head->fileLenBytes   = 0;
	head->responseStatus = GF_INVALID;	

	// "GETFILE"
	curStr = strtok( buffer, key );
	if (curStr == NULL)
	{
		return;
	}

	// <status>
	curStr = strtok( NULL, key );
	if (curStr == NULL)
	{
		return;
	}	

	// determine the status type
	if ( strcmp(curStr, STAT_OK) == 0 )
	{
		head->responseStatus = GF_OK;
	}
	else if ( strcmp(curStr, STAT_FILE) == 0 )
	{
		head->responseStatus = GF_FILE_NOT_FOUND;
		return;
	}
	else if ( strcmp(curStr, STAT_ERROR) == 0 )
	{
		head->responseStatus = GF_ERROR;
		return;
	}
	else
	{
		return;
	}

	// get the file length
	curStr = strtok( NULL, key );
	if (curStr == NULL)
	{
		head->responseStatus = GF_INVALID;	
		return;
	}	

	head->fileLenBytes = atoi(curStr);
}


//----------- gfc_perform ---------//
int gfc_perform(gfcrequest_t *gfr)
{
	BOOL gotHeader = FALSE;	
	BOOL rxFailed  = FALSE;

	// set the callback details for response header parsing
	gfc_set_headerfunc(gfr, gfc_parseRxHeader);
	gfc_set_headerarg(gfr, (void*)&headStruct);

	// connection setup
	int socketFD =  gfc_SetUpTCPConnection(gfr);
   if (socketFD < 0)
   {
	   return -1;
   }

	// send the request
	gfc_sendHeader(gfr, socketFD);

	// receive the response in chunks
	char* buffPtr = &dataBuffer[0];
	int curRxSize = 0;
	int totalSize = 0;
	while(1)
	{
		buffPtr = &dataBuffer[0];
		curRxSize = recv( socketFD, buffPtr, BUFFSIZE, 0 );  
		printf("rx: %d ", curRxSize);
	  	if (curRxSize < 0)
    	{
      	fprintf(stderr, "%s @ %d: file recv()failed with %d\n", __FILE__, __LINE__, curRxSize);    
			gfr->gfcHead->responseStatus = GF_INVALID;
			rxFailed = TRUE;
			break;
		}		
		else if (curRxSize == 0)
		{
			rxFailed = TRUE;
			break;
		}		

		// check for the header
		if ( gotHeader == FALSE)
		{
			curRxSize = gfc_extractHeader(gfr, buffPtr, curRxSize );
			if (gfr->gfcHead->responseStatus == GF_INVALID)
			{
				fprintf(stderr, "%s @ %d: invalid response\n", __FILE__, __LINE__); 
				rxFailed = TRUE;  
				break;
			}
			buffPtr += gfr->headerLen;
			gotHeader = TRUE;
		}	

		// write Rx data to a file
		gfr->writeFunc( buffPtr, curRxSize, gfr->writeFile );

		totalSize += curRxSize;
		if (totalSize >= gfr->gfcHead->fileLenBytes)
		{
			break;
		}		
	}

	close (socketFD);
	gfr->rxBytes = totalSize;

	if (rxFailed == TRUE)
		return -1;

   return 0;
}


//-------------- gfc_extractHeader --------------//
static int gfc_extractHeader(gfcrequest_t *gfr, char* buffPtr, int rxSize)
{
	for (int i=0; i<rxSize; ++i)
	{
		if (buffPtr[i] == '\r')
		{
			gfr->headerLen = i + HEAD_END_LEN;
			break;
		}
	}
	// get the header
	memcpy(gfr->header, buffPtr, gfr->headerLen);
	// parse the received header
	gfr->headerFunc( gfr->header, gfr->headerLen, gfr->gfcHead );
	
	rxSize  -= gfr->headerLen;

	return rxSize;
}


//----------- gfc_set_headerarg ---------//
// assigns the 3rd argument of the header callback function
void gfc_set_headerarg(gfcrequest_t *gfr, void *headerarg)
{
	gfr->gfcHead = headerarg;
}


//----------- gfc_set_headerfunc ---------//
// assigns the header callback function to use
void gfc_set_headerfunc(gfcrequest_t *gfr, void (*headerfunc)(void*, size_t, void *))
{
	gfr->headerFunc = headerfunc;
}


//----------- gfc_set_path ---------//
void gfc_set_path(gfcrequest_t *gfr, char* path)
{
	strcpy(gfr->reqPath, path);
	gfr->pathLen = strlen(gfr->reqPath);
}


//----------- gfc_set_port ---------//
void gfc_set_port(gfcrequest_t *gfr, unsigned short port)
{
	gfr->port = port;
}


//----------- gfc_set_server ---------//
void gfc_set_server(gfcrequest_t *gfr, char* server)
{
	strcpy(gfr->server, server);
}


//----------- gfc_set_writearg ---------//
void gfc_set_writearg(gfcrequest_t *gfr, void *writearg)
{
	gfr->writeFile = writearg;
}


//----------- gfc_set_writefunc ---------//
void gfc_set_writefunc(gfcrequest_t *gfr, void (*writefunc)(void*, size_t, void *))
{
	gfr->writeFunc = writefunc;
}


//----------- gfc_strstatus ---------//
char* gfc_strstatus(gfstatus_t status)
{
	switch(status)
	{
		case GF_OK:
			return (char*)STAT_OK;
		case GF_FILE_NOT_FOUND:
			return (char*)STAT_FILE;
		case GF_ERROR:
			return (char*)STAT_ERROR;
		case GF_INVALID:
			return (char*)STAT_INVALID;
	}

   return (char*)NULL;
}







