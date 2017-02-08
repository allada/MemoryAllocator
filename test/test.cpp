#include "gtest/gtest.h"

#include <array>

#define UNITTEST

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

namespace {

class MemoryAllocatorTest : public ::testing::Test {
};

#include "../MemoryAllocator.h"

constexpr int BITS = 8;
class TestAllocation2Bytes {
	FAST_ALLOCATE_WITH_SIZE(TestAllocation2Bytes, BITS)
private:
	volatile bool data_;

};

TEST_F(MemoryAllocatorTest, SmallBoundaryMacroCheck) {
	constexpr int totalBytes = 1 << BITS;
	constexpr int bytesAvailable = totalBytes - sizeof(MemoryBucket<BITS>::MetaData_);
fprintf(stderr, "total: %d, avail: %d, sizeof: %lu\n", totalBytes, bytesAvailable, sizeof(MemoryBucket<BITS>::MetaData_));
	// At 3x the test size so we can artifially align our bucket and if we overflow we don't crash. 
	auto heapPtr = new ::std::array<char, bytesAvailable * 3>;
	// We subtract 1 to ensure if we are already aligned we give enough of an end buffer.
	// We then modulious it down then add 0x200. Since we are alligned by 0x100 now we need to ensure
	// we have plenty of padding on each side so we go up by 0x200 because we are now garanteed to have
	// our startig point be at least 0x50 bytes right and left padded.
	uintptr_t ptr = ((reinterpret_cast<uintptr_t>(heapPtr->data()) - (totalBytes / 2)) & ~0xFF) + (totalBytes * 2);
fprintf(stderr, "ptr: %lx\n", ptr);
	MemoryBucket<BITS>::tipBucket_ = new(reinterpret_cast<void*>(ptr)) MemoryBucket<BITS>;
	auto firstBucket = MemoryBucket<BITS>::tipBucket_;
	constexpr int numberOfObjects = bytesAvailable / sizeof(TestAllocation2Bytes);
	std::array<TestAllocation2Bytes*, numberOfObjects> tmpData;

	for (auto i = 0; i < numberOfObjects; i++) {
		tmpData[i] = new TestAllocation2Bytes;
fprintf(stderr, "%p\n", tmpData[i]);
		ASSERT_EQ(firstBucket, MemoryBucket<BITS>::tipBucket_) << "at " << i;
	}

	auto someData = new TestAllocation2Bytes;
	auto newBucket = MemoryBucket<BITS>::tipBucket_;
	ASSERT_NE(firstBucket, newBucket);
	delete someData;
	ASSERT_EQ(newBucket, MemoryBucket<BITS>::tipBucket_);

	for (auto i = 0; i < numberOfObjects - 1; i++) {
		delete tmpData[i];
		ASSERT_EQ(newBucket, MemoryBucket<BITS>::tipBucket_);
	}

	bool firstBucketWasDeallocated = false;
	firstBucket->deallocCallbackForTest = [&firstBucketWasDeallocated](){ firstBucketWasDeallocated = true; };
	ASSERT_EQ(firstBucketWasDeallocated, false);
	delete tmpData[numberOfObjects-1];
	ASSERT_EQ(firstBucketWasDeallocated, true);

	free(heapPtr);
	free(MemoryBucket<BITS>::tipBucket_);
}

}
