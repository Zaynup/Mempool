#pragma once
#include <mutex>
#include <iostream>

// 封装了 malloc 和 free 操作，可以设置OOM释放内存的回调函数
template <int __inst>
class __malloc_alloc_template
{

private:
    static void *_S_oom_malloc(size_t);
    static void *_S_oom_realloc(void *, size_t);
    static void (*__malloc_alloc_oom_handler)();

public:
    static void *allocate(size_t __n)
    {
        void *__result = malloc(__n);
        if (0 == __result)
            __result = _S_oom_malloc(__n);
        return __result;
    }

    static void deallocate(void *__p, size_t /* __n */)
    {
        free(__p);
    }

    static void *reallocate(void *__p, size_t __old_sz, size_t __new_sz)
    {
        void *__result = realloc(__p, __new_sz);
        if (0 == __result)
            __result = _S_oom_realloc(__p, __new_sz);
        return __result;
    }

    static void (*__set_malloc_handler(void (*__f)()))()
    {
        void (*__old)() = __malloc_alloc_oom_handler;
        __malloc_alloc_oom_handler = __f;
        return (__old);
    }
};

template <int __inst>
void (*__malloc_alloc_template<__inst>::__malloc_alloc_oom_handler)() = nullptr;

template <int __inst>
void *__malloc_alloc_template<__inst>::_S_oom_malloc(size_t __n)
{
    void (*__my_malloc_handler)();
    void *__result;

    for (;;)
    {
        __my_malloc_handler = __malloc_alloc_oom_handler;
        if (0 == __my_malloc_handler)
        {
            throw std::bad_alloc();
        }
        (*__my_malloc_handler)();
        __result = malloc(__n);
        if (__result)
            return (__result);
    }
}

template <int __inst>
void *__malloc_alloc_template<__inst>::_S_oom_realloc(void *__p, size_t __n)
{
    void (*__my_malloc_handler)();
    void *__result;

    for (;;)
    {
        __my_malloc_handler = __malloc_alloc_oom_handler;
        if (0 == __my_malloc_handler)
        {
            throw std::bad_alloc();
            ;
        }
        (*__my_malloc_handler)();
        __result = realloc(__p, __n);
        if (__result)
            return (__result);
    }
}
typedef __malloc_alloc_template<0> malloc_alloc;

template <typename T>
class myallocator
{
public:
    using value_type = T;

    constexpr myallocator() noexcept {}
    constexpr myallocator(const myallocator &) noexcept = default;
    template <class Other>
    constexpr myallocator(const myallocator<Other> &) noexcept {}

    // 开辟内存
    T *allocate(size_t n)
    {
        n = n * sizeof(T);
        void *ret = 0;

        if (n > (size_t)MAX_BYTES_)
        {
            ret = malloc_alloc::allocate(n);
        }
        else
        {
            Obj_ *volatile *my_free_list = S_free_list_ + S_freelist_index_(n);

            std::lock_guard<std::mutex> guard(mtx); //智能锁

            Obj_ *result = *my_free_list;
            if (result == 0)
                ret = S_refill_(S_round_up_(n));
            else
            {
                *my_free_list = result->M_free_list_link_;
                ret = result;
            }
        }

        return (T *)ret;
    }

    //归还内存块
    void deallocate(void *p, size_t n)
    {
        if (n > (size_t)MAX_BYTES_)
        {
            malloc_alloc::deallocate(p, n);
        }
        else
        {
            Obj_ *volatile *my_free_list = S_free_list_ + S_freelist_index_(n);
            Obj_ *q = (Obj_ *)p;
            std::lock_guard<std::mutex> guard(mtx); //智能锁
            q->M_free_list_link_ = *my_free_list;
            *my_free_list = q;
        }
    }

    //内存扩容或者缩容
    static void *reallocate(void *p, size_t old_sz, size_t new_sz)
    {
        void *result;
        size_t copy_sz;

        if (old_sz > (size_t)MAX_BYTES_ && new_sz > (size_t)MAX_BYTES_)
        {
            return (realloc(p, new_sz));
        }
        if (S_round_up_(old_sz) == S_round_up_(new_sz))
        {
            return (p);
        }
        result = allocate(new_sz);
        copy_sz = new_sz > old_sz ? old_sz : new_sz;
        memcpy(result, p, copy_sz);
        deallocate(p, old_sz);
        return (result);
    }

    //对象构造
    void construct(T *p, const T &val)
    {
        new (p) T(val);
    }
    //对象析构
    void destroy(T *p)
    {
        p->~T();
    }

private:
    //将bytes上调至最临近的8的倍数
    static size_t S_round_up_(size_t bytes)
    {
        return (((bytes) + (size_t)ALIGN_ - 1) & ~((size_t)ALIGN_ - 1));
    }

