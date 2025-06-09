#include "jacl/small_vector.hh"

#include <gtest/gtest.h>

#include <list>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

class AllocationStats {
public:
  static std::size_t allocation_count() { return allocation_count_; }
  static std::size_t deallocation_count() { return deallocation_count_; }
  static std::size_t total_allocated() { return total_allocated_; }
  static std::size_t total_deallocated() { return total_deallocated_; }
  static std::size_t outstanding_allocations() { return allocations_.size(); }

  static void reset_counters() {
    allocation_count_   = 0;
    deallocation_count_ = 0;
    total_allocated_    = 0;
    total_deallocated_  = 0;
    allocations_.clear();
  }

protected:
  static void record_allocation(void* ptr, std::size_t n) {
    allocation_count_++;
    total_allocated_  += n;
    allocations_[ptr]  = n;
  }

  static void record_deallocation(void* ptr, std::size_t n) {
    deallocation_count_++;
    total_deallocated_ += n;
    auto it             = allocations_.find(ptr);
    if(it != allocations_.end()) { allocations_.erase(it); }
  }

private:
  static size_t allocation_count_;
  static size_t deallocation_count_;
  static size_t total_allocated_;
  static size_t total_deallocated_;
  static std::unordered_map<void*, std::size_t> allocations_;
}; // class AllocationStats

// C++11 compatible static member definitions
size_t AllocationStats::allocation_count_   = 0;
size_t AllocationStats::deallocation_count_ = 0;
size_t AllocationStats::total_allocated_    = 0;
size_t AllocationStats::total_deallocated_  = 0;
std::unordered_map<void*, std::size_t> AllocationStats::allocations_;

class StatefulPolicy {
public:
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap            = std::true_type;

  StatefulPolicy() : id_{next_id_++} {}

  StatefulPolicy(const StatefulPolicy&) noexcept : id_{next_id_++} {}

  StatefulPolicy& operator=(const StatefulPolicy&) noexcept {
    id_ = next_id_++;
    return *this;
  }

  StatefulPolicy(StatefulPolicy&& other) noexcept : id_{std::exchange(other.id_, next_id_++)} {}

  StatefulPolicy& operator=(StatefulPolicy&& other) noexcept {
    id_ = std::exchange(other.id_, next_id_++);
    return *this;
  }

  bool operator==(const StatefulPolicy& other) const noexcept { return id_ == other.id_; }
  bool operator!=(const StatefulPolicy& other) const noexcept { return id_ != other.id_; }

  std::size_t get_id() const noexcept { return id_; }

private:
  size_t id_{};
  static size_t next_id_;
}; // class StatefulPolicy

size_t StatefulPolicy::next_id_ = 1;

class NonstatefulPolicy {
public:
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap            = std::true_type;

  bool operator==(const NonstatefulPolicy&) const noexcept { return true; }
  bool operator!=(const NonstatefulPolicy&) const noexcept { return false; }

}; // class NonstatefulPolicy

template <typename T, typename policyT>
class MockAllocator : public AllocationStats, public policyT {
public:
  using value_type      = T;
  using size_type       = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_copy_assignment =
      typename policyT::propagate_on_container_copy_assignment;
  using propagate_on_container_move_assignment =
      typename policyT::propagate_on_container_move_assignment;
  using propagate_on_container_swap = typename policyT::propagate_on_container_swap;

  T* allocate(size_type n) {
    T* ptr = static_cast<T*>(std::malloc(n * sizeof(T)));
    if(!ptr) throw std::bad_alloc();
    record_allocation(ptr, n * sizeof(T));
    return ptr;
  }

  void deallocate(T* ptr, size_type n) {
    record_deallocation(ptr, n * sizeof(T));
    std::free(ptr);
  }

private:
  template <typename U>
  friend class InstrumentedAllocator;
}; // class InstrumentedAllocator

using alloc_stateful_int_t    = MockAllocator<int, StatefulPolicy>;
using alloc_nonstateful_int_t = MockAllocator<int, NonstatefulPolicy>;

