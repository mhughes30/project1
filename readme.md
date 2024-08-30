# Project README file

Author: Mike Hughes (gte769h)


## Project Description

This project consists of multiple parts, called "echo", "transfer", and parts 1 and 2 of implementing the Getfile protocol.

### echo project (socket warm-up)
- __Summary Description__
This portion consisted of creating a simple TCP client and server. My design focused on encapsulating everything in short methods, for example 
SetUpTCPConnection(), which I used throughout the entire project. I also made an effort to eliminate all magic numbers, by using statically global 
constants or macros. The echo project followed the relatively standard design for implementing a simple TCP echo server. 
I chose to use recv() and send() instead of read() and write(), although either would have worked.

- __What created problems for you?__
The only problems I had with this project was getting it to pass on the test server.
When I use printf, I habitually add '\n', and it took me the longest time to realize this was causing the client echo test to fail. 
Also, I originally used a header file, which contained shared constants, and header file includes. However, this prevented the auto-grader from compiling the code. 
For the server, I had "bind()" failures only on the auto-grader, which I believe were due to the socket being unavailable, preventing the "bind()" call from working.
I added the socket option of SOL_REUSEADDR, and this problem went away. 

- __What tests would you have added to the test suite?__
I manually tested this code, then made use of the autograder json output, by outputing extra data to stdout. 
The manual testing (which could be automated) involved testing the command options, various messages, and ports, especially around the edge conditions. 

- __If you were going to do this project again, how would you improve it?__
I am happy with how the "echo" project turned out. It is a "warm-up" exercise, and it provided me a refresher on socket programming. There is nothing I would do differently. 

- __If you didn't think something was clear in the documentation, what would you write instead?__
One thing that provided a little confusion was the buffer size used. In the direcitons, a buffer size of 16 was specified, but the source files in the echo folder had a buffer size of 4096. 
So, for a brief while I wasn't sure if the auto-grader tests were failing because I changed the buffer size from 4096 to 16. 
Other than that, the instructions were so simple that I overcomplicated things a bit.

### transfer project (socket warm-up)
- __Summary Description__
This portion consisted of creating a TCP client and server, that passed the contents of a file between the 2 of them.
I reused some of the code developed in the echo project, mainly SetUpTCPConnection(). 

The added complexity in the transfer project stems from sending and receiving a file. On the client side, in ReceiveFile(), a file
is opened, followed by a loop in which every recv() has its buffered contents appended to the file. Once recv() returns 0 bytes, it 
is assumed that the file transfer has completed. 

On the server side, I used SendFile() to handle the file transfer to the client. This implementation was essentially the inverse of the 
client ReceiveFile() implementation. A file is opened, followed by a while loop, in which a chunk of the file is read(), and 
passed to the client via send(). Once read() returns 0 bytes, it is assumed that all of the files content has been read. 

- __What created problems for you?__
I had no issues with the "transfer" project. I built upon the code I developed for the "echo" project, adding in the file open, read, and write mechanisms.

- __What tests would you have added to the test suite?__
I manually tested this code, then made use of the autograder json output, by outputing extra data to stdout. 
The manual testing (which could be automated) involved testing the command options, various messages, and ports, especially around the edge conditions. 

- __If you were going to do this project again, how would you improve it?__
I am happy with how the "transfer" project turned out. It is a "warm-up" exercise, and it provided me a refresher on socket programming. There is nothing I would do differently. 

- __If you didn't think something was clear in the documentation, what would you write instead?__
I believe the instructions were clear. 

### Part1: Implementing the Getfile Protocol
- __Summary Description__
This portion consisted of the single-threaded implementation of the GETFILE protocol. 

This portion of the project, involved many more design decisions than the previous portions, despite having a provided API to implement. 

On the client side, the primary design decisions involved creating the data structures, and implementing the callback set by
gfc_set_headerfunc() and gfc_set_headerarg(). 

For the gfc_request_t struct, I added a lot of content, including the server name,
requested file path, length of the file path, the server port, a FILE pointer for the file to be written, a function pointer for
the writing of the file, the number of received bytes, a buffer for the server response header, a function pointer for the
header parsing function, the length of the header, and a pointer to another struct I created called gfchead_t. All of this data
is relevant for several parts of the client API which take a gfcrequest_t struct as an argument. 

