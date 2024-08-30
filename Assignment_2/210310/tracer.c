#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>


///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////
//remember to check the boundary cases
//
int is_valid_mem_range(unsigned long buff, u32 count, int access_bit) {
    struct exec_context *current = get_current_ctx();
//	//printk("flag 0\n");

    if (current == NULL)
        return -EBADMEM; // Error: Invalid execution context
//	printk("flag 3\n");

    struct mm_segment *mms = current->mms;
    struct vm_area *vma = current->vm_area;
    // Check mm_segment areas
    for (int i = 0; i < MAX_MM_SEGS; i++) {
	if(i == MM_SEG_STACK)
	{
//		printk("buff is %d, count is %d, and [%d, %d]\n",buff,count,mms[i].start,mms[i].end);
        	if (buff >= mms[i].start && buff + count <= mms[i].end) {
			if ((mms[i].access_flags & access_bit) == access_bit) {
               		 	return 0; // Valid memory range with required access
           	      	}
        	}
		continue;
	}
//	printk("buff is %d, count is %d, and [%d, %d]\n",buff,count,mms[i].start,mms[i].next_free);
        if (buff >= mms[i].start && buff + count <= mms[i].next_free) {
            if ((mms[i].access_flags & access_bit) == access_bit) {
                return 0; // Valid memory range with required access
            }
        }
    }
//	printk("flag 1\n");

    // Check vm_area regions
    while (vma != NULL) {
        if (buff >= vma->vm_start && buff + count <= vma->vm_end) {
            if ((vma->access_flags & access_bit) == access_bit) {
                //printk("yes its mmap \n");
		return 0; // Valid memory range with required access
            	
		}
        }
        vma = vma->vm_next;
    }
//	printk("flag 2\n");
    return -EBADMEM; // Invalid memory range
}

long trace_buffer_close(struct file *filep) 
{
    if (filep == NULL || filep->type != TRACE_BUFFER)
        return -EINVAL; // Error: Invalid file or not a trace buffer

    // Release allocated memory
    if (filep->trace_buffer != NULL) {
        os_page_free(USER_REG, filep->trace_buffer->alloc);
        os_page_free(USER_REG, filep->trace_buffer);
    }

    os_free(filep->fops, sizeof(filep->fops));
    os_free(filep, sizeof(filep));

    return 0; // Success
}

// Helper function to wrap around offsets for circular buffers
static unsigned int wrap_around(unsigned int offset, unsigned int buffer_size) {
    return offset % buffer_size;
}

