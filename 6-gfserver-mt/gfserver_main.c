#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <pthread.h>

#include "gfserver.h"
#include "content.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  gfserver_main [options]\n"                                                 \
"options:\n"                                                                  \
"  -h                  Show this help message.\n"                             \
"  -c [content_file]   Content file mapping keys to content files\n"          \
"  -p [listen_port]    Listen port (Default: 8080)\n"                         \
"  -t [nthreads]       Number of threads (Default: 1)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = 
{
	{"port",          required_argument,      NULL,           'p'},
	{"content",       required_argument,      NULL,           'c'},
    {"nthreads",      required_argument,      NULL,           't'},
    {"help",          no_argument,            NULL,           'h'},
    {NULL,            0,                      NULL,             0}
};


//-------- externs from handler.c
//extern ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg);
// worker thread callback
extern void 	*workerFunc(void *threadArgument);
// boss thread callback
extern ssize_t boss_handler(gfcontext_t* ctx, char* path, void* arg);
// initialization of the global thread queue
extern void 	QueueInit( int size );
// cleans up queue dynamic memory
extern void		QueueCleanup( void );

// for allocating and defining the worker thread pool
pthread_t* 	  workerThreads;
int*		  threadIDs;


//------------------- _sig_handler -------------------------//
static void _sig_handler(int signo)
{
	if (signo == SIGINT || signo == SIGTERM)
	{
	   exit(signo);
	}
}


//------------------- Main -------------------------//
int main(int argc, char **argv) 
{
	int option_char  = 0;
	int i 			 = 0;
	int nthreads 	 = 1;	
	unsigned short port = 8080;
  	char *content = "content.txt";
  	gfserver_t *gfs;	

    if (signal(SIGINT, _sig_handler) == SIG_ERR)
	{
		fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    	exit(EXIT_FAILURE);
  	}

  	if (signal(SIGTERM, _sig_handler) == SIG_ERR)
	{
	   fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
	   exit(EXIT_FAILURE);
  	}

  	// Parse and set command line arguments
  	while ((option_char = getopt_long(argc, argv, "p:t:c:h", gLongOptions, NULL)) != -1) 
	{
		switch (option_char) 
		{
      	case 'p': // listen-port
        	port = atoi(optarg);
        	break;
      	case 't': // nthreads
        	nthreads = atoi(optarg);
        	break;
      	case 'c': // file-path
        	content = optarg;
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

  	/* not useful, but it ensures the initial code builds without warnings */
  	if (nthreads < 1) 
	{
	   nthreads = 1;
  	}
  
  	content_init(content);

  	/*Initializing server*/
  	gfs = gfserver_create();
  	gfserver_set_port(gfs, port);
  	gfserver_set_maxpending(gfs, 100);
  	//gfserver_set_handler(gfs, handler_get);
  	gfserver_set_handler(gfs, boss_handler);
  	gfserver_set_handlerarg(gfs, NULL);

	// Initialize global pthreads resources
	QueueInit( nthreads + 2 );
	workerThreads = (pthread_t*)malloc( nthreads*sizeof(pthread_t) );
	threadIDs     = (int*)malloc( nthreads*sizeof(int) );
	for (i=0; i<nthreads; ++i)
	{
		threadIDs[i] = i;
		pthread_create( &workerThreads[i], NULL, workerFunc, &threadIDs[i] );
	}

  	/*Loops forever*/
  	gfserver_serve(gfs);

	QueueCleanup();	// probably never reached
}





