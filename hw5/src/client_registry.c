#include "debug.h"
#include "client_registry.h"
#include "csapp.h"
#include "errno.h"

// TODO: test your client registry separately from the server(refer to doc)

int BUF_SIZE = 1024;    /* 1024 for FD_SETSIZE(see man select) */

typedef struct client_registry {
    int *buf;           /* Buffer array, initialize to all -1 */         
    sem_t mutex;        /* Protects accesses to buf */
    int cli_cnt;        /* Client count */
} CLIENT_REGISTRY;

CLIENT_REGISTRY *creg_init() {
    CLIENT_REGISTRY* cr = Malloc(sizeof(CLIENT_REGISTRY));
    cr->buf = Malloc(BUF_SIZE * sizeof(int));
    cr->cli_cnt = 0;
    for (int i = 0; i < BUF_SIZE; i++) cr->buf[i] = -1; /* initialize to all -1 */
    Sem_init(&cr->mutex, 0, 1);      /* Binary semaphore for locking */
    return cr;
}

void creg_fini(CLIENT_REGISTRY *cr) {
    Free(cr->buf);
    Free(cr);
}

int creg_register(CLIENT_REGISTRY *cr, int fd) {
    int res = -1;
    
    P(&cr->mutex);
    for(int i = 0; i < BUF_SIZE; i++) {     /* Find an idle slot and insert an fd */
        if (cr->buf[i] == -1) {
            cr->buf[i] = fd;

            ++cr->cli_cnt;
            
            res = 0;
            break;
        }
    }
    if (res != 0) debug("Registed threads over limit");
    else debug("creg_register for fd=%d", fd);

    V(&cr->mutex);
    return res;
}

int creg_unregister(CLIENT_REGISTRY *cr, int fd) {
    int res = -1;

    P(&cr->mutex);
    for(int i = 0; i < BUF_SIZE; i++) {     /* Find and reset fd to -1 */
        if (cr->buf[i] == fd) {
            debug("Related fd of %d found on index %d of buf", fd, i);
            cr->buf[i] = -1;

            --cr->cli_cnt;

            res = 0;
            break;
        }
    }
    V(&cr->mutex);
    if (res == -1) debug("Can't remove: fd of %d not found", fd);
    return res;
}

void creg_wait_for_empty(CLIENT_REGISTRY *cr) {
    P(&cr->mutex);
    if (cr->cli_cnt == 0)
        V(&cr->mutex);
}

void creg_shutdown_all(CLIENT_REGISTRY *cr) {
    P(&cr->mutex);
    for (int i = 0; i < BUF_SIZE; i++) {
        if (cr->buf[i] == -1) continue;
        
        debug("Shutting down fd %d", cr->buf[i]);
        shutdown(cr->buf[i], SHUT_RD);

        cr->buf[i] = -1;
        --cr->cli_cnt;
    }

    debug("Finish shutting down, current cli_cnt is %d", cr->cli_cnt);
    V(&cr->mutex);
}