// Implementation of the write function with wrapping
int trace_buffer_write(struct file *filep, char *buff, u32 count) 
{
	//printk("flaggg\n");	
    struct exec_context *current = get_current_ctx();

    if (filep == NULL || current == NULL)
        return -EINVAL; // Error: Invalid file or context

    // Check if the file type is TRACE_BUFFER
    if (filep->type != TRACE_BUFFER)
        return -EINVAL; // Error: Not a trace buffer

    struct trace_buffer_info *trace_buffer = filep->trace_buffer;
    if (trace_buffer == NULL)
        return -EINVAL; // Error: Trace buffer not initialized

    // Check if the memory range is valid for writing
    if (is_valid_mem_range((unsigned long)buff, count, MM_RD)!=0){
        return -EBADMEM; // Error: Invalid memory range
}
    // Determine the write offset
    unsigned int write_offset = wrap_around(trace_buffer->write_off, TRACE_BUFFER_MAX_SIZE);

    // Calculate the available space for writing
    unsigned int available_space = TRACE_BUFFER_MAX_SIZE - write_offset;
	unsigned int i = 0;
    if (count == 0)
        return 0; // Nothing to write, return 0


        // Wrap around and write the remaining data
	if(!(trace_buffer -> isfull)){
        for (i = 0; i < count; i++) {
            write_offset = wrap_around(write_offset, TRACE_BUFFER_MAX_SIZE);
            trace_buffer->alloc[write_offset] = buff[i];
            write_offset++;
	    if(wrap_around(write_offset, TRACE_BUFFER_MAX_SIZE) == wrap_around(trace_buffer->read_off, TRACE_BUFFER_MAX_SIZE))
	    {	trace_buffer->isfull = 1;
		i =  i + 1;
		break;
	    }
        }
	}
    		trace_buffer->write_off = write_offset;
		return i;
	
    // Update the write offset
}
// Implementation of the read function with wrapping
int trace_buffer_read(struct file *filep, char *buff, u32 count) 
{
	//printk("reachedHeredebug\n");

    struct exec_context *current = get_current_ctx();

    if (filep == NULL || current == NULL)
        return -EINVAL; // Error: Invalid file or context
	//printk("reachedHeredebug\n");

    // Check if the file type is TRACE_BUFFER
    if (filep->type != TRACE_BUFFER)
        return -EINVAL; // Error: Not a trace buffer
	//printk("reachedHeredebug\n");

    struct trace_buffer_info *trace_buffer = filep->trace_buffer;
    if (trace_buffer == NULL)
        return -EINVAL; // Error: Trace buffer not initialized

    // Check if the memory range is valid for reading
    if (is_valid_mem_range((unsigned long)buff, count, MM_WR)!=0)
	{
        return -EBADMEM; // Error: Invalid memory range
	}
    // Determine the read offset
	//printk("reachedHeredebug\n");
    unsigned int read_offset = wrap_around(trace_buffer->read_off, TRACE_BUFFER_MAX_SIZE);

    // Calculate the available data for reading
    unsigned int available_data = TRACE_BUFFER_MAX_SIZE - read_offset;
	//printk("reachedHeredebug\n");
    if (count == 0)
        return 0; // Nothing to read, return 0
//	//printk("%d\n",available_data);
    // Check if the available data is insufficient for the entire read
	unsigned int i;
	//boundary case
	if(wrap_around(read_offset, TRACE_BUFFER_MAX_SIZE) == wrap_around(trace_buffer->write_off, TRACE_BUFFER_MAX_SIZE) && (!(trace_buffer -> isfull)))
		return 0;

	//updating isfull of this trace buffer
	trace_buffer-> isfull = 0;
    if (count <= available_data) {
        // Read the data without wrapping
        for (i = 0; i < count; i++) {
	    //check for if read off aage to nahi chala gaya
            buff[i] = trace_buffer->alloc[read_offset];
            read_offset++;
	    if(wrap_around(read_offset, TRACE_BUFFER_MAX_SIZE) == wrap_around(trace_buffer->write_off, TRACE_BUFFER_MAX_SIZE))
    		{trace_buffer->read_off = read_offset;
	    	i = i + 1;
		break;
		}
        }
    } else {
        // Wrap around and read the remaining data
        for (i = 0; i < count; i++) {
            read_offset = wrap_around(read_offset, TRACE_BUFFER_MAX_SIZE);
            buff[i] = trace_buffer->alloc[read_offset];
            read_offset++;
	    if(wrap_around(read_offset, TRACE_BUFFER_MAX_SIZE) == wrap_around(trace_buffer->write_off, TRACE_BUFFER_MAX_SIZE)){
    		trace_buffer->read_off = read_offset;	
		i = i + 1;
		break;
	    }
        }
    }

    // Update the read offset
    trace_buffer->read_off = read_offset;
	return i;
}
int sys_create_trace_buffer(struct exec_context *current, int mode) 
{
    if (current == NULL)
        return -EINVAL; // Error: Invalid exec context

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (current->files[i] == NULL) {
            // Create a trace buffer file descriptor
            struct file *TBF = alloc_file();
            if (TBF == NULL)
                return -ENOMEM; // Error: Memory allocation failed

            TBF->type = TRACE_BUFFER;
            TBF->mode = mode;
            TBF->offp = 0;
            TBF->ref_count = 1;
            TBF->inode = NULL;

            // Allocate and initialize trace buffer
            struct trace_buffer_info *trace_buffer = (struct trace_buffer_info *)os_page_alloc(USER_REG);
            if (trace_buffer == NULL) {
                os_free(TBF,sizeof(TBF));
                return -ENOMEM; // Error: Memory allocation failed
            }
            trace_buffer->read_off = 0;
            trace_buffer->write_off = 0;
		trace_buffer->isfull = 0;
            char *allocation = (char *)os_page_alloc(USER_REG);
            if (allocation == NULL) {
                os_page_free(USER_REG, trace_buffer);
                os_free(TBF,sizeof(TBF));
                return -ENOMEM; // Error: Memory allocation failed
            }
            trace_buffer->alloc = allocation;
            TBF->trace_buffer = trace_buffer;

            // Set file operations
            TBF->fops = (struct fileops *)os_alloc(sizeof(struct fileops));
            if (TBF->fops == NULL) {
                os_page_free(USER_REG, allocation);
                os_page_free(USER_REG, trace_buffer);
                os_free(TBF, sizeof(TBF));
                return -ENOMEM; // Error: Memory allocation failed
            }
            TBF->fops->read = &trace_buffer_read;
            TBF->fops->write = &trace_buffer_write;
            TBF->fops->close = &trace_buffer_close;

            // Set the file descriptor in the process's files array
            current->files[i] = TBF;

            return i; // Return the file descriptor index
        }
    }

    // No available file descriptor slots
    return -EINVAL; // Error: No available file descriptor
}
///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////




