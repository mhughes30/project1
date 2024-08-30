
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if 0
/*
 * Structs exported from netinet/in.h (for easy reference)
 */

/* Internet address */
struct in_addr {
  unsigned int s_addr; 
};

/* Internet style socket address */
struct sockaddr_in  {
  unsigned short int sin_family; /* Address family */
  unsigned short int sin_port;   /* Port number */
  struct in_addr sin_addr;	 /* IP address */
  unsigned char sin_zero[...];   /* Pad to size of 'struct sockaddr' */
};

/*
 * Struct exported from netdb.h
 */

/* Domain name service (DNS) host entry */
struct hostent {
  char    *h_name;        /* official name of host */
  char    **h_aliases;    /* alias list */
  int     h_addrtype;     /* host address type */
  int     h_length;       /* length of address */
  char    **h_addr_list;  /* list of addresses */
}
#endif

typedef unsigned short uint16_t;

#define BUFFSIZE        16
#define MIN_PORT_NUM    1025
#define MAX_PORT_NUM    65535

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoserver [options]\n"                                                    \
"options:\n"                                                                  \
"  -h                  Show this help message\n"                              \
"  -n                  Maximum pending connections\n"                         \
"  -p                  Port (Default: 8080)\n"

//--------------------- OPTIONS DESCRIPTOR ------------------------//
static struct option gLongOptions[] = 
{
        {"port",        required_argument, NULL, 'p'},
        {"maxnpending", required_argument, NULL, 'n'},
        {"help",        no_argument,       NULL, 'h'},
        {NULL, 0,                          NULL, 0}
};

//----------- Function Prototypes --------------//
int   SetUpTCPConnection(void);
int   ProcessConnections(int socket);
void  ProcessCmdArgs(int argc, char* argv[]);


static char     message[BUFFSIZE];
static uint16_t port                = 8080;
static int      messageLen          = BUFFSIZE;
static int      maxNumPending       = 5;

//------------------ Main ----------------------//
int main(int argc, char *argv[]) 
{
    int serverSockFD = 0;
    int status = 0;
    
    ProcessCmdArgs(argc, argv);
    
    serverSockFD = SetUpTCPConnection();
    if (serverSockFD < 0)
    {
        exit(1);
    }  

    status = ProcessConnections(serverSockFD);
    if (status < 0)
    {
        exit(1);
    }       
    
    return 0;
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
        messageLen = recv(clientSockFD, message, BUFFSIZE, 0);    
        send(clientSockFD, message, messageLen, 0);
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
    while ( (option = getopt_long(argc, argv, "p:n:h", gLongOptions, NULL)) != -1 ) 
    {
        switch (option) 
        {
            case 'p': // listen-port
                port = atoi(optarg);
                break;
            case 'n': // server
                maxNumPending = atoi(optarg);
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

    if ( (port < 1025) || (port > 65535) ) 
    {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, port);
        exit(1);
    }

    if (maxNumPending < 1) 
    {
        fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__, maxNumPending);
        exit(1);
    }    
}