class SmallVectorTest : public ::testing::Test {
protected:
  template <typename T, std::size_t N, typename allocT>
  void fill_with_random_data(jacl::small_vector<T, N, allocT>& vec, size_t count) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<T> dis(1, 100);

    vec.clear();
    for(std::size_t i = 0; i < count; ++i) { vec.push_back(dis(gen)); }
  }

  void SetUp() override { AllocationStats::reset_counters(); }
}; // class SmallVectorTest

TEST_F(SmallVectorTest, DefaultConstructor) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec;
  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_TRUE(vec.empty());

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, AllocatorConstructor) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec(alloc_nonstateful_int_t{});
  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_TRUE(vec.empty());

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, InitializerListConstructorWithStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{1, 2, 3};
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_FALSE(vec.empty());
  EXPECT_EQ(vec[0], 1);
  EXPECT_EQ(vec[1], 2);
  EXPECT_EQ(vec[2], 3);

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, InitializerListConstructorWithStaticMemoryAndAllocator) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{
      {1, 2, 3},
      alloc_nonstateful_int_t{}
  };
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_FALSE(vec.empty());
  EXPECT_EQ(vec[0], 1);
  EXPECT_EQ(vec[1], 2);
  EXPECT_EQ(vec[2], 3);

  // Ensure that the allocator was not used for small vector with static memory.
  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, InitializerListConstructorWithDynamicMemoryAndAllocator) {
  {
    jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{
        {1, 2, 3, 4, 5, 6},
        alloc_nonstateful_int_t{}
    };
    EXPECT_EQ(vec.size(), 6);
    EXPECT_GT(vec.capacity(), 4);
    EXPECT_FALSE(vec.empty());
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec[2], 3);
    EXPECT_EQ(vec[3], 4);
    EXPECT_EQ(vec[4], 5);
    EXPECT_EQ(vec[5], 6);

    // Ensure that the allocator was used for small vector with dynamic memory.
    EXPECT_EQ(AllocationStats::allocation_count(), 1);
    EXPECT_EQ(AllocationStats::deallocation_count(), 0);
    EXPECT_GE(AllocationStats::total_allocated(), 6 * sizeof(int));
    EXPECT_EQ(AllocationStats::total_deallocated(), 0);
    EXPECT_GE(AllocationStats::outstanding_allocations(), 1);
  }

  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_EQ(AllocationStats::deallocation_count(), 1);
  EXPECT_GE(AllocationStats::total_allocated(), 6 * sizeof(int));
  EXPECT_GE(AllocationStats::total_deallocated(), 6 * sizeof(int));
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, CopyConstructorWithStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> original;
  fill_with_random_data(original, 3);
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> copy(original);
  EXPECT_EQ(copy.size(), 3);
  EXPECT_EQ(copy.capacity(), 4);
  EXPECT_EQ(copy.get_allocator(), original.get_allocator());
  for(std::size_t i = 0; i < original.size(); ++i) { EXPECT_EQ(copy[i], original[i]); }

  // If allocator propagation is not enabled, the allocators should be the same
  EXPECT_EQ(copy.get_allocator(), original.get_allocator());
}

TEST_F(SmallVectorTest, CopyConstructorWithDynamicMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> original;
  fill_with_random_data(original, 6);
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> copy(original);
  EXPECT_EQ(copy.size(), 6);
  EXPECT_GT(copy.capacity(), 4);
  EXPECT_EQ(copy.get_allocator(), original.get_allocator());
  for(std::size_t i = 0; i < original.size(); ++i) { EXPECT_EQ(copy[i], original[i]); }

  // If allocator propagation is not enabled, the allocators should be the same
  EXPECT_EQ(copy.get_allocator(), original.get_allocator());
}