int arguments(u64 syscall_num){
	int arg=0;
	if (syscall_num == 2 || syscall_num == 10 || syscall_num == 11 || syscall_num == 13 || syscall_num == 15 || syscall_num == 20 || syscall_num == 21 || syscall_num == 22 || syscall_num == 38 || syscall_num == 61)
	{
		arg=0;
	}
	else if (syscall_num == 1 || syscall_num == 6 || syscall_num == 7 || syscall_num == 12 || syscall_num == 14 || syscall_num == 19 || syscall_num == 27 || syscall_num == 29 ||syscall_num == 36)
	{
		arg=1;
	}
	else if (syscall_num == 4 || syscall_num == 8 || syscall_num == 9 || syscall_num == 17 || syscall_num == 23 || syscall_num == 28 || syscall_num == 37 || syscall_num == 40)
	{
		arg=2;
	}
	else if (syscall_num == 5 || syscall_num == 18 || syscall_num == 24 || syscall_num == 25 || syscall_num == 30 || syscall_num == 39 || syscall_num == 41)
	{
		arg=3;
	}
	else if (syscall_num == 16 || syscall_num == 35)
	{
		arg=4;
	}
	else
	{
		return -1;
	}
	return arg;
}
int trace_buffer_write_k(struct file *filep, char *buff, u32 count) 
{
    struct exec_context *current = get_current_ctx();
    struct trace_buffer_info *trace_buffer = filep->trace_buffer;
    // Determine the write offset
    unsigned int write_offset = wrap_around(trace_buffer->write_off, TRACE_BUFFER_MAX_SIZE);
    // Calculate the available space for writing
    unsigned int available_space = TRACE_BUFFER_MAX_SIZE - write_offset;
	unsigned int i = 0;
        // Wrap around and write the remaining data
	if(!(trace_buffer -> isfull)){
        for (i = 0; i < count; i++) {
            write_offset = wrap_around(write_offset, TRACE_BUFFER_MAX_SIZE);
            trace_buffer->alloc[write_offset] = buff[i];
            write_offset++;
	    if(wrap_around(write_offset, TRACE_BUFFER_MAX_SIZE) == wrap_around(trace_buffer->read_off, TRACE_BUFFER_MAX_SIZE))
	    {	trace_buffer->isfull = 1;
		i++;
		break;
	    }
        }
	}
    	trace_buffer->write_off = write_offset;
	return i;	
}

