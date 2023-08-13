#include <iostream>
#include <cassert>
#include <memory>

static unsigned long roundup_pow_of_two(unsigned long v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;

	return v;
}

#define DECLARE_FIFO(type, name)					\
	struct {							\
		size_t front, back, size, mask;				\
		type *data;						\
	} name

#define fifo_for_each(c, fifo, iter)					\
	for (iter = (fifo)->front;					\
	     c = (fifo)->data[iter], iter != (fifo)->back;		\
	     iter = (iter + 1) & (fifo)->mask)

#define __init_fifo(fifo, gfp)						\
({									\
	size_t _allocated_size, _bytes;					\
									\
	_allocated_size = roundup_pow_of_two((fifo)->size + 1);		\
	_bytes = _allocated_size * sizeof(*(fifo)->data);		\
									\
	(fifo)->mask = _allocated_size - 1;				\
	(fifo)->front = (fifo)->back = 0;				\
									\
	(fifo)->data = (long int *)malloc(_bytes);			\
	(fifo)->data;							\
})

#define init_fifo_exact(fifo, _size, gfp)				\
({									\
	(fifo)->size = (_size);						\
	__init_fifo(fifo, gfp);						\
})

#define init_fifo(fifo, _size, gfp)					\
({									\
	(fifo)->size = (_size);						\
	if ((fifo)->size > 4)						\
		(fifo)->size = roundup_pow_of_two((fifo)->size) - 1;	\
	__init_fifo(fifo, gfp);						\
})

#define free_fifo(fifo)							\
do {									\
	kvfree((fifo)->data);						\
	(fifo)->data = NULL;						\
} while (0)

#define fifo_used(fifo)		(((fifo)->back - (fifo)->front) & (fifo)->mask)
#define fifo_free(fifo)		((fifo)->size - fifo_used(fifo))

#define fifo_empty(fifo)	(!fifo_used(fifo))
#define fifo_full(fifo)		(!fifo_free(fifo))

#define fifo_front(fifo)	((fifo)->data[(fifo)->front])
#define fifo_back(fifo)							\
	((fifo)->data[((fifo)->back - 1) & (fifo)->mask])

#define fifo_idx(fifo, p)	(((p) - &fifo_front(fifo)) & (fifo)->mask)

#define fifo_push_back(fifo, i)						\
({									\
	bool _r = !fifo_full((fifo));					\
	if (_r) {							\
		(fifo)->data[(fifo)->back++] = (i);			\
		(fifo)->back &= (fifo)->mask;				\
	}								\
	_r;								\
})

#define fifo_pop_front(fifo, i)						\
({									\
	bool _r = !fifo_empty((fifo));					\
	if (_r) {							\
		(i) = (fifo)->data[(fifo)->front++];			\
		(fifo)->front &= (fifo)->mask;				\
	}								\
	_r;								\
})

#define fifo_push_front(fifo, i)					\
({									\
	bool _r = !fifo_full((fifo));					\
	if (_r) {							\
		--(fifo)->front;					\
		(fifo)->front &= (fifo)->mask;				\
		(fifo)->data[(fifo)->front] = (i);			\
	}								\
	_r;								\
})

#define fifo_pop_back(fifo, i)						\
({									\
	bool _r = !fifo_empty((fifo));					\
	if (_r) {							\
		--(fifo)->back;						\
		(fifo)->back &= (fifo)->mask;				\
		(i) = (fifo)->data[(fifo)->back]			\
	}								\
	_r;								\
})

#define fifo_push(fifo, i)	fifo_push_back(fifo, (i))
#define fifo_pop(fifo, i)	fifo_pop_front(fifo, (i))

#define fifo_swap(l, r)							\
do {									\
	swap((l)->front, (r)->front);					\
	swap((l)->back, (r)->back);					\
	swap((l)->size, (r)->size);					\
	swap((l)->mask, (r)->mask);					\
	swap((l)->data, (r)->data);					\
} while (0)

#define fifo_move(dest, src)						\
do {									\
	typeof(*((dest)->data)) _t;					\
	while (!fifo_full(dest) &&					\
	       fifo_pop(src, _t))					\
		fifo_push(dest, _t);					\
} while (0)

typedef enum {
	GFP_KERNEL,
	GFP_ATOMIC,
	__GFP_HIGHMEM,
	__GFP_HIGH
} gfp_t;


struct cache {
	DECLARE_FIFO(long, free_inc);
};

int main()
{
	struct cache ca;
	long int *ret;
	size_t free;
	long bucket;

	free = 32; //  roundup_pow_of_two(20480) >> 10;
	
	ret = init_fifo(&ca.free_inc, free << 2, GFP_KERNEL);
	assert(ret != NULL);

	printf("empty:%u free:%lu used:%lu\n",
		fifo_empty(&ca.free_inc),
		fifo_free(&ca.free_inc),
		fifo_used(&ca.free_inc));

	while (!fifo_full(&ca.free_inc)) {
		fifo_push(&ca->free_inc, b - ca->buckets);

	}


	while (true) {
		if (!fifo_pop(&ca.free_inc, bucket)) {
			printf("nothing to pop\n");
			break;
		}

		printf("bucket:%ld empty:%u free:%lu used:%lu\n",
			bucket,
			fifo_empty(&ca.free_inc),
			fifo_free(&ca.free_inc),
			fifo_used(&ca.free_inc));
	}


	return 0;
}