TEST_F(SmallVectorTest, CopyConstructorWithStaticMemoryAndStatefulAllocator) {
  jacl::small_vector<int, 4, alloc_stateful_int_t> original{};
  fill_with_random_data(original, 3);

  jacl::small_vector<int, 4, alloc_stateful_int_t> copy(original);
  EXPECT_EQ(copy.size(), 3);
  EXPECT_EQ(copy.capacity(), 4);
  EXPECT_NE(copy.get_allocator(), original.get_allocator());
  for(std::size_t i = 0; i < original.size(); ++i) { EXPECT_EQ(copy[i], original[i]); }

  // No allocations should occur for static memory
  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, CopyConstructorWithDynamicMemoryAndStatefulAllocator) {
  {
    jacl::small_vector<int, 4, alloc_stateful_int_t> original{};
    for(int i = 1; i <= 6; ++i) { original.push_back(i); }

    // Two allocations should occur: one for original, one for copy
    EXPECT_EQ(AllocationStats::allocation_count(), 1);
    EXPECT_GE(AllocationStats::total_allocated(), 6 * sizeof(int));
    EXPECT_EQ(AllocationStats::deallocation_count(), 0);
    EXPECT_EQ(AllocationStats::total_deallocated(), 0);
    EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

    jacl::small_vector<int, 4, alloc_stateful_int_t> copy(original);
    EXPECT_EQ(copy.size(), 6);
    EXPECT_GT(copy.capacity(), 4);
    EXPECT_NE(copy.get_allocator(), original.get_allocator());
    for(std::size_t i = 0; i < original.size(); ++i) { EXPECT_EQ(copy[i], original[i]); }

    // New allocation for copy
    EXPECT_EQ(AllocationStats::allocation_count(), 2);
    EXPECT_GE(AllocationStats::total_allocated(), 2 * 6 * sizeof(int));
    EXPECT_EQ(AllocationStats::deallocation_count(), 0);
    EXPECT_EQ(AllocationStats::total_deallocated(), 0);
    EXPECT_EQ(AllocationStats::outstanding_allocations(), 2);
  }

  // Both vectors destroyed, both allocations deallocated
  EXPECT_EQ(AllocationStats::allocation_count(), 2);
  EXPECT_GE(AllocationStats::total_allocated(), 2 * 6 * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 2);
  EXPECT_GE(AllocationStats::total_deallocated(), 2 * 6 * sizeof(int));
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, MoveConstructorWithStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> original;
  fill_with_random_data(original, 3);
  std::vector<int> original_copy(original.begin(), original.end());
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> moved(std::move(original));
  EXPECT_EQ(moved.size(), 3);
  EXPECT_EQ(moved.capacity(), 4);
  EXPECT_TRUE(original.empty());
  EXPECT_EQ(moved.get_allocator(), original.get_allocator());
  for(std::size_t i = 0; i < moved.size(); ++i) { EXPECT_EQ(moved[i], original_copy[i]); }

  // No allocations should occur for static memory
  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, MoveConstructorWithDynamicMemory) {
  {
    jacl::small_vector<int, 4, alloc_nonstateful_int_t> original;
    fill_with_random_data(original, 6);

    // Allocations should occur for dynamic memory
    EXPECT_EQ(AllocationStats::allocation_count(), 1);
    EXPECT_GE(AllocationStats::total_allocated(), 6 * sizeof(int));
    EXPECT_EQ(AllocationStats::deallocation_count(), 0);
    EXPECT_EQ(AllocationStats::total_deallocated(), 0);
    EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

    std::vector<int> original_copy(original.begin(), original.end());
    jacl::small_vector<int, 4, alloc_nonstateful_int_t> moved(std::move(original));
    EXPECT_EQ(moved.size(), 6);
    EXPECT_GT(moved.capacity(), 4);
    EXPECT_TRUE(original.empty());
    EXPECT_EQ(moved.get_allocator(), original.get_allocator());
    for(std::size_t i = 0; i < moved.size(); ++i) { EXPECT_EQ(moved[i], original_copy[i]); }

    // No change in allocation stats after move
    EXPECT_EQ(AllocationStats::allocation_count(), 1);
    EXPECT_GE(AllocationStats::total_allocated(), 6 * sizeof(int));
    EXPECT_EQ(AllocationStats::deallocation_count(), 0);
    EXPECT_EQ(AllocationStats::total_deallocated(), 0);
    EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
  }

  // Memory should be deallocated when destoryed
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_GE(AllocationStats::total_allocated(), 6 * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 1);
  EXPECT_GE(AllocationStats::total_deallocated(), 6 * sizeof(int));
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, ValueConstructorWithStaticMemory) {
  jacl::small_vector<int, 4> vec(4ull, 42);
  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_FALSE(vec.empty());
  for(const auto& elem : vec) { EXPECT_EQ(elem, 42); }
}