int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4) {
    // Get the current context
	if(syscall_num == 38) return 0;
    struct exec_context *current = get_current_ctx();

    if (current == NULL) {
        return -EINVAL; // Error: Invalid execution context
    }

    // Check if system call tracing is enabled

    if (current->st_md_base == NULL || current->st_md_base->is_traced == 0) {
	//printk("returned due to off\n");
        return 0; // Tracing is not enabled, no need to trace this call
    }

    // Determine the number of parameters to write
    int CountArgs = arguments(syscall_num);

    // Write the traced information to the trace buffer
    struct file *trace_buffer = current->files[current->st_md_base->strace_fd];
    if (trace_buffer == NULL || trace_buffer->type != TRACE_BUFFER) {
        return -EINVAL; // Error: Invalid trace buffer
    }
	//printk("%d\n",current->st_md_base->is_traced);

    // Write the system call number to the trace buffer
if(current -> st_md_base -> tracing_mode == FULL_TRACING){
    int tt = trace_buffer_write_k(trace_buffer, (char*)&syscall_num, sizeof(u64));

	//printk("%d---sn%d---%d-- --%s ---syscalnum\n",current->files[3]->trace_buffer->write_off,syscall_num,tt, (char*)&syscall_num);
    // Write the parameters to the trace buffer
    if (CountArgs >= 1) {
        trace_buffer_write_k(trace_buffer, (char*)&param1, sizeof(u64));
    }
    if (CountArgs >= 2) {
        trace_buffer_write_k(trace_buffer, (char*)&param2, sizeof(u64));
    }
    if (CountArgs >= 3) {
        trace_buffer_write_k(trace_buffer, (char*)&param3, sizeof(u64));
    }
    if (CountArgs >= 4) {
        trace_buffer_write_k(trace_buffer, (char*)&param4, sizeof(u64));
    }


    return 0; // Success
	}
	else{
		//printk("inelse\n");
		struct strace_info *si = current -> st_md_base -> next;
		while(si){
			if(si->syscall_num == syscall_num){
			    int tt = trace_buffer_write_k(trace_buffer, (char*)&syscall_num, sizeof(u64));
	
				//printk("%d---sn%d---%d-- --%s ---syscalnum\n",current->files[3]->trace_buffer->write_off,syscall_num,tt, (char*)&syscall_num);
	 			   // Write the parameters to the trace buffer
 				   if (CountArgs >= 1) {
 				       trace_buffer_write_k(trace_buffer, (char*)&param1, sizeof(u64));
 	  				 }
 	 			  if (CountArgs >= 2) {
 	     				  trace_buffer_write_k(trace_buffer, (char*)&param2, sizeof(u64));
 				   }
			  	  if (CountArgs >= 3) {
   			    		 trace_buffer_write_k(trace_buffer, (char*)&param3, sizeof(u64));
  				  }
  			 	 if (CountArgs >= 4) {
  				      trace_buffer_write_k(trace_buffer, (char*)&param4, sizeof(u64));
   				 }
				return 0;
			}
			si = si -> next;
		}
		
		return 0;
	}
}

