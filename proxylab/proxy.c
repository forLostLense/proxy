/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Thitaree Tanprasert, ttanprasert@g.hmc.edu 
 *     Jasmine Zhu, jizhu@g.hmc.edu
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"
#include "strmanip.h"

#define LISTEN_MAX 10		/* Maximum clients that can queue up */
/* The name of the proxy's log file */
#define PROXY_LOG "proxy.log"

/* Undefine this if you don't want debugging output */
#define DEBUG

/* 
 * This struct remembers some key attributes of an HTTP request and
 * the thread that is processing it.
 */
typedef struct {
    int myid;    /* Small integer used to identify threads in debug messages */
    int connfd;                    /* Connected file descriptor */ 
    struct sockaddr_in clientaddr; /* Client IP address */
} arglist_t;

/*
 * Place global declarations here.
 */ 
FILE * logFile;                 /* File descriptor of proxy log */
pthread_t thread_id;            
pthread_mutex_t lock;           /* mutex for writing to file */
pthread_mutex_t openLock;       /* mutex for open_clientd */

/*
 * Place forward function declarations here.
 */
void *process_request(void* vargp);
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen); 
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, int stringsize, struct sockaddr_in *sockaddr, char *uri, int size);

/*
 * Handy macro to compare something with a constant prefix.  For example,
 * prefixcmp(foo, "abc") returns 0 if the first three characters of foo
 * are "abc".
 */
#define prefixcmp(str, prefix) strncmp(str, prefix, sizeof(prefix) - 1)
#define postfixcmp(str, postfix) strcmp(str + sizeof(str) - sizeof(postfix), postfix)

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
    // Initialization for log file
    logFile = fopen ("proxy.log","w");
    setlinebuf(logFile);

    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

    int listenfd, connfd;
    socklen_t clientlen;
    int port;
    union {
	struct sockaddr_in client4;
	struct sockaddr_in6 client6;
    } clientaddr;
    char hostname[NI_MAXHOST];
    char hostaddr[NI_MAXHOST];
    int error;

    arglist_t* list1;

    port = atoi(argv[1]);	  /* the server listens on a port passed
                             on the command line */
    listenfd = open_listenfd(port);
    if (listenfd < 0) {
	    if (listenfd == -1)
	        (void) fprintf (stderr, "port %d: %s\n", port, strerror(errno));
	    exit(1);
    }

    while (1) {
        clientlen = sizeof clientaddr;
        connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        if (connfd == -1) {
	        fprintf (stderr, "accept: %s\n", strerror(errno));
	        continue;
        }

	    error = getnameinfo((struct sockaddr*)&clientaddr, clientlen,
	        hostname, sizeof hostname, NULL, 0, 0);
	    if (error != 0) {
	        fprintf (stderr, "Couldn't get name info for client: %s\n",
	        gai_strerror(error));
	        close(connfd);
	        continue;
	    }

	    error = getnameinfo((struct sockaddr*)&clientaddr, clientlen,
	        hostaddr, sizeof hostaddr, NULL, 0, NI_NUMERICHOST);

        if (error != 0) {
            fprintf (stderr, "Couldn't get numeric info for client %s: %s\n",
            hostname, gai_strerror(error));
            close(connfd);
            continue;
        }

        printf("server connected to %s (%s)\n", hostname, hostaddr);

        /* Initialize the variables to parse to process_request */
        list1 = (arglist_t *)malloc(sizeof(arglist_t));
	    list1->connfd = connfd;
        list1->myid = thread_id;
        list1->clientaddr = clientaddr.client4;
        pthread_create(&thread_id, NULL, process_request, list1);
    }

    /* Close the proxy log */
    fclose (logFile);

    /* Socket and thread initiation code goes here */
    exit(0);
    
}

/*
 * process_request - Thread routine.
 * 
 * Each thread reads an HTTP request from a client, forwards it to the
 * end server (always as a simple HTTP/1.0 request), waits for the
 * response, and then forwards it back to the client.
 */ 
