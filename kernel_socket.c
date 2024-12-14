
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_dev.h"
#include "kernel_cc.h"
#include "kernel_socket.h"



static file_ops socket_file_ops = {
    .Open = NULL,        // Function to handle opening the socket
    .Read = socket_read,        // Function to handle reading from the socket
    .Write = socket_write,      // Function to handle writing to the socket
    .Close = socket_close       // Function to handle closing the socket
};



int socket_read(void* socket, char* buf, unsigned int len) {
    if (socket == NULL || buf == NULL || len == 0) {
        return -1; // Invalid parameters
    }

    SCB* scb = (SCB*)socket;
    if (scb->type != SOCKET_PEER || scb->peer.read_pipe == NULL) {
        return -1; // Can only read from a peer socket with a valid read pipe
    }

    // Perform the read operation from the pipe and return the number of bytes read
    int bytes_read = pipe_read(scb->peer.read_pipe, buf, len);
    return (bytes_read >= 0) ? bytes_read : -1; // Ensure return value is valid
}

int socket_write(void* socket, const char* buf, unsigned int len) {
    if (socket == NULL || buf == NULL || len == 0) {
        return -1; // Invalid parameters
    }

    SCB* scb = (SCB*)socket;
    if (scb->type != SOCKET_PEER || scb->peer.write_pipe == NULL) {
        return -1; // Can only write to a peer socket with a valid write pipe
    }

    // Perform the write operation to the pipe and return the number of bytes written
    int bytes_written = pipe_write(scb->peer.write_pipe, buf, len);
    return (bytes_written >= 0) ? bytes_written : -1; // Ensure return value is valid
}

int socket_close(void* socket) {
    if (socket == NULL) {
        return -1; // Invalid socket control block
    }

    SCB* scb = (SCB*)socket;
    int result = 0;

    switch (scb->type) {
        case SOCKET_LISTENER:
            // Remove listener's port mapping
            if (scb->port <= MAX_PORT) {
                PORT_MAP[scb->port] = NULL;
            }

            // Notify that the listener's requests are available
            kernel_signal(&scb->listener.req_available);
            break;

        case SOCKET_PEER:
            // Close the write pipe for peer sockets
            if (scb->peer.write_pipe != NULL) {
                if (pipe_writer_close(scb->peer.write_pipe) != 0) {
                    result = -1; // Error occurred during write pipe closure
                }
            }

            // Close the read pipe for peer sockets
            if (scb->peer.read_pipe != NULL) {
                if (pipe_reader_close(scb->peer.read_pipe) != 0) {
                    result = -1; // Error occurred during read pipe closure
                }
            }
            break;

        default:
            return -1; // Unsupported socket type
    }

    return result; // Return success or failure
}



pipe_cb* init_pipe(FCB* fcb[2]) {
    pipe_cb* pipe = (pipe_cb*)xmalloc(sizeof(pipe_cb));

    pipe->reader = fcb[0];
    pipe->writer = fcb[1];
    pipe->has_space = COND_INIT;
    pipe->has_data = COND_INIT;
    pipe->w_position = 0;
    pipe->r_position = 0;

    return pipe;
}

request_connection* init_request_connection(SCB* client_scb) {
    // Allocate memory 
    request_connection* rc = (request_connection*)xmalloc(sizeof(request_connection));

    // Initialize 
    rc->admitted = 0;                  // Connection is not yet admitted
    rc->peer = client_scb;             // Associate the request with the client's socket control block
    rc->connected_cv = COND_INIT;      // Initialize the condition variable for signaling connection status

    // Initialize the queue node for adding the request to a listener's queue
    rlnode_init(&rc->queue_node, rc);

    return rc;  // Return the pointer to the initialized connection request
}


