#ifndef __KERNEL_STREAMS_H
#define __KERNEL_STREAMS_H

#include "tinyos.h"
#include "kernel_dev.h"

/**
	@file kernel_streams.h
	@brief Support for I/O streams.


	@defgroup streams Streams.
	@ingroup kernel
	@brief Support for I/O streams.

	The stream model of tinyos3 is similar to the Unix model.
	Streams are objects that are shared between processes.
	Streams are accessed by file IDs (similar to file descriptors
	in Unix).

	The streams of each process are held in the file table of the
	PCB of the process. The system calls generally use the API
	of this file to access FCBs: @ref get_fcb, @ref FCB_reserve
	and @ref FCB_unreserve.

	Streams are connected to devices by virtue of a @c file_operations
	object, which provides pointers to device-specific implementations
	for read, write and close.

	@{
*/



/** @brief The file control block.

	A file control block provides a uniform object to the
	system calls, and contains pointers to device-specific
	functions.
 */
typedef struct file_control_block
{
  uint refcount;  			/**< @brief Reference counter. */
  void* streamobj;			/**< @brief The stream object (e.g., a device) */
  file_ops* streamfunc;		/**< @brief The stream implementation methods */
  rlnode freelist_node;		/**< @brief Intrusive list node */
} FCB;


#define PIPE_BUFFER_SIZE 1024  // Define the buffer size for the pipe

typedef struct pipe_control_block {
    FCB *reader, *writer;            // File control blocks for pipe ends
    CondVar has_space;               // Synchronization for space
    CondVar has_data;                // Synchronization for data
    int w_position, r_position;      // Write and read positions in buffer
    char BUFFER[PIPE_BUFFER_SIZE];   // Circular buffer for data
} pipe_cb;



int pipe_write(void* pipecb_t, const char *buf, unsigned int n);
int pipe_read(void* pipecb_t, char *buf, unsigned int n);
int pipe_writer_close(void* _pipecb);
int pipe_reader_close(void* _pipecb);
int not_allowed_w(void* pipecb_t, const char* buf, unsigned int n);
int not_allowed_r(void* pipecb_t, char* buf, unsigned int n);



/** 
  @brief Initialization for files and streams.

  This function is called at kernel startup.
 */
void initialize_files();


// /* Socket-Specific Operations */
// typedef enum socket_type {
//     SOCKET_LISTENER,  /**< Listener socket for incoming connections. */
//     SOCKET_UNBOUND,   /**< Unbound socket, not tied to a specific port. */
//     SOCKET_PEER       /**< Peer socket, used in active communication. */
// } socket_type;


// typedef struct socket_listener {
//     rlnode queue;          /**< Queue to store incoming connection requests. */
//     CondVar req_available; /**< Condition variable to signal when a request is available. */
// } socket_listener;

// typedef struct socket_peer {
//     SCB* peer;             /**< Pointer to the connected peer socket control block (SCB). */
//     pipe_cb* read_pipe;    /**< Pipe used for reading data from the connected peer. */
//     pipe_cb* write_pipe;   /**< Pipe used for writing data to the connected peer. */
// } socket_peer;

// typedef struct socket_control_block {
//     unsigned int refcount; /**< Reference count to manage socket usage. */
//     FCB* fcb;              /**< File control block associated with the socket. */
//     socket_type type;      /**< Type of the socket (listener, unbound, or peer). */
//     port_t port;           /**< Port number the socket is bound to. */

//     union {
//         socket_listener listener; /**< Data specific to listener sockets. */
//         socket_peer peer;         /**< Data specific to peer sockets. */
//     };
// } SCB;

// typedef struct request_connection {
//     int admitted;          /**< Flag indicating whether the request has been admitted. */
//     SCB* peer;             /**< Pointer to the peer socket making the request. */
//     CondVar connected_cv;  /**< Condition variable to signal when the connection is established. */
//     rlnode queue_node;     /**< Node for adding the request to the listener's queue. */
// } request_connection;


// extern SCB* PORT_MAP[MAX_PORT + 1];

// Fid_t sys_Socket(port_t port);
// int sys_Listen(Fid_t sock);
// Fid_t sys_Accept(Fid_t lsock);
// int sys_Connect(Fid_t sock, port_t port, timeout_t timeout);
// int sys_ShutDown(Fid_t sock, shutdown_mode how);
// int socket_read(void* socketcb_t, char* buf, unsigned int len); 
// int socket_write(void* socketcb_t, const char* buf, unsigned int len); 
// int socket_close(void* socketcb_t); 



/**
	@brief Increase the reference count of an fcb 

	@param fcb the fcb whose reference count will be increased
*/
void FCB_incref(FCB* fcb);


/**
	@brief Decrease the reference count of the fcb.

	If the reference count drops to 0, release the FCB, calling the 
	Close method and returning its return value.
	If the reference count is still >0, return 0. 

	@param fcb  the fcb whose reference count is decreased
	@returns if the reference count is still >0, return 0, else return the value returned by the
	     `Close()` operation
*/
int FCB_decref(FCB* fcb);


/** @brief Acquire a number of FCBs and corresponding fids.

   Given an array of fids and an array of pointers to FCBs  of
   size @ num, this function will check is available resources
   in the current process PCB and FCB are available, and if so
   it will fill the two arrays with the appropriate values.
   If not, the state is unchanged (but the array contents
   may have been overwritten).

   If these resources are not needed, the operation can be
   reversed by calling @ref FCB_unreserve.

   @param num the number of resources to reserve.
   @param fid array of size at least `num` of `Fid_t`.
   @param fcb array of size at least `num` of `FCB*`.
   @returns 1 for success and 0 for failure.
*/
int FCB_reserve(size_t num, Fid_t *fid, FCB** fcb);


/** @brief Release a number of FCBs and corresponding fids.

   Given an array of fids of size @ num, this function will 
   return the fids to the free pool of the current process and
   release the corresponding FCBs.

   This is the opposite of operation @ref FCB_reserve. 
   Note that this is very different from closing open fids.
   No I/O operation is performed by this function.

   This function does not check its arguments for correctness.
   Use only with arrays filled by a call to @ref FCB_reserve.

   @param num the number of resources to unreserve.
   @param fid array of size at least `num` of `Fid_t`.
   @param fcb array of size at least `num` of `FCB*`.
*/
void FCB_unreserve(size_t num, Fid_t *fid, FCB** fcb);


/** @brief Translate an fid to an FCB.

	This routine will return NULL if the fid is not legal.

	@param fid the file ID to translate to a pointer to FCB
	@returns a pointer to the corresponding FCB, or NULL.
 */
FCB* get_fcb(Fid_t fid);


/** @} */

#endif
