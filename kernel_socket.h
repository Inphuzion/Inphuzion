#include "tinyos.h"
#include "util.h"
#include "kernel_dev.h"


/* Socket-Specific Operations */
typedef enum socket_type {
    SOCKET_LISTENER,  /**< Listener socket for incoming connections. */
    SOCKET_UNBOUND,   /**< Unbound socket, not tied to a specific port. */
    SOCKET_PEER       /**< Peer socket, used in active communication. */
} socket_type;


typedef struct socket_listener {
    rlnode queue;          /**< Queue to store incoming connection requests. */
    CondVar req_available; /**< Condition variable to signal when a request is available. */
} socket_listener;

typedef struct socket_peer {
    SCB* peer;             /**< Pointer to the connected peer socket control block (SCB). */
    pipe_cb* read_pipe;    /**< Pipe used for reading data from the connected peer. */
    pipe_cb* write_pipe;   /**< Pipe used for writing data to the connected peer. */
} socket_peer;

typedef struct socket_control_block {
    unsigned int refcount; /**< Reference count to manage socket usage. */
    FCB* fcb;              /**< File control block associated with the socket. */
    socket_type type;      /**< Type of the socket (listener, unbound, or peer). */
    port_t port;           /**< Port number the socket is bound to. */

    union {
        socket_listener listener; /**< Data specific to listener sockets. */
        socket_peer peer;         /**< Data specific to peer sockets. */
    };
} SCB;

typedef struct request_connection {
    int admitted;          /**< Flag indicating whether the request has been admitted. */
    SCB* peer;             /**< Pointer to the peer socket making the request. */
    CondVar connected_cv;  /**< Condition variable to signal when the connection is established. */
    rlnode queue_node;     /**< Node for adding the request to the listener's queue. */
} request_connection;


SCB* PORT_MAP[MAX_PORT + 1];

Fid_t sys_Socket(port_t port);
int sys_Listen(Fid_t sock);
Fid_t sys_Accept(Fid_t lsock);
int sys_Connect(Fid_t sock, port_t port, timeout_t timeout);
int sys_ShutDown(Fid_t sock, shutdown_mode how);
int socket_read(void* socketcb_t, char* buf, unsigned int len); 
int socket_write(void* socketcb_t, const char* buf, unsigned int len); 
int socket_close(void* socketcb_t); 
