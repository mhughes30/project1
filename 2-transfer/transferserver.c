#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
// socket-specific
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
// cmd-arg parsing
#include <getopt.h>
// file stuff
#include <sys/stat.h>


typedef unsigned short uint16_t;

#define BUFFSIZE        16
#define MIN_PORT_NUM    1025
#define MAX_PORT_NUM    65535
#define FILESIZE        64
#define DEF_PORT        8080

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  transferserver [options]\n"                                                \
"options:\n"                                                                  \
"  -f                  Filename (Default: bar.txt)\n"                         \
"  -h                  Show this help message\n"                              \
"  -p                  Port (Default: 8080)\n"

//--------------------- OPTIONS DESCRIPTOR ------------------------//
static struct option gLongOptions[] = 
{
	{"filename", required_argument, NULL, 'f'},
	{"port",     required_argument, NULL, 'p'},
	{"help",     no_argument,       NULL, 'h'},
	{NULL, 0,                       NULL, 0}
};


//----------- Function Prototypes --------------//
int   SetUpTCPConnection(void);
int   ProcessConnections(int socket);
void  ProcessCmdArgs(int argc, char* argv[]);


//------------- Static Variables --------------//
static char     dataBuffer[BUFFSIZE] = {0};
static char     fileName[FILESIZE]  = "bar.txt";
static uint16_t port                = DEF_PORT;
static int      maxNumPending       = 5;


//------------------ Main ----------------------//
int main(int argc, char **argv) 
{
	ProcessCmdArgs(argc, argv);

	int serverSockFD = SetUpTCPConnection();
	if (serverSockFD < 0)
	{
	   exit(1);
	}  

	int status = ProcessConnections(serverSockFD);
	if (status < 0)
	{
		exit(1);
	}       
    
	return 0;
}


//------------------ SendFile ----------------------//
int SendFile(int socket)
{
	char* buffPtr    = &dataBuffer[0];
	int curTxSize    = 0;
	int curBytesSent = 0;
	int totalSize    = 0;
	int readFD       = 0;

	// open file with read/write permission
	readFD = open( fileName, O_RDWR, S_IRUSR | S_IWUSR ); 	
	if (readFD < 0)
	{
		fprintf(stderr, "%s @ %d: open() file failed\n", __FILE__, __LINE__); 
	}	

	// send the file in chunks
	while(1)
	{
		// read in the file
		curTxSize = read( readFD, buffPtr, BUFFSIZE );
		if (curTxSize < 0)
		{
			fprintf(stderr, "%s @ %d: read() file failed\n", __FILE__, __LINE__); 
			return -1;
		}
		else if (curTxSize == 0)
		{
			break;
		}	

		//printf("curTxSize: %s\n", dataBuffer);

		curBytesSent = send( socket, buffPtr, curTxSize, 0 );  
	  	if (curBytesSent < 0)
    	{
      	fprintf(stderr, "%s @ %d: send()failed with %d\n", __FILE__, __LINE__, curBytesSent);          
			return -1;    
		}		

		totalSize += curTxSize;
	}

	return totalSize;
}


//------------------ ProcessConnections ----------------------//
int ProcessConnections(int socket)
{
    int clientSockFD = 0;
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    // can't fail for a valid socket
    listen(socket, maxNumPending);
    
    while(1)
    {
        // blocks until a client connects
        clientSockFD = accept(socket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSockFD < 0)
        {
            fprintf(stderr, "%s @ %d: accept() failed\n", __FILE__, __LINE__);
            return -1;   
        }

		  SendFile(clientSockFD);
		
		  close(clientSockFD);
    }    
}


//------------------ SetUpTCPConnection ----------------------//
int SetUpTCPConnection(void)
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
	servAddr.sin_port           = htons(port);
	servAddr.sin_addr.s_addr    = htonl(INADDR_ANY);
    
	//---- bind socket to server address
	status = bind(socketFD, (struct sockaddr*)&servAddr, sizeof(servAddr) );
	if (status < 0)
	{
       fprintf(stderr, "%s @ %d: bind() failed, port %d\n", __FILE__, __LINE__, port);
       return -1;            
	}
    
	return socketFD;
}


//------------------ ProcessCmdArgs ----------------------//
void ProcessCmdArgs(int argc, char* argv[])
{
	int option;

	// Parse and set command line arguments   
	while ( (option = getopt_long(argc, argv, "f:p:h", gLongOptions, NULL)) != -1 ) 
	{
		switch (option) 
		{
      	case 'p': // listen-port
      	   port = atoi(optarg);
            break;
         case 'f': // file name
            strcpy(fileName, optarg);
            break;
         case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
         default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
		}
	}

	if (NULL == fileName) 
	{
		fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
		exit(1);
	}

	if ( (port < MIN_PORT_NUM) || (port > MAX_PORT_NUM) ) 
	{
		fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, port);
		exit(1);
	}
}