For the gfchead_t struct, I added the server response status, and the expected file length sent by the server. 

For the parsing of the header received from the server, I created gfc_parseRxHeader, which simply splits the header string into sections, using 
strtok() and the key/token of " \r\n". This function also determined the integer representation of the response status, and used atoi()
to convert the string file length to an integer. 

On the server side, the primary design decisions involved creating the data structures, and the client header parser. 

For the gfserver_t struct, I added the following data members: server port, max number of pending connections, a function pointer 
for the handler callback and a pointer for the handler callback arugment, as of yet not defined. I intended for the gfserver_t
struct to contain only server-specific details, required throughout the API.

For the gfcontext_t struct, I added the following data members: the client socket resulting from accept(), the requested file path, 
the length of the requested file path, and the status of the client request. I intended for the gfcontext_t
struct to contain only client-request-specific details.

For parsing the client header, I created gfs_parseRxHeader(), which operates similarly to gfc_parseRxHeader on the client side, 
making use of strtok. However, this parser adds checks for a malformed request, specifically for the "GETFILE" portion, "GET"
portion, and making sure that a '\' is the first character in the file path, as specified in the protocol. 

Initially, I assumed that the complete client request would be received in a single recv() command. However, I found this to 
be unreliable. Therefore, I created the function gfs_getRequest(), which loops on recv() until a '\r' character is found in the 
response, indicating the end-of-request marker. 

- __What created problems for you?__
I had an issue with the client that was due to my client code looping on recv() for too long. I didn't break out of the loop, based upon the received file size 
equalling the file size reported in the server response. I was initially breaking out of the loop only when recv() returned 0. After I fixed, this I had 
no other problems with the client code. 

I personally found the server implementation easier than that of the client. The only problem I had with the server involved receiving the client request. 
I assumed the server would receive the entire client request in one recv() command, but I found this to not be the case, based upon the autograder JSON report. 
Therefore, I added the gfs_getRequest() function, which assumes nothing. It loops until the it finds the "end of request" marker in the received request. 

- __What tests would you have added to the test suite?__
I think the test suite does a pretty good job. It certainly caught issues that I didn't discover in my own local testing. 

On the server test, my JSON output showed that the autograder client requested \foo.jpg every time. I think testing with different path lengths and files 
could have been useful. I did test with different files on my local testing. 

- __If you were going to do this project again, how would you improve it?__
I am happy with how my implementation of the GETFILE protocol turned out. However, I think the handlers are a little strange, because I don't know 
why the implementation for parsing the header (on the client) would be user-setable variable. The header parser requires knowledge of 
implementation details, that the user of the GETFILE library shouldn't have to know. 

I thought it is odd that a "malformed client request" results in a FILE_NOT_FOUND error. Some of the malformed request issues have nothing 
to do with the file name. It might be good to have a MALFORMED_REQUEST error as well. 

- __If you didn't think something was clear in the documentation, what would you write instead?__
I think the documentation is good. It explains the problem without explicitly providing a step-by-step guide, requiring
the student to think. 

### Part2: Implementing a Multithreaded Getfile Server and Client
- __Summary Description__
This portion consisted of the multi-threaded implementation with the GETFILE protocol. The GETFILE API was used as a library, with 
implementation details hidden. 

The primary challened in this portion was using pthreads properly, including the use of mutexes and signals. 

I also implemented my own queue data structure, and associated functions, for both the client and server. The queue was used in order to
implement the boss-worker multithreading pattern. The boss thread assigns tasks to worker threads, by placing the required task 
information for the thread in the queue. The boss thread then signals a worker thread, telling it a new task is ready. The worker 
thread than grabs the required task information from the globally accessible queue. For all globally shared variables, a mutex is used. 

My primary aid for this portion of the project was the "High-level code design of multi-threaded GetFile server and client", provided in the 
"Helpful Hints" section of the Readme file for the project. This hint provided an outline for implementing the multithreaded server
and client. 

