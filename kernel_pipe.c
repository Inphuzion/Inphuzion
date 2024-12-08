#include "tinyos.h"
#include "util.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_dev.h"
#include "kernel_cc.h"

static  file_ops reader_file_ops = {
    .Open = NULL,
    .Read = pipe_read,
    .Write = not_allowed_r,  // Readers do not write
    .Close = pipe_reader_close
};

static file_ops writer_file_ops = {
    .Open = NULL,
    .Read = not_allowed_w,  // Writers do not read
    .Write = pipe_write,
    .Close = pipe_writer_close
};


int sys_Pipe(pipe_t* pipe) {	
    Fid_t fid[2];
    FCB* fcb[2];

    // Reserve file control blocks
    if (FCB_reserve(2, fid, fcb) != 1) return -1;
    pipe->read = fid[0];
    pipe->write = fid[1];

    // Allocate memory for the pipe control block
    pipe_cb* pcb = (pipe_cb*) xmalloc(sizeof(pipe_cb));
    if (pcb == NULL) return -1;

    // Initialize the pipe control block
    if (pcb == NULL || fcb[0] == NULL || fcb[1] == NULL) {
        return -1;
    }
    pcb->reader = fcb[0];
    pcb->writer = fcb[1];

    pcb->has_space = COND_INIT;
    pcb->has_data = COND_INIT;

    pcb->w_position = 0;
    pcb->r_position = 0;

    // Assign control block and operations
    fcb[0]->streamobj = pcb;
    fcb[1]->streamobj = pcb;

    fcb[0]->streamfunc = &reader_file_ops;
    fcb[1]->streamfunc = &writer_file_ops;

    return 0;
}


int pipe_read(void* pipecb_t, char* buf, unsigned int n) {
    // Cast the input pointer to the pipe control block structure
    pipe_cb* pcb = (pipe_cb*) pipecb_t;

    // Validate input
    if (!pcb || !pcb->reader) return -1;

    // Initialize the byte counter
    int bytesRead = 0;

    // Wait if the pipe is empty and a writer exists
    while (pcb->r_position == pcb->w_position && pcb->writer != NULL) {
        kernel_wait(&(pcb->has_data), SCHED_PIPE);
    }

    // Special case: No writer exists, but there may still be data to read
    if (pcb->writer == NULL) {
        while (pcb->r_position < pcb->w_position) {
            if (bytesRead == n) return bytesRead; // Stop if the buffer is full
            buf[bytesRead++] = pcb->BUFFER[pcb->r_position];
            pcb->r_position = (pcb->r_position + 1) % PIPE_BUFFER_SIZE;
        }
        return bytesRead;
    }

    // Read from the buffer until it reaches the writer position or the buffer limit
    while (bytesRead < n && pcb->r_position != pcb->w_position) {
        buf[bytesRead++] = pcb->BUFFER[pcb->r_position];
        pcb->r_position = (pcb->r_position + 1) % PIPE_BUFFER_SIZE;
    }

    // Signal that space is now available in the pipe
    kernel_broadcast(&(pcb->has_space));

    return bytesRead;
}



int pipe_write(void* pipecb_t, const char* buf, unsigned int n) {
    // Cast the input pointer to the pipe control block structure
    pipe_cb* pcb = (pipe_cb*) pipecb_t;

    // Validate input
    if (!pcb || !pcb->writer || !pcb->reader) return -1;

    // Initialize the counter for bytes written
    int bytesWritten = 0;

    // Wait if the buffer is full and a reader exists
    while ((pcb->w_position + 1) % PIPE_BUFFER_SIZE == pcb->r_position && pcb->reader != NULL) {
        kernel_wait(&(pcb->has_space), SCHED_PIPE);
    }

    // Write data to the buffer until it's full or all bytes are written
    while (bytesWritten < n && (pcb->w_position + 1) % PIPE_BUFFER_SIZE != pcb->r_position) {
        pcb->BUFFER[pcb->w_position] = buf[bytesWritten++];
        pcb->w_position = (pcb->w_position + 1) % PIPE_BUFFER_SIZE;
    }

    // Notify that data is available for reading
    kernel_broadcast(&(pcb->has_data));

    return bytesWritten; // Return the number of bytes successfully written
}



int pipe_writer_close(void* _pipecb) {
    pipe_cb* pcb = (pipe_cb*)_pipecb;
    pcb->writer = NULL;  // Close the write end
     if(pcb->reader == NULL){
		pcb = NULL;
		free(pcb);
		return 0;
	} // Close the read end
    kernel_broadcast(&pcb->has_data);  // Notify the reader that no more data will be written

    return 0;  // Successfully closed
}

int pipe_reader_close(void* _pipecb) {
    pipe_cb* pcb = (pipe_cb*)_pipecb;
    pcb->reader = NULL; 
    if(pcb->writer == NULL){
		pcb = NULL;
		free(pcb);
		return 0;
	}

    kernel_broadcast(&pcb->has_data);  // Notify that no more data is available

    return 0;  // Successfully closed
}




int not_allowed_r(void* pipecb_t,  char* buf, unsigned int n) {
	return  -1;}
int not_allowed_w(void* pipecb_t, const char* buf, unsigned int n) {
	return  -1;}