void *process_request(void *vargp) 
{
    arglist_t arglist;              /* Arg list passed into thread */ 
    struct sockaddr_in clientaddr;  /* Client socket address */      
    int connfd;                     /* Socket descriptor for talking with client */
    char *request;                  /* HTTP request from client */
    int realloc_size;               /* Used to increase size of request buffer if necessary */  
    int request_len;                /* Total size of HTTP request */
    int n;                          /* General counting variable */
    rio_t rio;                      /* Rio buffer for calls to buffered rio_readlineb routine */
    char buf[MAXLINE];              /* General I/O buffer */


    /*
     * Do some initial setup
     *
     * NOTE TO STUDENTS: the structure of this code implies some things
     * about how main should invoke the thread.  It is up to you
     * whether you write a main to match, or discard the code and
     * design your own interface.
     */
    arglist = *((arglist_t *)vargp); /* Copy the arguments onto the stack */
    connfd = arglist.connfd;         /* Put connfd and clientaddr in scalars for convenience */  
    clientaddr = arglist.clientaddr;

    /* See the man page on pthread_detach for why the following line is handy */
    Pthread_detach(pthread_self());  /* Detach the thread */
    Free(vargp);                     /* Free up the arguments */ 

    /* 
     * Read the entire HTTP request into the request buffer, one line
     * at a time.
     */
    request = (char *)Malloc(MAXLINE);
    request[0] = '\0';
    realloc_size = MAXLINE;
    request_len = 0;
    Rio_readinitb(&rio, connfd);
    while (1) {
        if ((n = Rio_readlineb_w(&rio, buf, MAXLINE)) <= 0) {
            printf("Thread %d: process_request: client issued a bad request (1).\n",
              arglist.myid);
            printf("Thread %d: process_request: partial request was %s\n",
              arglist.myid, request);
            close(connfd);
            free(request);
            return NULL;
        }
        /* Don't pass "Connection:" lines; they cause long hangs */
        if (prefixcmp(buf, "Connection:") == 0)
            continue;

        /* If not enough room in request buffer, make more room */
        if (request_len + n + 1 > realloc_size) {
            /*
             * In this program the following loop always runs exactly once,
             * but if you were to use similar code elsewhere it might be
             * needed, so we'll keep it here for pedagogical purposes.
             */
            while (request_len + n + 1 > realloc_size)
                realloc_size *= 2;
            request = Realloc(request, realloc_size);
        }

        memcpy(request + request_len, buf, n);
        request_len += n;

        /* An HTTP request is always terminated by a blank line */
        if (strcmp(buf, "\r\n") == 0  ||  strcmp(buf, "\n") == 0)
            break;
    }
    /* 
     * Make sure that this is indeed a GET request
     */
    if (prefixcmp(request, "GET ") != 0) {
        printf("process_request: Received non-GET request\n");
        close(connfd);
        free(request);
        return NULL;
    }

    /*
     * Most of your "real" code will go here.  You should first
     * extract the URI from the request and parse it (using
     * parse_uri).  Note that the extracted URI must be a proper C
     * string (i.e., it has to end with a null byte).  You should also
     * perform basic validity checks (e.g., check for HTTP/1.0 or
     * HTTP/1.1), forward the request to the server, pass the response
     * back to the client, log things, and clean up.  Be careful to
     * close all appropriate fds and free appropriate memory,
     * especially on error paths!
     */
    int clientfd;
    char *uri;
    char hostname[MAXLINE];
    char pathname[MAXLINE];
    int port;
    char logstring[MAXLINE];

    /* 
     * Initialize uri variable for parse_uri function
     * Get rid of the commend GET and HTTP/1.0-1 in request.
     */
    uri = (char *)Malloc(MAXLINE);
    strncpy(uri, request+4, request_len-17);
    uri[request_len-17] = '\0';
    if (parse_uri(uri, hostname, pathname, &port) == -1) {
        printf("Should close connection.\n");    
    } 

    /*
     * Format request to be sent to server
     * Consist of 3 lines:
     * GET pathname HTTP/1.0
     * Host: hostname
     * (empty line)
     */
    char newrequest[MAXLINE];
    newrequest[0] = '\0';
    strcat(newrequest, "GET /");
    strcat(newrequest, pathname);
    strcat(newrequest, " HTTP/1.0\r\nHost: ");
    strcat(newrequest, hostname);
    strcat(newrequest,"\r\n\r\n");

    /*
     * Make a threadsafe version of open_clientfd
     */
    pthread_mutex_lock(&openLock);
    clientfd = open_clientfd(hostname, port);
    pthread_mutex_unlock(&openLock);
    if (clientfd < 0) {
        if (clientfd == -1)
            fprintf (stderr, "%s:%d: %s\n", hostname, port, strerror(errno));
        exit(1);
    }

    // Proxy acts as a client, sending request to server    
    write(clientfd, newrequest, strlen(newrequest));

	int sum = 0; // Initialize variable for storing content length.

    // Loop for reading content from the server
	while(1) {

        n = rio_readn(clientfd, buf, sizeof buf - 1);

        // Check if there is error in reading from server
        if (n == -1) {
            fprintf (stderr, "%s:%d: %s\n", hostname, port, strerror(errno));
        }
	    else {
            buf[n] = '\0';
	        sum += n;
            write(connfd, buf, n); // Write content in buf back to client.
        }

        // Check if we have reached the end of the content
        if (n == 0) {
            format_log_entry(logstring, MAXLINE-1, &clientaddr, uri, sum);

            // Use mutex for writing to log file
            pthread_mutex_lock(&lock);
            fprintf(logFile, "%s", logstring);
            pthread_mutex_unlock(&lock);

            break;
        }
    }

    /*
     * Close both the proxy-server and proxy-client connections
     */
    close(clientfd);
    close(connfd);
    return NULL;
}


/*
 * Rio_readlineb_w - A wrapper for rio_readlineb (csapp.c) that
 * prints a warning when a read fails instead of terminating 
 * the process.
 */
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
        printf("Warning: rio_readlineb failed; error = %s\n", strerror(errno));
        return 0;
    }
    return rc;
} 

/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{ 
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;
    if (strncasecmp(uri, "http://", 7) != 0) {
        hostname[0] = '\0';
        return -1;
    }
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n");
    if (hostend != 0)
        len = hostend - hostbegin;
    else
        len = strlen(hostbegin);
    hostend = hostbegin + len;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
        *port = atoi(hostend + 1);
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
        pathname[0] = '\0';
    }
    else {
        pathbegin++;    
        strcpy(pathname, pathbegin);
    }
    printf("%c\n",*hostbegin);
    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The arguments are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, int stringsize,
                      struct sockaddr_in *sockaddr, char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    snprintf(logstring, stringsize, "%s: %d.%d.%d.%d %s %d\n", time_str,
                                    a, b, c, d, uri, size);
}