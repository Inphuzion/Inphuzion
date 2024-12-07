#include "tinyos.h"
#include "util.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_dev.h"
#include "kernel_cc.h"

file_ops static reader_file_ops = {
    .Open = NULL,
    .Read = pipe_read,
    .Write = NULL,  // Readers do not write
    .Close = pipe_reader_close
};

file_ops static writer_file_ops = {
    .Open = NULL,
    .Read = NULL,  // Writers do not read
    .Write = pipe_write,
    .Close = pipe_writer_close
};

int sys_Pipe(pipe_t* pipe) {
    Fid_t r, w; // File descriptors for read and write ends
    FCB *read_fcb, *write_fcb; // File control blocks for reading and writing
    pipe_cb *pcb; // Pipe control block to manage the buffer


     // Allocate memory for the pipe control block
    pcb = (pipe_cb*)xmalloc(sizeof(pipe_cb));
    

    // Initialize the pipe control block
    pcb->reader = read_fcb;
    pcb->writer = write_fcb;
    pcb->r_position =0;
     pcb->w_position = 0;
    memset(pcb->BUFFER, 0, PIPE_BUFFER_SIZE); // Clear the buffer


    // Reserve FCBs and file descriptors
    if (!FCB_reserve(2, &r, &read_fcb)) {
        return -1; // Failed to reserve FCB for reader
    }
    if (!FCB_reserve(2, &w, &write_fcb)) {
        FCB_unreserve(1, &r, &read_fcb); // Rollback if writer reservation fails
        return -1; // Failed to reserve FCB for writer
    }

   

   	

    // Set up file operations for reader and writer
    read_fcb->streamobj = (void*)pcb;
    read_fcb->streamfunc = &reader_file_ops;

    write_fcb->streamobj = (void*)pcb;
    write_fcb->streamfunc = &writer_file_ops;

    // Assign the FCBs to the current process’s file descriptor table
    CURPROC->FIDT[r] = read_fcb;
    CURPROC->FIDT[w] = write_fcb;

    // Return success with the pipe file descriptors
    pipe->read = r;
    pipe->write = w;

    return 0; // Pipe successfully created
}

int pipe_read(void* pipecb_t, char* buf, unsigned int n) {
    pipe_cb* pcb = (pipe_cb*)pipecb_t;
    int i = 0;

    // Check if the read end is closed (and buffer is empty)
    if (pcb->reader == NULL && pcb->r_position == pcb->w_position) {
        return 0;  // End of data
    }

    // Loop to read data from the pipe
    while (i < n) {
        if (pcb->r_position == pcb->w_position) {
            if (pcb->writer == NULL) {
                return 0;  // End of data (write end closed)
            }
            // Block until data is available in the buffer
            kernel_wait(&pcb->has_data, SCHED_PIPE);  // Wait for data
        }

        // Read one byte from the buffer
        buf[i] = pcb->BUFFER[pcb->r_position];
        pcb->r_position = (pcb->r_position + 1) % PIPE_BUFFER_SIZE;

        // Notify writer that space is available
        kernel_broadcast(&pcb->has_space);

        i++;
    }

    return i;  // Return the number of bytes read
}

int pipe_write(void* pipecb_t, const char* buf, unsigned int n) {
    pipe_cb* pcb = (pipe_cb*)pipecb_t;
    int bytes_written = 0;

    // Continue writing until all requested bytes are written or the buffer is full
    while (bytes_written < n) {
        while ((pcb->w_position + 1) % PIPE_BUFFER_SIZE == pcb->r_position) {
            kernel_wait(&pcb->has_space,SCHED_PIPE);  // Wait until space is available
        }

        // Write data into the buffer at the current write position
        pcb->BUFFER[pcb->w_position] = buf[bytes_written];
        pcb->w_position = (pcb->w_position + 1) % PIPE_BUFFER_SIZE;
        bytes_written++;

        // Notify the reader that new data is available
        kernel_broadcast(&pcb->has_data);
    }

    return bytes_written;  // Return the number of bytes successfully written
}

int pipe_reader_close(void* _pipecb) {
    pipe_cb* pcb = (pipe_cb*)_pipecb;
    pcb->reader = NULL;  // Close the read end
    kernel_broadcast(&pcb->has_data);  // Notify that no more data is available

    return 0;  // Successfully closed
}

int pipe_writer_close(void* _pipecb) {
    pipe_cb* pcb = (pipe_cb*)_pipecb;
    pcb->writer = NULL;  // Close the write end
    kernel_broadcast(&pcb->has_data);  // Notify the reader that no more data will be written

    return 0;  // Successfully closed
}