Fid_t sys_Socket(port_t port) {
    if (port < NOPORT || port > MAX_PORT) return -1;

    Fid_t fid[1];
    FCB* fcb[1];

    if (FCB_reserve(1, fid, fcb) != 1) return -1;

    // Inline logic for initializing an SCB (Socket Control Block)
    SCB* scb = (SCB*)xmalloc(sizeof(SCB));
    scb->refcount = 0;
    scb->port = port;
    scb->fcb = fcb[0];
    scb->type = SOCKET_UNBOUND;

    fcb[0]->streamobj = scb;
    fcb[0]->streamfunc = &socket_file_ops;

    if (PORT_MAP[port] == NULL) PORT_MAP[port] = scb;

    return fid[0];
}


int sys_Listen(Fid_t sock) {
    // Retrieve the file control block (FCB) for the given file descriptor
    FCB* fcb = get_fcb(sock);
    if (fcb == NULL || fcb->streamfunc != &socket_file_ops) {
        return -1;  // Invalid file descriptor or unsupported file operations
    }

    // Get the socket control block (SCB) associated with the FCB
    SCB* scb = (SCB*)fcb->streamobj;
    if (scb == NULL) {
        return -1;  // Invalid socket control block
    }

    // Validate the port number
    if (scb->port <= NOPORT || scb->port > MAX_PORT) {
        return -1;  // Invalid port
    }

    // Ensure no other listener is already bound to this port
    if (PORT_MAP[scb->port] != NULL && PORT_MAP[scb->port]->type == SOCKET_LISTENER) {
        return -1;  // Port already has a listener
    }

    // Ensure the socket is currently unbound
    if (scb->type != SOCKET_UNBOUND) {
        return -1;  // Socket is not unbound
    }

    // Transition the socket into a listener
    scb->type = SOCKET_LISTENER;

    // Initialize the listener's queue and condition variable
    rlnode_init(&scb->listener.queue, NULL);  // Initialize the queue as empty
    scb->listener.req_available = COND_INIT; // Initialize the condition variable

    return 0;  // Successfully set up as a listener
}

