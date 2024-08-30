#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>

#include "workload.h"
#include "gfclient.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webclient [options]\n"                                                     \
"options:\n"                                                                  \
"  -h                  Show this help message\n"                              \
"  -n [num_requests]   Requests download per thread (Default: 1)\n"           \
"  -p [server_port]    Server port (Default: 8080)\n"                         \
"  -s [server_addr]    Server address (Default: 0.0.0.0)\n"                   \
"  -t [nthreads]       Number of threads (Default 1)\n"                       \
"  -w [workload_path]  Path to workload file (Default: workload.txt)\n"       \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = 
{
  {"server",        required_argument,      NULL,           's'},
  {"port",          required_argument,      NULL,           'p'},
  {"workload-path", required_argument,      NULL,           'w'},
  {"nthreads",      required_argument,      NULL,           't'},
  {"nrequests",     required_argument,      NULL,           'n'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};


#define BUFFER_SIZE 		4096
#define PATH_BUFF_SIZE	256

char 		    *server    = "localhost";
unsigned short port 	   = 8080;
int			   reqDoneCntr = 0;			// keeps track of the # of requests completed


// for allocating and defining the worker thread pool
pthread_t* 	  	 	 workerThreads;
int*		  		 threadIDs;
pthread_mutex_t gMutex    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gMutexReq = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  gCond     = PTHREAD_COND_INITIALIZER;

// worker thread callback
void *workerFunc(void *threadArgument);

int  GetRequestCounter( void );
void IncRequestCounter( void );


//----------------- Queue Type ------------------//
// items are added at the rear and popped off of the front
// the data in this queue is specific to the client requirements. 
typedef struct queue
{
	char** reqPaths;
	char** locPaths;
	FILE** files;

	int	front;
	int rear;
	int	numItem;
	int capacity;
} queue_t;
static queue_t theQ;
static void QueueInit( int size );
static int  QueueDeq( void );
static void QueueEnq( char* path, FILE* file, char* locPath );
static int  isQueueEmpty( void );
static int  isQueueFull( void );
void		QueueCleanup( void );


//------------ Usage -------------//
static void Usage() 
{
	fprintf(stdout, "%s", USAGE);
}

//------------ localPath -------------//
static void localPath(char *req_path, char *local_path)
{
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

//------------ openFile -------------//
static FILE* openFile(char *path)
{
  	char *cur, *prev;
  	FILE *ans;

  	/* Make the directory if it isn't there */
  	prev = path;
    while(NULL != (cur = strchr(prev+1, '/')))
	{
		*cur = '\0';

    	if (0 > mkdir(&path[0], S_IRWXU))
		{
			if (errno != EEXIST)
			{
        		perror("Unable to create directory");
        		exit(EXIT_FAILURE);
			}
    	}

    	*cur = '/';
    	prev = cur;
  	}

	if( NULL == (ans = fopen(&path[0], "w")))
	{
		perror("Unable to open file");
    	exit(EXIT_FAILURE);
  	}

  return ans;
}

/* Callbacks ========================================================= */
static void writecb(void* data, size_t data_len, void *arg)
{
  FILE *file = (FILE*) arg;

  fwrite(data, 1, data_len, file);
}


//----------- GetRequestCounter ---------------//
int GetRequestCounter( void )
{
	static int count;

	pthread_mutex_lock(&gMutexReq);
	count = reqDoneCntr;
	pthread_mutex_unlock(&gMutexReq);

	return count;
}

//----------- IncRequestCounter ---------------//
void IncRequestCounter( void )
{
	pthread_mutex_lock(&gMutexReq);
	++reqDoneCntr;
	pthread_mutex_unlock(&gMutexReq);
}


//----------- Worker Thread: workerFunc ---------------//
void *workerFunc(void *threadArgument)
{
	int   tID 	  = *( (int*)threadArgument );
	int   pathIdx = 0;
	char  path[PATH_BUFF_SIZE] = {0};
	char  locPath[PATH_BUFF_SIZE] = {0};
	FILE* curFile;
	int   returncode;

	gfcrequest_t* gfr;

	fprintf(stdout, "Thread %d\n", tID);

	while(1)
	{
		//------ dequeue the current task with a mutex lock for the queue access
		pthread_mutex_lock(&gMutex);

		while ( isQueueEmpty() )
			pthread_cond_wait( &gCond, &gMutex );

		pathIdx = QueueDeq();
		strcpy(path, theQ.reqPaths[pathIdx]);	
		strcpy(locPath, theQ.locPaths[pathIdx]);	
		curFile = theQ.files[pathIdx];

		pthread_mutex_unlock(&gMutex);

		//------ create the client requester
    	gfr = gfc_create();
    	gfc_set_server(gfr, server);
    	gfc_set_path(gfr, path);
    	gfc_set_port(gfr, port);
		gfc_set_writefunc(gfr, writecb);
    	gfc_set_writearg(gfr, curFile);

		fprintf(stdout, "Requesting %s%s\n", server, path);
    	if ( 0 > (returncode = gfc_perform(gfr)))
		{
			fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
			fclose(curFile);
			if ( 0 > unlink(locPath) )
				fprintf(stderr, "unlink failed on %s\n", locPath);
		}
		else 
		{
			fclose(curFile);
    	}

    	if ( gfc_get_status(gfr) != GF_OK)
		{
			if ( 0 > unlink(locPath) )
				fprintf(stderr, "unlink failed on %s\n", locPath);
    	}

    	fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(gfr)));
    	fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(gfr), gfc_get_filelen(gfr));

    	gfc_cleanup(gfr);

		IncRequestCounter();	// report that this this current tasks
	}

	pthread_exit(0);
}


