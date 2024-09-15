#include <stdio.h>
#include <sys/mman.h>

#include "mem_alloc.h"

Header * get_header(void * mem) {
    return (Header *) ((char *)mem - sizeof(Header));
}

size_t get_size(Header * header) {
    return header->size - 1; 
}

void print_header(Header * header) {
    printf("    Addr: %p\n", header);
    printf("    Size: %zu\n", get_size(header));
    printf("    Previous: %p\n", header->previous);
    printf("    Next: %p\n", header->next);
}

void print_list() {
    if (free_list == NULL) {
        puts("(Empty List.)");
    } else {
        Header * current = free_list;
        while (current->next != NULL) {
            printf("%p -> ", current);
            current = current->next;
        }
        printf("%p\n", current);
    }
}

int is_allocated(Header * header) {
    return (header->size & 1);
}

int is_free(Header * header) {
    return (! is_allocated(header));
}

void set_allocated(Header * header) {
    header->size = (header->size | 1);
}

void set_free(Header * header) {
    header->size >>= 1;
    header->size <<= 1;
}

int same_page(Header * h1, Header * h2) {
    unsigned long mask = 0xFFFFF000;
    unsigned long h_1 = (unsigned long)h1 & mask;
    unsigned long h_2 = (unsigned long)h2 & mask;
    if (h_1 == h_2) {
        return 1;
    }
    return 0;
}

int mem_init() {
    if (mmap(NULL, PAGE_SIZE - sizeof(Header), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0) == MAP_FAILED) {
        return FAILURE;
    }
    free_list = mmap(NULL, PAGE_SIZE - sizeof(Header), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    free_list->size = PAGE_SIZE - sizeof(Header);
    free_list->previous = NULL;
    free_list->next = NULL;
    return SUCCESS;
}

int mem_extend(Header * last) {
    if (mmap(NULL, PAGE_SIZE - sizeof(Header), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0) == MAP_FAILED) {
        return FAILURE;
    }
    Header * new_page = mmap(NULL, PAGE_SIZE - sizeof(Header), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);  
    last->next = new_page;
    new_page->size = PAGE_SIZE - sizeof(Header);
    new_page->previous = last;
    new_page->next = NULL;
    return SUCCESS;
}

void * mem_alloc(size_t requested_size) {
    if (requested_size > PAGE_SIZE - sizeof(Header)) {
        return NULL;
    } 
    if (free_list == NULL) {
        int result = mem_init();
        if (result == FAILURE) {
            return NULL;
        }
    }
    size_t aligned_size;
    if (requested_size % WORD_SIZE != 0) {
        aligned_size = requested_size / WORD_SIZE + WORD_SIZE;
    } else {
        aligned_size = requested_size;
    }
    Header * current = free_list;
    while (current->next != NULL) {
        if (is_free(current) && aligned_size < current->size) {
            break;
        }
        current = current->next;
    }
    if (current->next == NULL && (aligned_size > current->size || is_allocated(current))) {
        int result = mem_extend(current); 
        if (result == FAILURE) {
            return NULL;
        }
        current = current->next;
    }
    char * payload_start = (char *)current;
    payload_start += sizeof(Header); 
    if (current->size > aligned_size + sizeof(Header)) {
        Header * new_header = (Header *) ((char *)current + sizeof(Header) + aligned_size);
        new_header->size = current->size - aligned_size - sizeof(Header);
        current->size = aligned_size;
        new_header->next = current->next;
        current->next = new_header;
        new_header->previous = current;
        if (new_header->next != NULL) {
            Header * next = new_header->next;
            next->previous = new_header;
        }
    } 
    set_allocated(current);
    return payload_start;
}

void mem_free(void * ptr) {
    Header * header = get_header(ptr);
    set_free(header);
    Header * next = header->next;
    if (next != NULL) {
        if (is_free(next) && same_page(header, next)) {
            header->next = next->next;
            header->size += next->size + sizeof(Header);
        }
    }
    Header * previous = header->previous;
    if (previous != NULL) {
        if (is_free(previous) && same_page(header, previous)) {
            header = previous;
            header->previous = previous->previous;
            header->size += previous->size + sizeof(Header);
        }
    }
    if (header->size == PAGE_SIZE - sizeof(Header)) {
        if (header->previous != NULL) {
            Header * previous_page = header->previous;
            previous_page->next = header->next;
        }
        if (header->next != NULL) {
            Header * next_page = header->next;
            next_page->previous = header->previous;
        }
        if (free_list == header) {
            free_list = header->next;
        }
        munmap(header, PAGE_SIZE - sizeof(Header));
    }
}
