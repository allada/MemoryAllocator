#ifndef MemoryAllocator_h
#define MemoryAllocator_h

#include <atomic>
#include <mutex>

// 2MB sized pages because most architectures/os's page memory in this size. This allows
#define _ALLOCATOR_DEFAULT_BITS 21
template <int SHIFT_BITS = _ALLOCATOR_DEFAULT_BITS>
struct MemoryBucket {
private:
    static constexpr size_t BUCKET_SIZE = 1 << SHIFT_BITS;
    static constexpr size_t BUCKET_MASK = BUCKET_SIZE - 1;

public:
    template <typename T>
    static T* nextPtr(size_t sz) {
        // Must always be even.
        if (sz & 1) {
            sz += 1;
        }
        MemoryBucket<SHIFT_BITS>* bucket = tipBucket_;
        bucket->meta_.used_count_.fetch_add(1);
        const uintptr_t basePtr = bucket->meta_.begin_ptr_;
        const size_t used_bytes = bucket->meta_.bytes_used_.fetch_add(sz);
        if (used_bytes + sz >= BUCKET_SIZE) {
            tryNewBucket_(sz);
            return nextPtr<T>(sz);
        }
        const uintptr_t endPtr = reinterpret_cast<uintptr_t>(&bucket->data_) + BUCKET_SIZE;
        const uintptr_t desiredPtr = basePtr + used_bytes;
fprintf(stderr, "tipBucket: %p\n", bucket);
fprintf(stderr, "basePtr: %p\n", basePtr);
fprintf(stderr, "desiredPtr: %lx\n", desiredPtr);
// fprintf(stderr, "Desired: %lu, start: %lu, used: %zu\n", desiredPtr, basePtr, used_bytes);
        bool isOverFence = bucket->meta_.is_over_fence_;
        if (desiredPtr + sz >= endPtr && !isOverFence) {
            {
                // We do not use try_lock() here because it can return false with no lock being active.
                ::std::lock_guard<::std::mutex> lock(bucket->meta_.over_fence_mux_);
                // Make sure we do not set it twice because of timing with lock.
                if (!bucket->meta_.is_over_fence_) {
                    bucket->meta_.bytes_used_.fetch_add(1);
                    bucket->meta_.is_over_fence_ = true;
                    isOverFence = true;
                }
            }
            bucket->meta_.used_count_.fetch_sub(1);
            return nextPtr<T>(sz);
        }
        if (desiredPtr + sz > endPtr && desiredPtr < endPtr) {
            // Run again. We are on the right fence post.
            bucket->meta_.used_count_.fetch_sub(1);
            return nextPtr<T>(sz);
        }
        if (desiredPtr + sz >= endPtr) {
            return reinterpret_cast<T*>(desiredPtr - BUCKET_SIZE);
        }

        return reinterpret_cast<T*>(desiredPtr);
    }

    template <typename T>
    static void freePtr(void* ptr) {
fprintf(stderr, "withPtr: %p\n", ptr);
        MemoryBucket* bucket = *memoryBucketPointerFromPointer(ptr);
fprintf(stderr, "bucketPtr: %p\n", bucket);
        size_t usedCount = bucket->meta_.used_count_.fetch_sub(1) - 1;
        if (usedCount == 0 && tipBucket_ != bucket) {
            delete bucket;
        }
    }
#ifndef UNITTEST
private:
#endif

    MemoryBucket() {
        uintptr_t ptr = reinterpret_cast<uintptr_t>(memoryBucketPointerFromPointer(this));
fprintf(stderr, "PTR: %p\n", ptr);
        new(reinterpret_cast<void*>(ptr)) MetaData_(reinterpret_cast<uintptr_t>(this));
        // Offset by 1 to tell memoryBucketPointerFromPointer that it should go to right not left.
        constexpr uint8_t used = sizeof(MetaData_);
        constexpr uint8_t offset = (used & 0x1) ? 0 : 1;
        meta_.bytes_used_ = used + offset;
    }

    static MemoryBucket<SHIFT_BITS>** memoryBucketPointerFromPointer(void* ptr) {
        return memoryBucketPointerFromPointer(reinterpret_cast<uintptr_t>(ptr));
    }

    static MemoryBucket<SHIFT_BITS>** memoryBucketPointerFromPointer(uintptr_t ptr) {
        // TODO check even ptr problems.
        if ((ptr & 1) == 0) {
            return reinterpret_cast<MemoryBucket<SHIFT_BITS>**>((ptr & ~BUCKET_MASK) + BUCKET_SIZE - sizeof(MetaData_));
        } else {
            return reinterpret_cast<MemoryBucket<SHIFT_BITS>**>((ptr & ~BUCKET_MASK) + sizeof(MetaData_));
        }
    }

