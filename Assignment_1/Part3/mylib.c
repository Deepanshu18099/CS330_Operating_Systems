#include <stdio.h>
#include <sys/mman.h>

// Constants for memory stuff
const unsigned long CHUNK_SIZE = 8;       // Size of each memory chunk
const unsigned long MIN_PADDING = 8;      // Min extra space for memory
const unsigned long MIN_CHUNK_SIZE = 16;  // Smallest memory chunk size
const unsigned long MMAP_CHUNK_SIZE = 4 * 1024 * 1024; // Size for mmap
const unsigned long MIN_SPLIT_SIZE = 24;  // Minimum for splitting

// Imaginary space for data
unsigned long *memory_head = NULL;

// Function to free a memory chunk
void free_memory_chunk(unsigned long *chunk) {
    if (!chunk)
        return;

    // If the chunk is the head, update memory_head
    if (memory_head == chunk) {
        memory_head = (unsigned long *)chunk[1];
        if (memory_head)
            memory_head[2] = (unsigned long)NULL;
    } else {
        // Update linked chunks
        if (chunk[1])
            ((unsigned long *)chunk[1])[2] = chunk[2];
        if (chunk[2])
            ((unsigned long *)chunk[2])[1] = chunk[1];
    }
}

// Function to split a memory chunk into two smaller chunks
void split_memory_chunk(unsigned long *chunk, unsigned long space) {
    unsigned long *new_chunk = chunk + space / CHUNK_SIZE;

    // Create a new chunk
    new_chunk[0] = chunk[0] - space;
    new_chunk[1] = (unsigned long)memory_head;
    new_chunk[2] = (unsigned long)NULL;

    // Link the new chunk to the previous one
    if (new_chunk[1])
        ((unsigned long *)new_chunk[1])[2] = (unsigned long)new_chunk;

    // Update memory_head to the new chunk
    memory_head = new_chunk;
}

// Function to allocate a new memory chunk
void allocate_memory_chunk(unsigned long *chunk, unsigned long space) {
    unsigned long *new_chunk = chunk;

    // Create a new chunk
    new_chunk[0] = space;
    new_chunk[1] = (unsigned long)memory_head;
    new_chunk[2] = (unsigned long)NULL;

    // Link the new chunk to the previous one
    if (new_chunk[1])
        ((unsigned long *)new_chunk[1])[2] = (unsigned long)new_chunk;

    // Update memory_head to the new chunk
    memory_head = new_chunk;
}

// Function to link two memory chunks together
void link_chunks(unsigned long *prev_chunk, unsigned long *next_chunk) {
    if (prev_chunk)
        prev_chunk[2] = (unsigned long)next_chunk;
    if (next_chunk)
        next_chunk[1] = (unsigned long)prev_chunk;
}

// Function to adjust padding based on size
unsigned long adjust_padding(unsigned long size) {
    unsigned long padding = CHUNK_SIZE - size % CHUNK_SIZE;
    if (size % CHUNK_SIZE == 0)
        padding = 0;
    if (size + CHUNK_SIZE + padding < MIN_CHUNK_SIZE)
        padding += MIN_PADDING;
    if (size + CHUNK_SIZE + padding < MIN_SPLIT_SIZE)
        padding += MIN_PADDING;
    return padding;
}

// Function to allocate memory of a certain size
void *memalloc(unsigned long size) {
    unsigned long *temp = memory_head;
    unsigned long padding = adjust_padding(size);

    while (temp) {
        unsigned long space = *temp;

        if (space < size + CHUNK_SIZE + padding) {
            temp = (unsigned long *)temp[1];
            continue;
        }

        if (space == size + CHUNK_SIZE + padding) {
            free_memory_chunk(temp);
            temp[0] = (unsigned long)space;
            return temp + 1;
        }

        if (space - size - CHUNK_SIZE - padding < MIN_SPLIT_SIZE) {
            free_memory_chunk(temp);
            temp[0] = (unsigned long)(space);
            return temp + 1;
        }

        if (space - size - CHUNK_SIZE - padding >= MIN_SPLIT_SIZE) {
            free_memory_chunk(temp);
            split_memory_chunk(temp, size + CHUNK_SIZE + padding);
            temp[0] = (unsigned long)(size + CHUNK_SIZE + padding);
            return temp + 1;
        }
    }

    unsigned long count = (size + CHUNK_SIZE + padding) / MMAP_CHUNK_SIZE;

    // Adjust count if needed
    if (MMAP_CHUNK_SIZE * count < size + CHUNK_SIZE + padding)
        count++;

    // Allocate memory using mmap
    unsigned long *allocated_memory = mmap(NULL, MMAP_CHUNK_SIZE * count, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocated_memory == MAP_FAILED) {
        return NULL;
    }

    // Check if the allocated memory can be split
    if ((MMAP_CHUNK_SIZE * count == size + CHUNK_SIZE + padding) || (MMAP_CHUNK_SIZE * count - size - CHUNK_SIZE - padding < MIN_SPLIT_SIZE)) {
        allocated_memory[0] = MMAP_CHUNK_SIZE * count;
        return allocated_memory + 1;
    }

    // Split the allocated memory chunk
    allocated_memory[0] = MMAP_CHUNK_SIZE * count;
    split_memory_chunk(allocated_memory, size + CHUNK_SIZE + padding);
    allocated_memory[0] = size + CHUNK_SIZE + padding;
    return allocated_memory + 1;
}

// Function to free previously allocated memory
int memfree(void *ptr) {
    unsigned long *allocated_memory = (unsigned long *)ptr;
    allocated_memory -= 1;
    unsigned long *temp = memory_head;
    unsigned long *previous_chunk = NULL;
    unsigned long *next_chunk = NULL;

    // Find adjacent memory chunks
    while (temp) {
        unsigned long chunk_size = *temp;
        unsigned long allocated_space = *allocated_memory;

        // Check if the current chunk is adjacent to the allocated memory
        if (temp + chunk_size / CHUNK_SIZE == allocated_memory) {
            previous_chunk = temp;
        }

        // Check if the allocated memory is adjacent to the current chunk
        if (allocated_memory + allocated_space / CHUNK_SIZE == temp) {
            next_chunk = temp;
        }

        temp = (unsigned long *)temp[1];
    }

    // Free memory chunks as needed
    if (previous_chunk)
        free_memory_chunk(previous_chunk);
    if (next_chunk)
        free_memory_chunk(next_chunk);

    // Allocate memory for the remaining space, if any
    if (!previous_chunk && !next_chunk) {
        allocate_memory_chunk(allocated_memory, *allocated_memory);
    } else if (!previous_chunk && next_chunk) {
        allocate_memory_chunk(allocated_memory, *allocated_memory + *next_chunk);
    } else if (previous_chunk && !next_chunk) {
        allocate_memory_chunk(previous_chunk, *previous_chunk + *allocated_memory);
    } else {
        allocate_memory_chunk(previous_chunk, *previous_chunk + *allocated_memory + *next_chunk);
    }
    return 0;
}
