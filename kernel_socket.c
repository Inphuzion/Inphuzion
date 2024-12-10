
// #include "tinyos.h"
// #include "kernel_streams.h"
// #include "kernel_streams.h"
// #include "kernel_sched.h"
// #include "kernel_proc.h"
// #include "kernel_dev.h"
// #include "kernel_cc.h"
// #include "kernel_pipe.c"




// file_ops socket_file_ops = {
//     .Open = socket_open,        // Function to handle opening the socket
//     .Read = socket_read,        // Function to handle reading from the socket
//     .Write = socket_write,      // Function to handle writing to the socket
//     .Close = socket_close       // Function to handle closing the socket
// };


// int socket_open(FCB* fcb) {
//     // Typically, socket is already initialized when created
//     // So this function can remain empty or do any setup required
//     return 0;  // Success
// }

// int socket_read(FCB* fcb, char* buf, unsigned int size) {
//     socket_cb* socket = (socket_cb*)fcb->streamobj;
    
//     if (socket->type != SOCKET_PEER) {
//         return -1;  // Can't read from an unbound or listener socket
//     }

//     // Read from the peer socket's read pipe
//     return pipe_read(socket->peer_socket.read_pipe, buf, size);
// }

// int socket_write(FCB* fcb, const char* buf, unsigned int size) {
//     socket_cb* socket = (socket_cb*)fcb->streamobj;
    
//     if (socket->type != SOCKET_PEER) {
//         return -1;  // Can't write to an unbound or listener socket
//     }

//     // Write to the peer socket's write pipe
//     return pipe_write(socket->peer_socket.write_pipe, buf, size);
// }

// int socket_close(FCB* fcb) {
//     socket_cb* socket = (socket_cb*)fcb->streamobj;
    
//     if (socket->type == SOCKET_PEER) {
//         // Close the pipes for peer sockets
//         pipe_writer_close(socket->peer_socket.write_pipe);
//         pipe_reader_close(socket->peer_socket.read_pipe);
//     }
    
//     // Clean up the socket control block
//     free(socket);
//     fcb->streamobj = NULL;
    
//     return 0;  // Success
// }



// Fid_t sys_Socket(port_t port) {
//     FCB *fcb;
//     Fid_t fid;
//     socket_cb *socket;

//     // Step 1: Check if the port is valid
//     if (port < 1 || port > MAX_PORT) {
//         return NOFILE;  // Invalid port number
//     }

//     // Step 2: Reserve a file descriptor and get the associated FCB
//     if (!FCB_reserve(1, &fid, &fcb)) {
//         return NOFILE;  // No file descriptors available
//     }

//     // Step 3: Allocate memory for the socket control block (SCB)
//     socket = (socket_cb*)xmalloc(sizeof(socket_cb));
//     if (!socket) {
//         FCB_unreserve(1, &fid, &fcb);  // Rollback if allocation fails
//         return NOFILE;  // Memory allocation failure
//     }

//     // Step 4: Initialize the socket control block (SCB)
//     socket->refcount = 0;  // Initial reference count for the socket
//     socket->port = port;
//     socket->type = (port == NOPORT) ? SOCKET_UNBOUND : SOCKET_PEER;  // Mark the socket as unbound or peer
//     memset(&socket->peer_s, 0, sizeof(socket->peer_s));  // Initialize peer socket (if needed)

//     // Step 5: Assign the socket control block to the file descriptor's stream object
//     fcb->streamobj = socket;
//     fcb->streamfunc = &socket_file_ops;  // Set file operations (to be defined elsewhere)

//     return fid;  // Return the file descriptor for the created socket


// int sys_Listen(Fid_t sock) {
//     FCB *fcb;
//     socket_cb *socket;

//     // Step 1: Get the FCB for the socket
//     if (!FCB_get(sock, &fcb)) {
//         return -1;  // Invalid file descriptor
//     }

//     // Step 2: Retrieve the socket control block
//     socket = (socket_cb*)fcb->streamobj;

//     // Step 3: Ensure the socket is bound to a valid port
//     if (socket->port == NOPORT) {
//         return -1;  // Socket is not bound to a valid port
//     }

//     // Step 4: Ensure the socket is not already a listener
//     if (socket->type == SOCKET_LISTENER) {
//         return -1;  // Socket is already a listener
//     }

//     // Step 5: Mark the socket as a listener
//     socket->type = SOCKET_LISTENER;

//     // Step 6: Initialize the listener socket's request queue
//     rlnode_new(&socket->listener_socket.queue);  // Initialize the request queue
//     socket->listener_socket.req_available = COND_INIT;  // Initialize the condition variable

//     // Step 7: Add the socket to the listener map (PORT_MAP)
//     PORT_MAP[socket->port] = socket;

//     return 0;  // Successfully initialized as a listener
// }


// Fid_t sys_Accept(Fid_t lsock) {
//     FCB *fcb;
//     socket_cb *listener_socket;
//     rlnode *req_node;
//     connection_request *req;

//     // Step 1: Check if the file descriptor is valid
//     if (!FCB_get(lsock, &fcb)) {
//         return NOFILE;  // Invalid file descriptor
//     }

//     // Step 2: Retrieve the listener socket control block
//     listener_socket = (socket_cb*)fcb->streamobj;

//     // Step 3: Ensure the socket is a listener
//     if (listener_socket->type != SOCKET_LISTENER) {
//         return NOFILE;  // This socket is not a listener
//     }

