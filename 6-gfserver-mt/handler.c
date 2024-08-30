#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "gfserver.h"
#include "content.h"

#define BUFFER_SIZE 	4096
#define PATH_BUFF_SIZE	128

ssize_t handler_get(gfcontext_t* ctx, char* path, void* arg);


//------------ Global PThread Resources -------------//
pthread_mutex_t gMutex    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gMutexCtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  gCond     = PTHREAD_COND_INITIALIZER;

//----------------- Queue Type ------------------//
// items are added at the rear and poppoed off of the front
typedef struct queue
{
	char** 		  paths;
	gfcontext_t** ctx;
	int		front;
	int 	rear;
	int		numItem;
	int   	capacity;
} queue_t;
static queue_t theQ;

//-------- GetNextIdx --------//
int GetNextIdx( int curIdx, int size )
{
	return ( (curIdx + 1) % size );
}

//-------- QueueInit --------//
void QueueInit( int size )
{
	int i = 0 ;

	theQ.front	  = 0;
	theQ.rear  	  = size - 1;
	theQ.capacity = size; 
	theQ.numItem  = 0;

	theQ.paths = (char**)malloc(size*sizeof(char*));
	theQ.ctx   = (gfcontext_t**)malloc(size*sizeof(gfcontext_t*));

	for (i=0; i<size; ++i)
	{
		theQ.paths[i] = (char*)malloc(PATH_BUFF_SIZE*sizeof(char));
	}
}

//-------- QueueEnq --------//
void QueueEnq( char* path, gfcontext_t* ctx )
{
	theQ.rear = GetNextIdx( theQ.rear, theQ.capacity );
	strcpy( theQ.paths[theQ.rear], path ); 
	theQ.ctx[theQ.rear] = ctx;
	theQ.numItem++;
}

//-------- QueueDeq --------//
int QueueDeq( void )
{
	int idx    = theQ.front;
	theQ.front = GetNextIdx(theQ.front, theQ.capacity);
	theQ.numItem--;

	return idx;
}

//-------- isQueueEmpty --------//
int isQueueEmpty( void )
{
	if (theQ.numItem == 0)
		return 1;

	return 0;
}

//-------- isQueueFull --------//
int isQueueFull( void )
{
	if (theQ.numItem >= theQ.capacity)
		return 1;

	return 0;
}

//-------- QueueCleanup --------//
void QueueCleanup( void )
{
	free( *theQ.paths );
	free( theQ.paths );
	free( theQ.ctx );
}


//-------------- Worker Thread Callback ------------------//
void *workerFunc(void *threadArgument) 
{
	int tID 	 	= *( (int*)threadArgument );
	int pathIdx = 0;
	char buffer[PATH_BUFF_SIZE] = {0};
	int  result = 0;

	fprintf(stdout, "Thread %d\n", tID);

	while(1)
	{
		pthread_mutex_lock(&gMutex);

		while ( isQueueEmpty() )
			pthread_cond_wait( &gCond, &gMutex );

		pathIdx = QueueDeq();
		strcpy(buffer, theQ.paths[pathIdx]);		
		pthread_mutex_unlock(&gMutex);		

		result = handler_get( theQ.ctx[pathIdx], buffer, NULL);
		if (result < 0)
		{
			printf("handle error\n");
		}

	}

	pthread_exit(0);
}


//-------------- boss_handler  ------------------//
ssize_t boss_handler(gfcontext_t* ctx, char* path, void* arg)
{
	//fprintf(stdout, "The Boss\n");
	// enqueue the request
	pthread_mutex_lock(&gMutex);
	QueueEnq(path, ctx);
	pthread_mutex_unlock(&gMutex);

	while ( isQueueFull() );
	pthread_cond_signal(&gCond);

	return 0;
}


//-------------- single_threaded handler  ------------------//
ssize_t handler_get(gfcontext_t* ctx, char* path, void* arg)
{
	int fildes;
	size_t file_len, bytes_transferred;
	ssize_t read_len, write_len;
	char buffer[BUFFER_SIZE];

	if( 0 > (fildes = content_get(path)))
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);

	/* Calculating the file size */
	file_len = lseek(fildes, 0, SEEK_END);

	gfs_sendheader(ctx, GF_OK, file_len);

	/* Sending the file contents chunk by chunk. */
	bytes_transferred = 0;
	while(bytes_transferred < file_len)
	{
		read_len = pread(fildes, buffer, BUFFER_SIZE, bytes_transferred);
		if (read_len <= 0)
		{
			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
			gfs_abort(ctx);
			return -1;
		}
		write_len = gfs_send(ctx, buffer, read_len);
		if (write_len != read_len)
		{
			fprintf(stderr, "handle_with_file write error");
			gfs_abort(ctx);
			return -1;
		}
		bytes_transferred += write_len;
	}

	return bytes_transferred;
}