TEST_F(SmallVectorTest, ValueConstructorWithDynamicMemory) {
  jacl::small_vector<int, 4> vec(6ull, 42);
  EXPECT_EQ(vec.size(), 6);
  EXPECT_GT(vec.capacity(), 4);
  EXPECT_FALSE(vec.empty());
  for(const auto& elem : vec) { EXPECT_EQ(elem, 42); }
}

TEST_F(SmallVectorTest, ValueConstructorWithStaticMemoryAndAllocator) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec(4ull, 42, alloc_nonstateful_int_t{});
  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_FALSE(vec.empty());
  for(const auto& elem : vec) { EXPECT_EQ(elem, 42); }

  // No allocations should occur for small vector with static memory
  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, IteratorConstructorWithStaticMemory) {
  std::vector<int> source{10, 20, 30};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec(source.begin(), source.end());
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_FALSE(vec.empty());
  EXPECT_EQ(vec[0], 10);
  EXPECT_EQ(vec[1], 20);
  EXPECT_EQ(vec[2], 30);

  // No allocations should occur for small vector with static memory
  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, IteratorConstructorWithDynamicMemory) {
  std::vector<int> source{1, 2, 3, 4, 5, 6, 7};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec(source.begin(), source.end());

  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_GE(AllocationStats::total_allocated(), 7 * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

  EXPECT_EQ(vec.size(), 7);
  EXPECT_GE(vec.capacity(), 7);
  EXPECT_FALSE(vec.empty());
  for(std::size_t i = 0; i < source.size(); ++i) { EXPECT_EQ(vec[i], source[i]); }
}

TEST_F(SmallVectorTest, IteratorConstructorWithEmptyRange) {
  std::vector<int> source;
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec(source.begin(), source.end());

  // No allocations should occur for small vector with static memory
  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);

  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_TRUE(vec.empty());
}

TEST_F(SmallVectorTest, IteratorConstructorWithArrayPointers) {
  int arr[] = {100, 200, 300, 400, 500};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec(arr, arr + 5);

  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_GE(AllocationStats::total_allocated(), 7 * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

  EXPECT_EQ(vec.size(), 5);
  EXPECT_GE(vec.capacity(), 5);
  EXPECT_FALSE(vec.empty());
  for(std::size_t i = 0; i < 5; ++i) { EXPECT_EQ(vec[i], arr[i]); }
}

TEST_F(SmallVectorTest, BidiIteratorConstructorWithArrayPointers) {
  std::list<int> list{100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec(list.begin(), list.end());

  EXPECT_GT(AllocationStats::allocation_count(), 1);
  EXPECT_GE(AllocationStats::total_allocated(), 10 * sizeof(int));
  EXPECT_GT(AllocationStats::deallocation_count(), 0);
  EXPECT_GT(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

  EXPECT_EQ(vec.size(), 10);
  EXPECT_GE(vec.capacity(), 10);
  EXPECT_FALSE(vec.empty());
  std::size_t i = 0;
  for(auto it = list.begin(); it != list.end(); ++it, ++i) { EXPECT_EQ(vec[i], *it); }
}