//------------------------------- Main --------------------------------------//
int main(int argc, char **argv) 
{
	char *workload_path = "workload.txt";
  
  	int i;
  	int option_char = 0;
  	int nrequests 	= 1;
  	int nthreads 	= 1;
	int numReqTotal = 0;
  	FILE *file;
  	char *req_path;
  	char local_path[512];

  	// Parse and set command line arguments
  	while ( (option_char = getopt_long(argc, argv, "s:p:w:n:t:h", gLongOptions, NULL)) != -1 ) 
	{
	    switch (option_char) 
		{
		case 's': // server
			server = optarg;
			break;
   	    case 'p': // port
			port = atoi(optarg);
			break;
      	case 'w': // workload-path
			workload_path = optarg;
			break;
      	case 'n': // nrequests
			nrequests = atoi(optarg);
			break;
      	case 't': // nthreads
			nthreads = atoi(optarg);
			break;
      	case 'h': // help
			Usage();
			exit(0);
			break;                      
      	default:
			Usage();
			exit(1);
    	}
  	}

	if( EXIT_SUCCESS != workload_init(workload_path))
	{
		fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
    	exit(EXIT_FAILURE);
  	}

  	gfc_global_init();

	numReqTotal = nrequests * nthreads;

	//------- initialize the pool of worker threads
	QueueInit( nthreads + 2 );
	workerThreads = (pthread_t*)malloc( nthreads*sizeof(pthread_t) );
	threadIDs     = (int*)malloc( nthreads*sizeof(int) );
	for (i=0; i<nthreads; ++i)
	{
		threadIDs[i] = i;
		pthread_create( &workerThreads[i], NULL, workerFunc, &threadIDs[i] );
	}


  	//------ Boss Thread: enqueues requests for worker threads
  	for (i = 0; i < numReqTotal; ++i)
	{
		req_path = workload_get_path();

		if (strlen(req_path) > PATH_BUFF_SIZE)
		{
			fprintf(stderr, "Request path exceeded maximum of 256 characters\n.");
			exit(EXIT_FAILURE);
    	}

    	localPath(req_path, local_path);
    	file = openFile(local_path);

		// enqueue the request
		pthread_mutex_lock(&gMutex);
		QueueEnq( req_path, file, local_path );
		pthread_mutex_unlock(&gMutex);

		while ( isQueueFull() );
		pthread_cond_signal(&gCond);
	}

	//------ wait for the number of requests to be completed. 
	while ( numReqTotal > GetRequestCounter() );


  	gfc_global_cleanup();
	//QueueCleanup();
	
	return 0;
}  


//-------- GetNextIdx --------//
static int GetNextIdx( int curIdx, int size )
{
	return ( (curIdx + 1) % size );
}

//-------- QueueInit --------//
static void QueueInit( int size )
{
	int i = 0;

	theQ.front	  = 0;
	theQ.rear  	  = size - 1;
	theQ.capacity = size; 
	theQ.numItem  = 0;

	theQ.reqPaths = (char**)malloc(size*sizeof(char*));
	theQ.locPaths = (char**)malloc(size*sizeof(char*));
	theQ.files    = (FILE**)malloc(size*sizeof(FILE*));

	for (i=0; i<size; ++i)
	{
		theQ.reqPaths[i] = (char*)malloc(PATH_BUFF_SIZE*sizeof(char));
		theQ.locPaths[i] = (char*)malloc(PATH_BUFF_SIZE*sizeof(char));
	}
}

//-------- QueueEnq --------//
static void QueueEnq( char* path, FILE* file, char* locPath )
{
	theQ.rear = GetNextIdx( theQ.rear, theQ.capacity );
	strcpy( theQ.reqPaths[theQ.rear], path ); 
	strcpy( theQ.locPaths[theQ.rear], locPath ); 
	theQ.files[theQ.rear] = file;
	theQ.numItem++;
}

//-------- QueueDeq --------//
static int QueueDeq( void )
{
	int idx    = theQ.front;
	theQ.front = GetNextIdx(theQ.front, theQ.capacity);
	theQ.numItem--;

	return idx;
}

//-------- isQueueEmpty --------//
static int isQueueEmpty( void )
{
	if (theQ.numItem == 0)
		return 1;

	return 0;
}

//-------- isQueueFull --------//
static int isQueueFull( void )
{
	if (theQ.numItem >= theQ.capacity)
		return 1;

	return 0;
}

//-------- QueueCleanup --------//
void QueueCleanup( void )
{
	free( *theQ.reqPaths );
	free( theQ.reqPaths );
	free( *theQ.locPaths );
	free( theQ.locPaths );
	free( theQ.files );
}