//     // Step 4: Increase refcount to prevent socket from being closed while we are waiting
//     listener_socket->refcount++;

//     // Step 5: Wait for a connection request in the listener's queue
//     while (is_rlist_empty(&listener_socket->listener_socket.queue)) {
//         kernel_timedwait(&listener_socket->listener_socket.req_available, NULL);  // Wait for a connection request
//     }

//     // Step 6: Extract the connection request from the queue
//     req_node = rlist_pop_front(&listener_socket->listener_socket.queue);
//     req = (connection_request*)req_node->obj;

//     // Step 7: Check if the port is still valid (listener socket may have been closed)
//     if (listener_socket->port == NOPORT) {
//         return NOFILE;  // Socket is no longer valid
//     }

//     // Step 8: Mark the connection request as admitted (i.e., accepted)
//     req->admitted = 1;

//     // Step 9: Create the peer socket for communication
//     Fid_t peer_fid = sys_Socket(listener_socket->port);  // Assuming sys_Socket handles binding

//     // Step 10: Initialize the peer socket (set up its connection)
//     FCB *peer_fcb;
//     socket_cb *peer_socket;
//     FCB_get(peer_fid, &peer_fcb);
//     peer_socket = (socket_cb*)peer_fcb->streamobj;

//     // Set the peer socket type and initialize its peer connection
//     peer_socket->type = SOCKET_PEER;
//     peer_socket->peer_socket = listener_socket->peer_socket;

//     // Step 11: Signal the Connect side (client) that the connection is established
//     kernel_broadcast(&peer_socket->peer_socket.req_available);  // Notify the client side

//     // Step 12: Release the connection request (freeing the request node)
//     free(req_node);

//     // Step 13: Decrease the refcount for the listener socket after processing the request
//     listener_socket->refcount--;

//     // Step 14: Return the peer file descriptor for the established connection
//     return peer_fid;
// }


// int sys_Connect(Fid_t sock, port_t port, timeout_t timeout) {
//     FCB *fcb;
//     socket_cb *socket;
//     connection_request *req;
//     socket_cb *listener_socket;
//     Fid_t peer_fid;
//     rlnode *req_node;

//     // Step 1: Check if the socket is valid
//     if (!FCB_get(sock, &fcb)) {
//         return -1;  // Invalid socket file descriptor
//     }

//     socket = (socket_cb*)fcb->streamobj;

//     // Step 2: Ensure the socket is unbound (i.e., it’s not yet connected)
//     if (socket->type != SOCKET_UNBOUND) {
//         return -1;  // Socket is already connected or in the wrong state
//     }

//     // Step 3: Check if the port is valid and there is a listener on the port
//     if (port < 1 || port > MAX_PORT || PORT_MAP[port] == NULL) {
//         return -1;  // Invalid port or no listener on the port
//     }

//     // Step 4: Increase refcount to prevent the socket from being closed while we wait
//     socket->refcount++;

//     // Step 5: Create a connection request and fill in the details
//     req = (connection_request*)xmalloc(sizeof(connection_request));
//     if (!req) {
//         socket->refcount--;  // Decrease refcount if allocation fails
//         return -1;  // Memory allocation failure
//     }

//     req->sock = sock;
//     req->port = port;
//     req->admitted = 0;  // Initially not admitted

//     // Step 6: Add the connection request to the listener's request queue
//     listener_socket = (socket_cb*)PORT_MAP[port];
//     rlnode_init(&req_node, req);
//     rlist_push_back(&listener_socket->listener_socket.queue, req_node);
//     kernel_signal(&listener_socket->listener_socket.req_available);  // Notify listener

//     // Step 7: Wait for the request to be admitted (i.e., connection accepted)
//     while (!req->admitted) {
//         int ret = kernel_timedwait(&req->req_available, timeout);
//         if (ret == -1) {  // Timeout occurred
//             socket->refcount--;  // Decrease refcount if timed out
//             free(req);  // Free connection request
//             return -1;
//         }
//     }

//     // Step 8: Once admitted, create the peer socket for communication
//     peer_fid = sys_Socket(port);
//     if (peer_fid == NOFILE) {
//         socket->refcount--;  // Decrease refcount if socket creation fails
//         free(req);  // Free connection request
//         return -1;
//     }

//     FCB *peer_fcb;
//     socket_cb *peer_socket;
//     FCB_get(peer_fid, &peer_fcb);
//     peer_socket = (socket_cb*)peer_fcb->streamobj;

//     // Step 9: Set up the peer socket and link to the listener socket
//     peer_socket->type = SOCKET_PEER;
//     peer_socket->peer_socket = listener_socket->peer_socket;

//     // Step 10: Signal the peer (client side) that the connection is established
//     kernel_signal(&peer_socket->peer_socket.req_available);

//     // Step 11: Clean up - decrease the refcount for the unbound socket and free the request
//     socket->refcount--;
//     free(req);

//     // Step 12: Return the peer file descriptor for the established connection
//     return peer_fid;


// int sys_ShutDown(Fid_t sock, shutdown_mode how)
// {
// 	return -1;
// }



#include "tinyos.h"


Fid_t sys_Socket(port_t port)
{
    return NOFILE;
}

int sys_Listen(Fid_t sock)
{
    return -1;
}


Fid_t sys_Accept(Fid_t lsock)
{
    return NOFILE;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
    return -1;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
    return -1;
}