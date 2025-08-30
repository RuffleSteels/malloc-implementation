#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct s_block *t_block;

struct s_block {
    size_t size;
    t_block next;
    t_block prev;
    int free;
    void *ptr;
    char data[1];  // provides a pointer to usable memory after the metadata
};

#define BLOCK_SIZE 40  // only the headers of s_block

#define align4(x) (((((x) - 1) >> 2) << 2) + 4) // snap to nearest multiple of 4 (equal or higher)

void *base = NULL;

t_block get_block (void *p) {
    return (t_block)((char*)p - BLOCK_SIZE); // casts to char so that the math works in 1 byte steps (otherwise it would be in sizeof(tmp) steps) // moves the pointer back to the start of the metadata of the block
}

int valid_addr(void *p) {
    if (base) {
        if (p>base && p<sbrk(0)) {
            return (p == (get_block(p))->ptr); // one last check to see if the ptr lines up with usable memory after metadata in a block (otherwise ->ptr will return null)
        }
    }
    return 0;
}

t_block find_block(t_block *last, size_t size) {
    t_block b = base;
    while (b && !(b->free && b->size >= size)) {
        *last = b;
        b = b->next;
    }
    return b;
}

void copy_block(t_block src, t_block dst) {
    size_t i;
    char *sdata = src->ptr;
    char *ddata = dst->ptr;

    for (i=0; i < src->size && i < dst->size; i++) { // ptr[n] = *(ptr + n), and as sizeof(char) is 1 bytes, we are reading 1 bytes at a time (ptr[0] is bytes 0, [1] is 1...)
        ddata[i] = sdata[i];
    }
}

t_block extend_heap(t_block last, size_t s) {
    int sb;
    t_block b;
    b = sbrk(0);
    if (sbrk(BLOCK_SIZE + s) == (void *)-1)
        return (NULL);
    b->size = BLOCK_SIZE + s;
    b->next = NULL;
    b->prev = last;
    b->ptr = b->data;
    b->free = 0;
    if (last)
        last->next = b;
    return (b);
}

void split_block(t_block b, size_t s) {
    t_block new;
    new = (t_block)(b->data + s);  // move pointer 's' bytes forward
    new->size = b->size - s - BLOCK_SIZE;
    new->next = b->next;
    new->prev = b;
    new->free = 1;
    new->ptr = new->data;
    b->size = s;
    b->next = new;
    if (new->next) {
        new->next->prev = new;
    }
}

t_block fusion(t_block b) {
    if (b->next && b->next->free) {
        b->size += BLOCK_SIZE + b->next->size;
        b->next = b->next->next;
        if (b->next) {
            b->next->prev = b;
        }
    }
    return b;
}

void my_free(void *p) {
    t_block b;
    
    if (valid_addr(p)) {
        b = get_block(p);
        b->free = 1;


        if (b->prev && b->prev->free) {
            b = fusion(b->prev);
        }
        

        if (b->next && b->next->free) {
            b = fusion(b);
        }

        if (!(b->next)) {
            if (b->prev) { // see if this is the last block remaining
                b->prev->next = NULL;
            } else {
                base = NULL; // if so, then we have no more blocks (base back to null)
            }
            brk(b); // sets the heap line back to ptr b
        }
    }
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

void *my_realloc(void *p, size_t size) {
    size_t s;
    t_block b, new;
    void *newp;

    if (!p)
        return (my_malloc(size));

    if (valid_addr(p)) {
        s = align4(size);
        b = get_block(p);
        if (b->size >= s) {
            if ((b->size - s) >= (BLOCK_SIZE + 4)) {
                split_block(b, s);
                if (b->next->next && b->next->next->free)
                    fusion(b->next);
                if (!(b->next->next)) {
                    brk(b->next);
                }
            }
            return p;
        } else {
            if (b->next && b->next->free && (b->size + BLOCK_SIZE + b->next->size) >= s) {
                fusion(b);
                if (b->size - s >= (BLOCK_SIZE + 4)) {
                    split_block(b,s);
                }
            } else {
                newp = my_malloc(s);
                if (!newp)
                    return NULL;

                new = get_block(newp);
                copy_block(b,new);
                my_free(p);
                return (newp);
            }
        }
        return (p);
    }
    return (NULL);
}


int main() {

    return 0;
}