    // This function is designed to create a new bucket, but there's a rare case
    // where two threads may compete for eachother in creating a new bucket, causing both
    // to succeed and both threads may end up using the same bucket causing a memory leak.
    // This function should be protected against that edge case using locks.
    static void tryNewBucket_(size_t lastRequestSize) {
        ::std::lock_guard<::std::mutex> lock(newAllocatorMux_);
        // We do another check here because between this function and the call point there may already
        // be a new bucket assigned. This will ensure we do not create a new bucket in such case.
fprintf(stderr, "D: %d\n", __LINE__);
        if (tipBucket_->meta_.bytes_used_.load() + lastRequestSize < BUCKET_SIZE)
            return;
fprintf(stderr, "D: %d\n", __LINE__);
        MemoryBucket<SHIFT_BITS>* previousTipBucket = tipBucket_;
fprintf(stderr, "D: %d\n", __LINE__);
        tipBucket_ = new MemoryBucket<SHIFT_BITS>;
fprintf(stderr, "NewTip: %p\n", tipBucket_);
fprintf(stderr, "D: %d\n", __LINE__);
        // We do this here because it needs to be increased by 1 before we get into this function
        // for speed and race condition reasons.
        if (previousTipBucket->meta_.used_count_.fetch_sub(1) - 1 == 0) {
            delete previousTipBucket;
        }
    }

    struct MetaData_ {
        MetaData_(const uintptr_t begin_ptr)
            : begin_ptr_(begin_ptr) { }
        uintptr_t begin_ptr_;
        ::std::atomic<int32_t> bytes_used_ = ATOMIC_VAR_INIT(0);
        ::std::atomic<int32_t> used_count_ = ATOMIC_VAR_INIT(0);
        bool is_over_fence_ = false;
        ::std::mutex over_fence_mux_;
        ::std::function<void()> deallocCallbackForTest;
    };

// Needed for test, can be removed if never going to write/use tests for this.
//#ifdef UNITTEST
    friend class MemoryAllocatorTest;
    ~MemoryBucket() {
fprintf(stderr, "Dealloc %p\n", this);
        if (meta_.deallocCallbackForTest) {
            meta_.deallocCallbackForTest();
        }
    }
//#endif

    union {
        uint8_t data_[BUCKET_SIZE];
        MetaData_ meta_;
    };

    static MemoryBucket<SHIFT_BITS>* tipBucket_;
    static ::std::mutex newAllocatorMux_;

};

template <int SHIFT_BITS>
MemoryBucket<SHIFT_BITS>* MemoryBucket<SHIFT_BITS>::tipBucket_;
template <int SHIFT_BITS>
::std::mutex MemoryBucket<SHIFT_BITS>::newAllocatorMux_;

template <typename T>
struct FastAllocator {
    typedef T               value_type;
    typedef T*              pointer;
    typedef T&              reference;
    typedef const T*        const_pointer;
    typedef const T&        const_reference;
    typedef size_t          size_type;

    FastAllocator() { }
    explicit FastAllocator(const FastAllocator& other)
        : size_(other.size_) { }
    explicit FastAllocator(FastAllocator&& other)
        : size_(other.size_) { }
    template <class U>
    FastAllocator(const FastAllocator<U>& other)
        : size_(other.max_size()) { }

    template <class U, class... Args>
    void construct(U* p, Args&&... args) {
        new(p) U (::std::forward<Args>(args)...);
    }

    void destroy(T* p) {
        p->~T();
    }

    size_type max_size() const noexcept {
        return size_;
    }

    size_type max_size(T* p) const noexcept {
        return size_;
    }

    T* allocate(size_type n, const_pointer = 0) {
        size_ = n;
        return MemoryBucket<_ALLOCATOR_DEFAULT_BITS>::nextPtr<T>(n * sizeof(T));
    }

    void deallocate(T* p, size_type size_t) {
        size_ = 0;
        return MemoryBucket<_ALLOCATOR_DEFAULT_BITS>::freePtr<T>(p);
    }

private:
    size_t size_;

};

#define FAST_ALLOCATE(TYPE) FAST_ALLOCATE_WITH_SIZE(TYPE, _ALLOCATOR_DEFAULT_BITS)
#define FAST_ALLOCATE_WITH_SIZE(TYPE, BUCKET_SIZE) \
    public: \
        void* operator new(size_t sz, void*) { \
            return MemoryBucket<BUCKET_SIZE>::nextPtr<TYPE>(sz); \
        } \
        void* operator new[](size_t sz, void*) { \
            return MemoryBucket<BUCKET_SIZE>::nextPtr<TYPE>(sz); \
        } \
        void* operator new(size_t sz) \
        { \
            return MemoryBucket<BUCKET_SIZE>::nextPtr<TYPE>(sz); \
        } \
        void operator delete(void* p) \
        { \
            MemoryBucket<BUCKET_SIZE>::freePtr<TYPE>(p); \
        } \
    private:

#endif /* MemoryAllocator_h */