    //返回bytes大小的小额区块位于free-list中的编号
    static size_t S_freelist_index_(size_t bytes)
    {
        return (((bytes) + (size_t)ALIGN_ - 1) / (size_t)ALIGN_ - 1);
    }

    //把开辟的连续内存划分成对应大小内存块并连接，添加到对应自由链表节点中
    static void *S_refill_(size_t n)
    {
        int nobjs = 20;
        char *chunk = S_chunk_alloc_(n, nobjs);
        Obj_ *volatile *my_free_list;
        Obj_ *result;
        Obj_ *current_obj;
        Obj_ *next_obj;
        int i;

        if (1 == nobjs)
        {
            return (chunk);
        }
        my_free_list = S_free_list_ + S_freelist_index_(n);

        result = (Obj_ *)chunk;
        *my_free_list = next_obj = (Obj_ *)(chunk + n);
        for (i = 1;; i++)
        {
            current_obj = next_obj;
            next_obj = (Obj_ *)((char *)next_obj + n);
            if (nobjs - 1 == i)
            {
                current_obj->M_free_list_link_ = 0;
                break;
            }
            else
            {
                current_obj->M_free_list_link_ = next_obj;
            }
        }
        return (result);
    }

    //开辟连续内存空间
    static char *S_chunk_alloc_(size_t size, int &nobjs)
    {
        char *result;
        size_t total_bytes = size * nobjs;
        size_t bytes_left = S_end_free_ - S_start_free_;

        if (bytes_left >= total_bytes)
        {
            result = S_start_free_;
            S_start_free_ += total_bytes;
            return (result);
        }
        else if (bytes_left >= size)
        {
            nobjs = (int)(bytes_left / size);
            total_bytes = size * nobjs;
            result = S_start_free_;
            S_start_free_ += total_bytes;
            return (result);
        }
        else
        {
            size_t bytes_to_get =
                2 * total_bytes + S_round_up_(S_heap_size_ >> 4);
            if (bytes_left > 0)
            {
                Obj_ *volatile *my_free_list = S_free_list_ + S_freelist_index_(bytes_left);

                ((Obj_ *)S_start_free_)->M_free_list_link_ = *my_free_list;

                *my_free_list = (Obj_ *)S_start_free_;
            }
            S_start_free_ = (char *)malloc(bytes_to_get);
            if (nullptr == S_start_free_)
            {
                size_t i;
                Obj_ *volatile *my_free_list;
                Obj_ *p;

                for (i = size; i <= (size_t)MAX_BYTES_; i += (size_t)ALIGN_)
                {
                    my_free_list = S_free_list_ + S_freelist_index_(i);
                    p = *my_free_list;
                    if (0 != p)
                    {
                        *my_free_list = p->M_free_list_link_;
                        S_start_free_ = (char *)p;
                        S_end_free_ = S_start_free_ + i;
                        return (S_chunk_alloc_(size, nobjs));
                    }
                }
                S_end_free_ = 0;
                S_start_free_ = (char *)malloc_alloc::allocate(bytes_to_get);
            }
            S_heap_size_ += bytes_to_get;
            S_end_free_ = S_start_free_ + bytes_to_get;
            return (S_chunk_alloc_(size, nobjs));
        }
    }

private:
    enum
    {
        ALIGN_ = 8
    }; // 自由链表是从8字节开始以8字节为对齐方式一直扩充到128字节
    enum
    {
        MAX_BYTES_ = 128
    }; // 内存池最大的内存块
    enum
    {
        NFREELISTS_ = 16
    }; // MAX_BYTES_/ALIGN_  自由链表（数组节点）的个数

    // 每一个内存块的头信息，M_free_list_link_存储下一个内存块的地址
    union Obj_
    {
        union Obj_ *M_free_list_link_;
        char M_client_data_[1];
    };

    //已分配的内存内存块的使用情况
    static char *S_start_free_;
    static char *S_end_free_;
    static size_t S_heap_size_;

    // S_free_list_表示存储自由链表数组的起始地址
    // volatile,防止多线程给这个数组的元素进行缓存，导致一个线程对它的修改另外一个线程无法及时看到
    static Obj_ *volatile S_free_list_[NFREELISTS_];

    // 内存池基于free_list实现，需要考虑线程安全
    static std::mutex mtx;
};

// 内存块使用情况初始化
template <typename T>
char *myallocator<T>::S_start_free_ = nullptr;
template <typename T>
char *myallocator<T>::S_end_free_ = nullptr;
template <typename T>
size_t myallocator<T>::S_heap_size_ = 0;

// 自由链表数组初始化
template <typename T>
typename myallocator<T>::Obj_ *volatile myallocator<T>::S_free_list_[NFREELISTS_] =
    {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
     nullptr, nullptr, nullptr, nullptr};

template <typename T>
std::mutex myallocator<T>::mtx;