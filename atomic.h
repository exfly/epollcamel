// typedef struct
// {
//     volatile int count;
// } atomic_t;

// static inline int atomic_xchg(atomic_t *v, int i)
// {
//     int ret;
//     asm volatile(
//         "xchgl %0, %1"
//         : "=r"(ret)
//         : "m"(v->count), "0"(i));
//     return ret;
// }
// https://blog.csdn.net/shenwansangz/article/details/50109835

#ifndef _CAMEL_ATOMIC_H_INCLUDED_
#define _CAMEL_ATOMIC_H_INCLUDED_

#define atomic_cmp_set(lock, old, set) \
    __sync_bool_compare_and_swap(lock, old, set)

#define atomic_fetch_add(value, add) \
    __sync_fetch_and_add(value, add)

typedef struct
{
    volatile int counter;
} atomic_t;

#define atomic_inc(value)                    \
    do                                       \
    {                                        \
        atomic_fetch_add(&value->counter, 1) \
    } while (0)

#endif