int sys_strace(struct exec_context *current, int syscall_num, int action) {
    if (action != ADD_STRACE && action != REMOVE_STRACE) {
        return -EINVAL; // Invalid action.
    }

    if (!current->st_md_base) {
	current -> st_md_base = (struct strace_head*)os_alloc(sizeof(struct strace_head));
    }
	//printk("debug --flag2\n");


    // Allocate memory for a new strace_info structure.
    struct strace_info *entry = os_alloc(sizeof(struct strace_info));
    if (!entry) {
        return -ENOMEM; // Memory allocation failed.
    }

    entry->syscall_num = syscall_num;
    entry->next = NULL;
	//printk("debug --flag1\n");
    // Add or remove the traced syscall based on the action.
    if (action == ADD_STRACE) {
	if(current->st_md_base->count == MAX_STRACE) return -EINVAL;
        // Add the new strace_info structure to the end of the list.
        if (!current->st_md_base->next) {
            current->st_md_base->next = entry;
            current->st_md_base->last = entry;
        } else {
            current->st_md_base->last->next = entry;
            current->st_md_base->last = entry;
        }

        // Increment the count of traced system calls.
        current->st_md_base->count++;
    } else {
        // Remove the traced syscall from the list if it exists.
        struct strace_info *prev = NULL;
        struct strace_info *current_entry = current->st_md_base->next;
        while (current_entry) {
            if (current_entry->syscall_num == syscall_num) {
                if (prev) {
                    prev->next = current_entry->next;
                }
                if (current_entry == current->st_md_base->next) {
                    current->st_md_base->next = current_entry->next;
                }
                if (current_entry == current->st_md_base->last) {
                    current->st_md_base->last = prev;
                }

                os_free(current_entry,sizeof(current_entry));
                // Decrement the count of traced system calls.
                current->st_md_base->count--;
                break;
            }
            prev = current_entry;
            current_entry = current_entry->next;
        }
    }

    return 0; // Success.
}
int trace_buffer_read_k(struct file *filep, char *buff, u32 count)
{
    struct exec_context *current = get_current_ctx();
    struct trace_buffer_info *trace_buffer = filep->trace_buffer;
    unsigned int read_offset = wrap_around(trace_buffer->read_off, TRACE_BUFFER_MAX_SIZE);
    int syscalls_read = 0;  // Counter for the number of syscalls read
	int bytes_read = 0;

    while (syscalls_read < count && (!((read_offset == trace_buffer->write_off) && (trace_buffer -> isfull == 0)))) {
	trace_buffer -> isfull = 0;
        unsigned long syscall_num;
        // Read the syscall number
        //memcpy(&syscall_num, trace_buffer->alloc + read_offset, sizeof(syscall_num));
	(syscall_num) = *(u64*)(trace_buffer->alloc + read_offset);

        // Determine the number of arguments based on the syscall number
        int args_to_read = arguments(syscall_num) * sizeof(u64) + sizeof(u64);
        // Check if there's enough space to read the arguments
            for (int i = 0; i < args_to_read; i++) {

                buff[bytes_read] = trace_buffer->alloc[read_offset];
                bytes_read++;
                read_offset = wrap_around(read_offset + 1, TRACE_BUFFER_MAX_SIZE);
            }

	//printk("syscal is read with cb %d argss %d\n)",bytes_read,arguments(syscall_num));	
        // Increment the number of syscalls read
        syscalls_read++;
    }

    trace_buffer->read_off = read_offset;
    return bytes_read;
}

int sys_read_strace(struct file *filep, char *buff, u64 count)
{
	//Assuming buff is writable, now just read everything from buffer ithink count bytes, as it already stores the info in given 8 byte chunks
	return trace_buffer_read_k(filep, buff, count);
}

int sys_start_strace(struct exec_context *current, int fd, int tracing_mode) {
    // Check if the current context is valid
    if (current == NULL) {
        return -EINVAL; // Error: Invalid execution context
    }

    // Check if the provided file descriptor corresponds to a valid trace buffer
    struct file *trace_buffer = current->files[fd];
    if (trace_buffer == NULL || trace_buffer->type != TRACE_BUFFER) {
        return -EINVAL; // Error: Invalid trace buffer
    }
	

    // Initialize system call tracing based on the tracing mode
    if (current->st_md_base == NULL) {
        current -> st_md_base = (struct strace_head*)os_alloc(sizeof(current -> st_md_base));
    }

    if (current->st_md_base == NULL) {
	return -ENOMEM; // Error: Memory allocation failed
    }

    // Initialize the newly allocated st_md_base
    current->st_md_base->is_traced = 1;
    current->st_md_base->strace_fd = fd;
    current->st_md_base->tracing_mode = tracing_mode;
    return 0; // Success
}

int sys_end_strace(struct exec_context *current) {
    if (!current->st_md_base) {
        return -EINVAL; // Error: Missing strace_head for the current context.
    }

    // Free the list of traced system calls (strace_info) for the current context.
    struct strace_info *current_entry = current->st_md_base->next;
    while (current_entry) {
        struct strace_info *next = current_entry->next;
        os_free(current_entry,sizeof(current_entry));
        current_entry = next;
    }

    // Free the st_md_base structure.
    os_free(current->st_md_base,sizeof(current->st_md_base));

    current->st_md_base = NULL; // Set the st_md_base to NULL to indicate no more tracing.

    return 0; // Success.
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////


long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{
    return 0;
}

//Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{
        return 0;
}


int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{
    return 0;
}


#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>


///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