Fid_t sys_Accept(Fid_t lsock) {    
    // Retrieve the file control block (FCB) for the listening socket
    FCB* fcb = get_fcb(lsock);
    if (!fcb || fcb->streamfunc != &socket_file_ops) {
        return NOFILE;  // Invalid file descriptor or unsupported operations
    }

    // Get the socket control block (SCB) associated with the FCB
    SCB* listener_scb = (SCB*)fcb->streamobj;
    if (!listener_scb) {
        return -1;  // Invalid listener SCB
    }

    // Validate the listener socket
    if (listener_scb->port <= NOPORT || listener_scb->port > MAX_PORT) {
        return -1;  // Invalid port
    }
    if (listener_scb->type == SOCKET_PEER) {
        return -1;  // Cannot accept on a peer socket
    }
    if (PORT_MAP[listener_scb->port]->type != SOCKET_LISTENER) {
        return -1;  // Port is not associated with a listener
    }

    // Prevent socket closure during wait
    listener_scb->refcount++;

    // Wait for a connection request
    while (is_rlist_empty(&listener_scb->listener.queue)) {
        kernel_wait(&listener_scb->listener.req_available, SCHED_IO);

        // Check if the listener socket is still valid
        if (!PORT_MAP[listener_scb->port]) {
            listener_scb->refcount--;
            return -1;  // Listener closed while waiting
        }
    }

    // Decrease reference count after exiting the wait
    if (listener_scb->refcount > 0) {
        listener_scb->refcount--;
    }

    // Create a server socket for the accepted connection
    Fid_t serverFid = sys_Socket(listener_scb->port);
    if (serverFid == NOFILE) {
        return -1;  // Failed to create a new server socket
    }

    FCB* serverFcb = get_fcb(serverFid);
    if (!serverFcb) {
        return -1;  // Failed to retrieve the server's FCB
    }

    SCB* server_scb = (SCB*)serverFcb->streamobj;
    if (!server_scb) {
        return -1;  // Failed to retrieve the server's SCB
    }

    // Retrieve and process the connection request
    rlnode* requestNode = rlist_pop_front(&listener_scb->listener.queue);
    request_connection* rc = (request_connection*)requestNode->obj;

    SCB* client_scb = rc->peer;
    if (!client_scb) {
        return -1;  // Invalid client SCB
    }

    // Establish bidirectional pipes between client and server
    FCB* pipe_fcb_1[2] = {client_scb->fcb, serverFcb};
    pipe_cb* pipe_1 = init_pipe(pipe_fcb_1);

    FCB* pipe_fcb_2[2] = {serverFcb, client_scb->fcb};
    pipe_cb* pipe_2 = init_pipe(pipe_fcb_2);

    if (pipe_1 && pipe_2) {
        // Configure the server socket
        server_scb->type = SOCKET_PEER;
        server_scb->peer.read_pipe = pipe_2;
        server_scb->peer.write_pipe = pipe_1;

        // Configure the client socket
        client_scb->type = SOCKET_PEER;
        client_scb->peer.read_pipe = pipe_1;
        client_scb->peer.write_pipe = pipe_2;
    }

    // Mark the request as admitted and signal the client
    rc->admitted = 1;
    kernel_signal(&rc->connected_cv);

    return serverFid;  // Return the server socket file descriptor
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout) {
    // Validate input file descriptor and retrieve the associated FCB
    FCB* fcb = get_fcb(sock);
    if (fcb == NULL || fcb->streamfunc != &socket_file_ops) {
        return NOFILE; // Invalid socket
    }

    // Validate the port number
    if (port <= NOPORT || port > MAX_PORT) {
        return -1; // Invalid port
    }

    // Retrieve the socket control block (SCB) for the client
    SCB* client_scb = (SCB*)fcb->streamobj;
    if (client_scb == NULL || client_scb->type != SOCKET_UNBOUND) {
        return -1; // Invalid client socket
    }

    // Retrieve the listener SCB for the target port
    SCB* listener_scb = PORT_MAP[port];
    if (listener_scb == NULL || listener_scb->type != SOCKET_LISTENER) {
        return -1; // No listener available on the specified port
    }

    // Initialize a connection request for the client
    request_connection* conn_req = init_request_connection(client_scb);

    // Add the connection request to the listener's queue
    rlist_push_back(&listener_scb->listener.queue, &conn_req->queue_node);

    // Notify the listener that a new request is available
    kernel_signal(&listener_scb->listener.req_available);

    // Increment reference count for the client SCB to prevent closure during wait
    client_scb->refcount++;

    // Wait for the connection to be admitted or timeout
    while (conn_req->admitted == 0) {
        if (kernel_timedwait(&conn_req->connected_cv, SCHED_IO, timeout * 1000) == 0) {
            client_scb->refcount--;
            free(conn_req);
            return -1; // Timeout
        }
    }

    // Decrement the reference count after the connection is established
    if (client_scb->refcount > 0) {
        client_scb->refcount--;
    }

    // Free the connection request object
    free(conn_req);

    return 0; // Success
}



int sys_ShutDown(Fid_t sock, shutdown_mode how) {
    // Validate the shutdown mode
    if (how < SHUTDOWN_READ || how > SHUTDOWN_BOTH) {
        return -1; // Invalid mode
    }

    // Retrieve the FCB for the socket
    FCB* fcb = get_fcb(sock);
    if (fcb == NULL) {
        return -1; // Invalid file descriptor
    }

    // Retrieve the socket control block (SCB)
    SCB* scb = (SCB*)fcb->streamobj;
    if (scb == NULL || scb->type != SOCKET_PEER) {
        return -1; // Shutdown only valid for peer sockets
    }

    // Perform the appropriate shutdown operation
    int result = 0;
    switch (how) {
        case SHUTDOWN_READ:
            result = pipe_reader_close(scb->peer.read_pipe);
            break;

        case SHUTDOWN_WRITE:
            result = pipe_writer_close(scb->peer.write_pipe);
            break;

        case SHUTDOWN_BOTH:
            if (pipe_writer_close(scb->peer.write_pipe) != 0 ||
                pipe_reader_close(scb->peer.read_pipe) != 0) {
                result = -1; // Failure in closing one or both directions
            }
            break;

        default:
            result = -1; // Should never reach here
            break;
    }

    return result; // Return the result of the shutdown operation
}







