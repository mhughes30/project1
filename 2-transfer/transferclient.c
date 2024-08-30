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


#define BUFFSIZE        4096
#define HOSTSIZE        64
#define FILESIZE        64
#define MIN_PORT_NUM    1025
#define MAX_PORT_NUM    65535
#define DEF_PORT        8080


//---------- Purpose ---------//
// 1) connects to server
// 2) receives data in chunks
// 3) saves resulting data to a file on disk. 


//-------------- USAGE macro ------------//
#define USAGE                                                                 \
"usage:\n"                                                                    \
"  transferclient [options]\n"                                                \
"options:\n"                                                                  \
"  -h                  Show this help message\n"                              \
"  -o                  Output file (Default foo.txt)\n"                       \
"  -p                  Port (Default: 8080)\n"                                \
"  -s                  Server (Default: localhost)\n"

//------------- OPTIONS DESCRIPTOR -------------/
static struct option gLongOptions[] = 
{
	{"server", required_argument, NULL, 's'},
	{"port",   required_argument, NULL, 'p'},
	{"output", required_argument, NULL, 'o'},
	{"help",   no_argument,       NULL, 'h'},
	{NULL, 0,                     NULL, 0}
};

//----------- Function Prototypes --------------//
int   SetUpTCPConnection(void);
int 	ReceiveFile(int socket);
void  ProcessCmdArgs(int argc, char* argv[]);


//------------- Static Variables --------------//
static char     dataBuffer[BUFFSIZE];
static char     hostName[HOSTSIZE] = "localhost";
static char     fileName[FILESIZE] = "foo.txt";
static uint16_t port               = DEF_PORT;


//------------------ Main ----------------------//
int main(int argc, char **argv) 
{
	ProcessCmdArgs(argc, argv);
  
	int socketFD =  SetUpTCPConnection();
	if (socketFD < 0)
	{
	   exit(1);
	}

	int fileSizeBytes = ReceiveFile(socketFD);
	if (fileSizeBytes < 0)
	{
		fprintf(stderr, "%s @ %d: Unable to receive file\n", __FILE__, __LINE__); 
	}


	fprintf(stderr, "file size %d\n", fileSizeBytes);
    
   //if (close(socketFD) < 0)
   //{
   //   fprintf(stderr, "%s @ %d: close() socket failed\n", __FILE__, __LINE__); 
   //   return 0;
   //}
    
	return 0;
}



//------------------ SetUpTCPConnection ----------------------//
int ReceiveFile(int socket)
{
	char* buffPtr  	  = &dataBuffer[0];
	int curRxSize  	  = 0;
	int curBytesWritten = 0;
	int totalSize  	  = 0;
	int writeFD    	  = 0;
	
	memset(&dataBuffer[0], 0, BUFFSIZE);

	// open file with read/write permission, and create it if it doesn't exist
	writeFD = open( fileName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR ); 	
	if (writeFD < 0)
	{
		fprintf(stderr, "%s @ %d: open() file failed\n", __FILE__, __LINE__); 
	}

	// recieve the file in chunks
	while(1)
	{
		curRxSize = recv( socket, buffPtr, BUFFSIZE, 0 );  
	  	if (curRxSize < 0)
    	{
      	fprintf(stderr, "%s @ %d: recv()failed with %d\n", __FILE__, __LINE__, curRxSize);          
			return -1;    
		}		
		else if (curRxSize == 0)
		{
			break;
		}

		totalSize += curRxSize;

		// write Rx data to a file
		curBytesWritten = write( writeFD, buffPtr, curRxSize );
		if (curBytesWritten < 0)
		{
			fprintf(stderr, "%s @ %d: write() file failed\n", __FILE__, __LINE__); 
			return -1;
		}
	}

	return totalSize;
}




//------------------ SetUpTCPConnection ----------------------//
int SetUpTCPConnection(void)
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
    server = gethostbyname(hostName);
    if (server == NULL)
    {
        fprintf(stderr, "%s @ %d: gethostbyname() failed\n", __FILE__, __LINE__);
        return -1;               
    }   
    
    //---- server address
    bzero( (char*)&servAddr, sizeof(servAddr) );
    bcopy( (char*)server->h_addr, (char*)&servAddr.sin_addr.s_addr, server->h_length );
    servAddr.sin_family         = AF_INET;
    servAddr.sin_port           = htons(port);  
    
    status = connect( socketFD, (struct sockaddr*)&servAddr, sizeof(servAddr) );
    if (status < 0)
    {
        fprintf(stderr, "%s @ %d: connect() failed\n", __FILE__, __LINE__);
        return -1;                       
    }
    
    return socketFD;    
}


//------------------ ProcessCmdArgs ----------------------//
void ProcessCmdArgs(int argc, char* argv[])
{
	int option = 0;

	// Parse and set command line arguments
	while ( (option = getopt_long(argc, argv, "s:p:o:h", gLongOptions, NULL)) != -1 ) 
	{
		switch (option) 
		{
		case 's': // server
	        strcpy(hostName, optarg);
            break;
        case 'p': // listen-port
            port = atoi(optarg);
            break;
        case 'o': // filename
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

	if (NULL == hostName) 
	{
		fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
		exit(1);
	}
}