For the server boss thread, I wrote boss_handler() to be passed as the callback function for the gfserver_set_handler(). The boss_handler() simply
enqueues the file path and gfcontext_t pointer to the queue, whithin a mutex. It waits until the queue is not full, and then sends a
pthread signal. 

For the server work threads, I wrote workerFunc(), which acquires the mutex, waits for a signal from the boss, and dequeus the file path and gfcontext_t 
pointer. It then uses the already provided handler_get() function for sending the requested file to the client. 

For the client boss thread, I placed its mechanism inside of main(). The boss thread runs until all requests are completed. During this
time, it acquires the mutex, enqueues the task data (file path, local path, and FILE handle) to the task queue, and sends a signal
to the worker threads, once the queue is not full - in order to prevent the boss from placing a request on a full queue. After 
enqueuing data and signaling worker threads for all tasks, the boss thread then waits for all request to be completed. 

Each worker thread increments a counter everytime it completes a task. Once this counter equals the number of requested tasks, 
the boss thread knows all of the work is done. The global task-completion counter is protected by another mutex, separate than that used for the 
task queue. 

The worker threads acquire a mutex, wait for a signal from the boss thread, and dequeue the task data. After this,
the worker thread creates its own gfcrequest_t struct, and uses all of the gfclient API required for setting 
parameters, and performing the file request. Once the request is completed gfc_cleanup() is called followed by 
incrementing the task-completion counter. 

- __What created problems for you?__
I had few issues with this portion of the project. One thing I found troubling was the gfcontext_t opaque pointer, used in the server.
This pointer must be passed to handler_get() in order to process a request. However, this data associated with this pointer
can't be physically copied, becasue the pointer is opaque. Therefore, every thread sees the same gfcontext_t. I didn't protect gfcontext_t usage by a mutex,
because doing so would have made each thread essentially run to completion before another thread could run, defeating the purpose
of multithreading. Fortunately, not protecting the gfcontext_t usage by a mutex caused no issues; the content of the gfcontext_t struct must truly be the same
for each thread. Of course, I have no idea what is in gfcontext_t in the API version for this portion of the projectl; I only know its content in my own implementation.

- __What tests would you have added to the test suite?__
I would have added some tests that push the memory limits. For example, how large of a queue can be handled (my queue size was
a function of the number of threads requested)? How large of a file can reliably be handled? How large of a file path name can be 
handled? 

- __If you were going to do this project again, how would you improve it?__
I am happy with how this portion of the project turned out. I was surprised by how easy pthreads is to use, at least
in a simple application. However, I would have liked to gotten around the globally shared gfcontext_t opaque pointer. As explained earlier, 
this variable could not have its content copied (because its opaque), and couldn't be protected by a mutex (because that would make each
thread essentially run to completion before another thread could run). However, I still placed the gfcontext_t pointer in the task queue, which had its
access protected by a mutex. 

- __If you didn't think something was clear in the documentation, what would you write instead?__
I thought everything was clear. I found the "helpful hints" section, which provided a high-level design to be very helpful. 


## Known Bugs/Issues/Limitations

__Please tell us what you know doesn't work in this submission__

I believe that everything works in my submissions. The code passed my own tests as well as the auto-grader's test consistently. 

## References

### echo project
I used the provided references from the Readme.md file, in particular the socket tutorial at www.cs.rpi.edu. 
I also used some TCP/UDP socket code that I wrote several years ago as a reference. 

### transfer project
I built upon the code I wrote in the "echo" project, so this was my primary reference. Also, I consulted the linux man pages for file I/O operations using open(), read(), and write(). 
In the past, I have always used a "FILE" pointer and fopen()-styled commands, so this was new to me. 

### Part1: Implementing the Getfile Protocol
I built upon the code I wrote in the "transfer" project, so this was my primary reference. Also, I consulted examples for 
usage of strtok() for parsing the request and response headers. 
The reference I used is here: http://www.tutorialspoint.com/c_standard_library/c_function_strtok.htm

### Part2: Implementing a Multithreaded Getfile Server and Client
For the queue data structure, and associated functions, I used some code I wrote in the past, as a baseline for the queue implementation.

For the multithreaded implementation, I used the example pthreads code from the lectures, and the "helpful hint" in the Readme file
as a reference. 



