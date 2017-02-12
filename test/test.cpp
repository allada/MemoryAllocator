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

class TestAllocation2Bytes {
	FAST_ALLOCATE_WITH_SIZE(TestAllocation2Bytes, BITS)
public:
	static constexpr int BITS = 8;
	volatile bool data_[2];
};

TEST_F(MemoryAllocatorTest, SmallBoundaryMacroCheck) {
	typedef MemoryBucket<TestAllocation2Bytes::BITS> TestBucket;
	constexpr int totalBytes = 1 << TestAllocation2Bytes::BITS;
	ASSERT_EQ(sizeof(TestBucket), totalBytes);
	constexpr int metaDataSize = sizeof(TestBucket::MetaData_) + ((sizeof(TestBucket::MetaData_) & 0x1) ? 0 : 1);
	constexpr int bytesAvailable = totalBytes - metaDataSize;
	// At 3x the test size so we can artifially align our bucket and if we overflow we don't crash. 
	auto heapPtr = new ::std::array<char, sizeof(TestBucket) * 3>;
	// We subtract 1 to ensure if we are already aligned we give enough of an end buffer.
	// We then modulious it down then add 0x200. Since we are alligned by 0x100 now we need to ensure
	// we have plenty of padding on each side so we go up by 0x200 because we are now garanteed to have
	// our startig point be at least 0x50 bytes right and left padded.
	uintptr_t ptr = ((reinterpret_cast<uintptr_t>(heapPtr->data()) - (totalBytes / 2)) & ~0xFF);
	TestBucket::tipBucket_ = new(reinterpret_cast<void*>(ptr)) TestBucket;
	ASSERT_EQ(reinterpret_cast<uintptr_t>(&TestBucket::tipBucket_->meta_), reinterpret_cast<uintptr_t>(&TestBucket::tipBucket_->data_));
	ASSERT_EQ(reinterpret_cast<uintptr_t>(TestBucket::tipBucket_), reinterpret_cast<uintptr_t>(&TestBucket::tipBucket_->data_));
	auto firstBucket = TestBucket::tipBucket_;
	constexpr int numberOfObjects = bytesAvailable / sizeof(TestAllocation2Bytes);
	std::array<TestAllocation2Bytes*, numberOfObjects> tmpData;

	for (auto i = 0; i < numberOfObjects; i++) {
		tmpData[i] = new TestAllocation2Bytes;
		const uintptr_t firstBucketPtr = reinterpret_cast<uintptr_t>(&TestBucket::tipBucket_->data_);
		const uintptr_t metaDataPtr = reinterpret_cast<uintptr_t>(&TestBucket::tipBucket_->meta_);
		ASSERT_EQ(firstBucketPtr, metaDataPtr);
		const uintptr_t tmpDataPtr = reinterpret_cast<uintptr_t>(tmpData[i]);
		// Last item gets a new bucket.
		//if (i < numberOfObjects) {
			ASSERT_EQ(firstBucket, TestBucket::tipBucket_) << "i = " << i << " of " << numberOfObjects - 1;
		//}
	//fprintf(stderr, "firstBucketPtr: %lu metaDataSize: %d\n", firstBucketPtr, metaDataSize);
		ASSERT_EQ(firstBucketPtr + metaDataSize + (i * 2), tmpDataPtr) << "metaDataSize = " << metaDataSize << "\ni = " << i;
// fprintf(stderr, "allocPtr: %p\n", tmpData[i]);
	}

	auto someData = new TestAllocation2Bytes;
	auto newBucket = TestBucket::tipBucket_;
fprintf(stderr, "newPtr: %p\n", someData);
// fprintf(stderr, "firstBucket: %p\n", firstBucket);
	ASSERT_NE(firstBucket, newBucket);
fprintf(stderr, "L: %d\n", __LINE__);
	delete someData;
fprintf(stderr, "L: %d\n", __LINE__);
	ASSERT_EQ(newBucket, TestBucket::tipBucket_);

	for (auto i = 0; i < numberOfObjects - 1; i++) {
		delete tmpData[i];
		ASSERT_EQ(newBucket, TestBucket::tipBucket_);
	}

	bool firstBucketWasDeallocated = false;
	firstBucket->meta_.deallocCallbackForTest = [&firstBucketWasDeallocated](){ firstBucketWasDeallocated = true; };
	ASSERT_EQ(firstBucketWasDeallocated, false);
	delete tmpData[numberOfObjects-1];
	ASSERT_EQ(firstBucketWasDeallocated, true);

	free(heapPtr);
	free(TestBucket::tipBucket_);
	TestBucket::tipBucket_ = nullptr;
}

}
