#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// socket-specific
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
// cmd-arg parsing
#include <getopt.h>         

typedef unsigned short uint16_t;

// Be prepared accept a response of this length
#define BUFFSIZE        16
#define MIN_PORT_NUM    1025
#define MAX_PORT_NUM    65535
#define DEF_PORT        8080
#define DEF_MSG         "Hello World!"
#define ASCII_BACKSLASH 0x5C

//-------------- USAGE macro ------------//
#define USAGE                                                                 \
"usage:\n"                                                                    \
"  echoclient [options]\n"                                                    \
"options:\n"                                                                  \
"  -h                  Show this help message\n"                              \
"  -m                  Message to send to server (Default: \"Hello World!\"\n"\
"  -p                  Port (Default: 8080)\n"                                \
"  -s                  Server (Default: localhost)\n"

//------------- OPTIONS DESCRIPTOR -------------/
static struct option gLongOptions[] = 
{
        {"server",  required_argument, NULL, 's'},
        {"port",    required_argument, NULL, 'p'},
        {"message", required_argument, NULL, 'm'},
        {"help",    no_argument,       NULL, 'h'},
        {NULL, 0,                      NULL, 0}
};

//----------- Function Prototypes --------------//
int   SetUpTCPConnection(void);
void  ProcessCmdArgs(int argc, char* argv[]);
void  ReceiveData(int socket);
void  TransmitData(int socket);


static char     hostName[]         = "localhost";
static char     message[BUFFSIZE];
static uint16_t port               = DEF_PORT;
static int      messageLen         = BUFFSIZE;


//------------------ Main ----------------------//
int main(int argc, char *argv[]) 
{
    strcpy(message, DEF_MSG);
    ProcessCmdArgs(argc, argv);

    /* Socket Code Here */
    int socketFD =  SetUpTCPConnection();
    if (socketFD < 0)
    {
        exit(1);
    }
    
    TransmitData(socketFD);
    ReceiveData(socketFD);
    
    if (close(socketFD) < 0)
    {
        fprintf(stderr, "%s @ %d: close() socket failed\n", __FILE__, __LINE__); 
        return 0;
    }
    
    return 0;
}


//------------------ TransmitData ----------------------//
void TransmitData(int socket)
{
	send(socket, message, messageLen, 0);
}


//------------------ ReceiveData ----------------------//
void ReceiveData(int socket)
{
    memset(&message[0], 0, BUFFSIZE);
	messageLen = recv( socket, message, BUFFSIZE, 0 );  

    if (messageLen <= 0)
    {
       fprintf(stderr, "%s @ %d: recv()failed with %d\n", __FILE__, __LINE__, messageLen);          
    }
   
    printf("%s", message);    
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
    int  option  = 0;
    int len = 0;
    // Parse and set command line arguments
    while ( (option = getopt_long(argc, argv, "s:p:m:h", gLongOptions, NULL)) != -1 ) 
    {
        switch (option) 
        {
        case 's': // server
            strcpy(hostName, optarg);
            break;
        case 'p': // listen-port
            port = atoi(optarg);
            break;
        case 'm': // server
            len = strlen(optarg);
            if (len > BUFFSIZE-1)
            {
                fprintf(stderr, "%s @ %d: invalid message\n", __FILE__, __LINE__);
                exit(1);
            }
            strcpy(message, optarg);
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
    
    if (NULL == message) 
    {
        fprintf(stderr, "%s @ %d: invalid message\n", __FILE__, __LINE__);
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