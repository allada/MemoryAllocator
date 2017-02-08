#include "MemoryAllocator.h"

template <>
MemoryBucket<_ALLOCATOR_DEFAULT_BITS>* MemoryBucket<_ALLOCATOR_DEFAULT_BITS>::tipBucket_ = new MemoryBucket<_ALLOCATOR_DEFAULT_BITS>;
template <>
std::mutex MemoryBucket<_ALLOCATOR_DEFAULT_BITS>::newAllocatorMux_;
