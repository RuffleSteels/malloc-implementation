#include <unistd.h>
#include <stdio.h>

typedef struct s_block *t_block;

struct s_block {
    size_t size;
    t_block next;
    int free;
    char data[1];  // provides a pointer to usable memory after the metadata
};

#define BLOCK_SIZE (sizeof(struct s_block) - 1)  // only the headers of s_block

#define align4(x) (((((x) - 1) >> 2) << 2) + 4) // snap to nearest multiple of 4 (equal or higher)

void *base = NULL;

t_block find_block(t_block *last, size_t size) {
    t_block b = base;
    while (b && !(b->free && b->size >= size)) {
        *last = b;
        b = b->next;
    }
    return b;
}

t_block extend_heap(t_block last, size_t s) {
    t_block b;
    b = sbrk(0);
    if (sbrk(BLOCK_SIZE + s) == (void *)-1)
        return NULL;
    b->size = s;
    b->next = NULL;
    b->free = 0;
    if (last)
        last->next = b;
    return b;
}

void split_block(t_block b, size_t s) {
    t_block new;
    new = (t_block)(b->data + s);  // move pointer 's' bytes forward
    new->size = b->size - s - BLOCK_SIZE;
    new->next = b->next;
    new->free = 1;
    b->size = s;
    b->next = new;
}

void *my_malloc(size_t size) {
    t_block b, last;
    size_t s = align4(size);

    if (base) { // if NULL then never allocated before, so has to extend heap
        last = base;
        b = find_block(&last, s); // finds the next block where there is enough space
        if (b) {
            if ((b->size - s) >= (BLOCK_SIZE + 4)) // if the block size can also accomodate extra free space (BLOCK_SIZE for metadata, + 4 for one piece of data)
                split_block(b, s);
            b->free = 0; // set this block as occupied
        } else {
            b = extend_heap(last, s);
            if (!b)
                return NULL;
        }
    } else {
        b = extend_heap(NULL, s);
        if (!b)
            return NULL;
        base = b;
    }
    return b->data;
}

int main() {
    double *x = (double *)my_malloc(sizeof(double));
    *x = 42.3;
    printf("x = %f\n", *x);
    return 0;
}