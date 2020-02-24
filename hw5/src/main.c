#include <stdlib.h>

#include "client_registry.h"
#include "exchange.h"
#include "trader.h"
#include "server.h"
#include "debug.h"
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include "csapp.h"

extern EXCHANGE *exchange;
extern CLIENT_REGISTRY *client_registry;

static void terminate(int status);
int get_port(int argc, char* argv[]);
void shut_down_server(int sig);

/*
 * "Bourse" exchange server.
 *
 * Usage: bourse <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    // Perform required initializations of the client_registry,
    // maze, and player modules.
    int port = get_port(argc, argv);

    client_registry = creg_init();
    exchange = exchange_init();
    trader_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function brs_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.
    Signal(SIGHUP, shut_down_server);   // sigaction wrapped in csapp.h

    int listenfd, *connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    listenfd = Open_listenfd(port); 
    debug("open_listenfd success");       
    
    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Malloc(sizeof(int));   // TODO: need to be freed and detached in brs_client_service
        *connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Pthread_create(&tid, NULL, brs_client_service, connfd);
        debug("thread create success");
        // creg_register(client_registry, *connfd); /* Insert connfd in buffer */
    }

    fprintf(stderr, "You have to finish implementing main() "
	    "before the Bourse server will function.\n");

    terminate(EXIT_FAILURE);
}

void shut_down_server(int sig) {
    terminate(sig);
}

/* $begin get_port */
void server_input_invalid() {
    printf("%s\n", "Usage: util/demo_server -p <port>");
    exit(EXIT_FAILURE);
}

int get_port(int argc, char* argv[]) {
    // debug("server argc: %d", argc);
    // for (int i = 0; i < argc; i++) debug("server argv[%d]: %s", i, argv[i]);
    if (argc < 3 || strcmp(argv[1], "-p") != 0) server_input_invalid();
    
    int port = atoi(argv[2]);
    if (port <= 0) {
        // debug("invalid server port");
        server_input_invalid();
    }
    // debug("server port is %d", port);
    return port;
}
/* $end get_port */

/*
 * Function called to cleanly shut down the server.
 */
static void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);
    
    debug("Waiting for service threads to terminate...");
    creg_wait_for_empty(client_registry);
    debug("All service threads terminated.");

    // Finalize modules.
    creg_fini(client_registry);
    exchange_fini(exchange);
    trader_fini();

    debug("Bourse server terminating");
    exit(status);
}
