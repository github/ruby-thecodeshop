/*
 * this is generic pool allocator
 * you should define following macroses:
 * ITEM_NAME - unique identifier, which allows to hold functions in a namespace
 * ITEM_TYPEDEF(name) - passed to typedef to localize item type
 * free_entry - desired name of function for free entry
 * alloc_entry - defired name of function for allocate entry
 */

#if POOL_ALLOC_PART == 1
#ifdef HEAP_ALIGN_LOG
#define DEFAULT_POOL_SIZE (1 << HEAP_ALIGN_LOG)
#else
#define DEFAULT_POOL_SIZE (sizeof(void*) * 2048)
#endif
typedef unsigned int pool_holder_counter;

typedef struct pool_entry_list pool_entry_list;
typedef struct pool_holder pool_holder;

typedef struct pool_header {
    pool_holder         *first;
    rb_atomic_t          lock;
    pool_holder_counter  size; // size of entry in sizeof(void*) items
    pool_holder_counter  total; // size of entry in sizeof(void*) items
} pool_header;

struct pool_holder {
    pool_holder_counter free, total;
    pool_header  *header;
    void               *freep;
    pool_holder        *fore, *back;
    void *data[1];
};
#define POOL_DATA_SIZE(pool_size) (((pool_size) - sizeof(void*) * 6 - offsetof(pool_holder, data)) / sizeof(void*))
#define POOL_ENTRY_SIZE(item_type) ((sizeof(item_type) - 1) / sizeof(void*) + 1)
#define POOL_HOLDER_COUNT(pool_size, item_type) (POOL_DATA_SIZE(pool_size)/POOL_ENTRY_SIZE(item_type))
#define INIT_POOL(item_type) {NULL, 0, POOL_ENTRY_SIZE(item_type), POOL_HOLDER_COUNT(DEFAULT_POOL_SIZE, item_type)}

#elif POOL_ALLOC_PART == 2

#if   defined(_WIN32)
#define native_thread_yield() Sleep(0)
#elif HAVE_SCHED_YIELD
#define native_thread_yield() (void)sched_yield()
#else
#define native_thread_yield() ((void)0)
#endif

#define MAX_TRY_CICLES 5
static inline int
living_threads()
{
    rb_vm_t *vm = GET_VM();
    st_table *living_threads;
    return vm && (living_threads = vm->living_threads) ? living_threads->num_entries : 1;
}

static void
lock_header(pool_header *header)
{
    int i;
    if (living_threads() == 1) {
	header->lock = 1;
	return;
    }
    i = MAX_TRY_CICLES;
    while(ATOMIC_EXCHANGE(header->lock, 1)) {
	if (--i == 0) {
	    native_thread_yield();
	    i = MAX_TRY_CICLES;
	}
    }
}

static inline void
unlock_header(pool_header *header)
{
    if (living_threads() == 1) {
	header->lock = 0;
	return;
    }
    ATOMIC_SET(header->lock, 0);
}

static pool_holder *
pool_holder_alloc(pool_header *header)
{
    pool_holder *holder;
    pool_holder_counter i, size, count;
    register void **ptr;

    size_t sz = offsetof(pool_holder, data) +
	    header->size * header->total * sizeof(void*);
#define objspace (&rb_objspace)
    unlock_header(header);
    vm_malloc_prepare(objspace, DEFAULT_POOL_SIZE);
    lock_header(header);
    if (header->first != NULL) {
	return header->first;
    }
    holder = (pool_holder*) aligned_malloc(DEFAULT_POOL_SIZE, sz);
    if (!holder) {
	unlock_header(header);
	if (!garbage_collect_with_gvl(objspace)) {
	    ruby_memerror();
	}
	holder = (pool_holder*) aligned_malloc(DEFAULT_POOL_SIZE, sz);
	if (!holder) {
	    ruby_memerror();
	}
	lock_header(header);
    }
    malloc_increase += DEFAULT_POOL_SIZE;
#if CALC_EXACT_MALLOC_SIZE
    objspace->malloc_params.allocated_size += DEFAULT_POOL_SIZE;
    objspace->malloc_params.allocations++;
#endif
#undef objspace

    size = header->size;
    count = header->total;
    holder->free = count;
    holder->total = count;
    holder->header = header;
    holder->fore = NULL;
    holder->back = NULL;
    holder->freep = &holder->data;
    ptr = holder->data;
    for(i = count - 1; i; i-- ) {
	ptr = *ptr = ptr + size;
    }
    *ptr = NULL;
    header->first = holder;
    return holder;
}

static inline void
pool_holder_unchaing(pool_header *header, pool_holder *holder)
{
    register pool_holder *fore = holder->fore, *back = holder->back;
    holder->fore = NULL;
    holder->back = NULL;
    if (fore != NULL)  fore->back     = back;
    if (back != NULL)  back->fore     = fore;
    else               header->first = fore;
}

static inline pool_holder *
entry_holder(void **entry)
{
    return (pool_holder*)(((uintptr_t)entry) & ~(DEFAULT_POOL_SIZE - 1));
}

static inline void
pool_free_entry(void **entry)
{
    pool_holder *holder = entry_holder(entry);
    pool_header *header = holder->header;

    lock_header(header);

    if (holder->free++ == 0) {
	register pool_holder *first = header->first;
	if (first == NULL) {
	    header->first = holder;
	} else {
	    holder->back = first;
	    holder->fore = first->fore;
	    first->fore = holder;
	    if (holder->fore)
		holder->fore->back = holder;
	}
    } else if (holder->free == holder->total && header->first != holder ) {
	pool_holder_unchaing(header, holder);
	aligned_free(holder);
#if CALC_EXACT_MALLOC_SIZE
	rb_objspace.malloc_params.allocated_size -= DEFAULT_POOL_SIZE;
	rb_objspace.malloc_params.allocations--;
#endif
        unlock_header(header);
	return;
    }

    *entry = holder->freep;
    holder->freep = entry;
    unlock_header(header);
}

static inline void*
pool_alloc_entry(pool_header *header)
{
    pool_holder *holder;
    void **result;

    lock_header(header);
    holder = header->first;

    if (holder == NULL) {
	holder = pool_holder_alloc(header);
    }

    result = holder->freep;
    holder->freep = *result;

    if (--holder->free == 0) {
	pool_holder_unchaing(header, holder);
    }

    unlock_header(header);

    return result;
}

static void
pool_finalize_header(pool_header *header)
{
    if (header->first) {
        aligned_free(header->first);
        header->first = NULL;
    }
}
#endif
