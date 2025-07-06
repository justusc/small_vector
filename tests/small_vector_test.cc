#include "jacl/small_vector.hh"

#include <cstddef>
#include <gtest/gtest.h>

#include <list>
#include <memory>
#include <random>
#include <stdexcept>
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

using alloc_stateful_int_t        = MockAllocator<int, StatefulPolicy>;
using alloc_nonstateful_int_t     = MockAllocator<int, NonstatefulPolicy>;
using alloc_nonstateful_int_ptr_t = MockAllocator<std::unique_ptr<int>, NonstatefulPolicy>;

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

  void TearDown() override {
    // Ensure no memory leaks
    EXPECT_EQ(AllocationStats::allocation_count(), AllocationStats::deallocation_count());
    EXPECT_EQ(AllocationStats::total_allocated(), AllocationStats::total_deallocated());
    EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
  }
}; // class SmallVectorTest

TEST_F(SmallVectorTest, DefaultConstructor) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec;
  EXPECT_NE(vec.data(), nullptr);
  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_TRUE(vec.empty());
  EXPECT_THROW(vec.at(0), std::out_of_range);
  EXPECT_EQ(vec.static_capacity, 4);

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
}

TEST_F(SmallVectorTest, AllocatorConstructor) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec(alloc_nonstateful_int_t{});
  EXPECT_NE(vec.data(), nullptr);
  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_TRUE(vec.empty());
  EXPECT_THROW(vec.at(0), std::out_of_range);

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
}

TEST_F(SmallVectorTest, InitializerListConstructorWithStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{1, 2, 3};
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_FALSE(vec.empty());
  for(size_t i = 0; i < vec.size(); ++i) {
    EXPECT_EQ(vec[i], i + 1);
    EXPECT_EQ(vec.at(i), i + 1);
  }
  EXPECT_EQ(vec.data(), &vec[0]);

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
}

TEST_F(SmallVectorTest, InitializerListConstructorWithStaticMemoryAndAllocator) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{
      {1, 2, 3},
      alloc_nonstateful_int_t{}
  };
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_FALSE(vec.empty());
  for(size_t i = 0; i < vec.size(); ++i) {
    EXPECT_EQ(vec[i], i + 1);
    EXPECT_EQ(vec.at(i), i + 1);
  }
  EXPECT_THROW(vec.at(10), std::out_of_range);
  EXPECT_EQ(vec.data(), &vec[0]);

  // Ensure that the allocator was not used for small vector with static memory.
  EXPECT_EQ(AllocationStats::allocation_count(), 0);
}

TEST_F(SmallVectorTest, InitializerListConstructorWithDynamicMemoryAndAllocator) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{
      {1, 2, 3, 4, 5, 6},
      alloc_nonstateful_int_t{}
  };
  EXPECT_EQ(vec.size(), 6);
  EXPECT_GT(vec.capacity(), 4);
  EXPECT_FALSE(vec.empty());
  for(size_t i = 0; i < vec.size(); ++i) {
    EXPECT_EQ(vec[i], i + 1);
    EXPECT_EQ(vec.at(i), i + 1);
  }
  EXPECT_THROW(vec.at(vec.size()), std::out_of_range);
  EXPECT_EQ(vec.data(), &vec[0]);

  // Ensure that the allocator was used for small vector with dynamic memory.
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_GE(AllocationStats::total_allocated(), 6 * sizeof(int));
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_GE(AllocationStats::outstanding_allocations(), 1);
}

TEST_F(SmallVectorTest, CopyConstructorWithStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> original;
  fill_with_random_data(original, 3);
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> copy(original);
  EXPECT_NE(copy.data(), original.data());
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
  auto original_data = original.data();

  jacl::small_vector<int, 4, alloc_nonstateful_int_t> copy(original);
  EXPECT_EQ(original.data(), original_data);
  EXPECT_NE(copy.data(), original_data);
  EXPECT_NE(copy.data(), original.data());
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
  auto original_data = original.data();

  jacl::small_vector<int, 4, alloc_stateful_int_t> copy(original);
  EXPECT_EQ(original.data(), original_data);
  EXPECT_NE(copy.data(), original_data);
  EXPECT_EQ(copy.size(), 3);
  EXPECT_EQ(copy.capacity(), 4);
  EXPECT_NE(copy.get_allocator(), original.get_allocator());
  for(std::size_t i = 0; i < original.size(); ++i) { EXPECT_EQ(copy[i], original[i]); }

  // No allocations should occur for static memory
  EXPECT_EQ(AllocationStats::allocation_count(), 0);
}

TEST_F(SmallVectorTest, CopyConstructorWithDynamicMemoryAndStatefulAllocator) {
  jacl::small_vector<int, 4, alloc_stateful_int_t> original{};
  for(int i = 1; i <= 6; ++i) { original.push_back(i); }
  auto original_data = original.data();

  // Two allocations should occur: one for original, one for copy
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_GE(AllocationStats::total_allocated(), 6 * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

  jacl::small_vector<int, 4, alloc_stateful_int_t> copy(original);
  EXPECT_EQ(original.data(), original_data);
  EXPECT_NE(copy.data(), original_data);
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

TEST_F(SmallVectorTest, MoveConstructorWithStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> original;
  fill_with_random_data(original, 3);
  auto original_data = original.data();

  std::vector<int> original_copy(original.begin(), original.end());
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> moved(std::move(original));
  EXPECT_EQ(original.data(), original_data);
  EXPECT_NE(moved.data(), original_data);
  EXPECT_EQ(moved.size(), 3);
  EXPECT_EQ(moved.capacity(), 4);
  EXPECT_TRUE(original.empty());
  EXPECT_EQ(moved.get_allocator(), original.get_allocator());
  for(std::size_t i = 0; i < moved.size(); ++i) { EXPECT_EQ(moved[i], original_copy[i]); }

  // No allocations should occur for static memory
  EXPECT_EQ(AllocationStats::allocation_count(), 0);
}

TEST_F(SmallVectorTest, MoveConstructorWithDynamicMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> original;
  auto original_static_data = original.data();
  fill_with_random_data(original, 6);
  auto original_data = original.data();
  EXPECT_NE(original_data, original_static_data);

  // Allocations should occur for dynamic memory
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_GE(AllocationStats::total_allocated(), 6 * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

  std::vector<int> original_copy(original.begin(), original.end());
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> moved(std::move(original));
  EXPECT_EQ(original.data(), original_static_data);
  EXPECT_EQ(moved.data(), original_data);
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

TEST_F(SmallVectorTest, MoveConstructorWithStaticMemoryAndStatefulAllocator) {
  jacl::small_vector<int, 4, alloc_stateful_int_t> original{};
  fill_with_random_data(original, 3);
  auto original_allocator_id = original.get_allocator().get_id();
  std::vector<int> original_copy(original.begin(), original.end());

  jacl::small_vector<int, 4, alloc_stateful_int_t> moved(std::move(original));
  EXPECT_EQ(moved.size(), 3);
  EXPECT_EQ(moved.capacity(), 4);
  EXPECT_TRUE(original.empty());
  EXPECT_EQ(moved.get_allocator().get_id(), original_allocator_id);
  EXPECT_NE(moved.get_allocator(), original.get_allocator());
  for(std::size_t i = 0; i < moved.size(); ++i) { EXPECT_EQ(moved[i], original_copy[i]); }

  // No allocations should occur for static memory
  EXPECT_EQ(AllocationStats::allocation_count(), 0);
}

TEST_F(SmallVectorTest, MoveConstructorWithDynamicMemoryAndStatefulAllocator) {
  jacl::small_vector<int, 4, alloc_stateful_int_t> original{};
  fill_with_random_data(original, 6);
  auto original_allocator_id = original.get_allocator().get_id();

  // Allocations should occur for dynamic memory
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_GE(AllocationStats::total_allocated(), 6 * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

  std::vector<int> original_copy(original.begin(), original.end());
  jacl::small_vector<int, 4, alloc_stateful_int_t> moved(std::move(original));
  EXPECT_EQ(moved.size(), 6);
  EXPECT_GT(moved.capacity(), 4);
  EXPECT_TRUE(original.empty());
  EXPECT_EQ(moved.get_allocator().get_id(), original_allocator_id);
  EXPECT_NE(moved.get_allocator(), original.get_allocator());
  for(std::size_t i = 0; i < moved.size(); ++i) { EXPECT_EQ(moved[i], original_copy[i]); }

  // No change in allocation stats after move - memory ownership transferred
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_GE(AllocationStats::total_allocated(), 6 * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
}

TEST_F(SmallVectorTest, ValueConstructorWithStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec(4ull, 42);
  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_FALSE(vec.empty());
  for(const auto& elem : vec) { EXPECT_EQ(elem, 42); }

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
}

TEST_F(SmallVectorTest, ValueConstructorWithDynamicMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec(12ull, 42);
  EXPECT_EQ(vec.size(), 12);
  EXPECT_GE(vec.capacity(), 12);
  EXPECT_FALSE(vec.empty());
  for(const auto& elem : vec) { EXPECT_EQ(elem, 42); }

  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_GE(AllocationStats::total_allocated(), 12 * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
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
  int arr[] = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec(arr, arr + 10);

  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_GE(AllocationStats::total_allocated(), 10 * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

  EXPECT_EQ(vec.size(), 10);
  EXPECT_GE(vec.capacity(), 10);
  EXPECT_FALSE(vec.empty());
  for(std::size_t i = 0; i < 10; ++i) { EXPECT_EQ(vec[i], arr[i]); }
}

TEST_F(SmallVectorTest, BidiIteratorConstructorWithArrayPointers) {
  // Compute expected allocation count, capacity, and allocated size
  // for the expected growth rate of the small vector.
  // Growth rate is (capacity += capacity/2 + 1)
  constexpr size_t static_capacity = 4;
  size_t expected_allocation_count = 0;
  size_t expected_capacity         = static_capacity;
  size_t expected_allocated        = 0;
  while(expected_capacity < 10) {
    ++expected_allocation_count;
    expected_capacity  += expected_capacity / 2 + 1;
    expected_allocated += expected_capacity * sizeof(int);
  }

  {
    std::list<int> list{100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
    jacl::small_vector<int, static_capacity, alloc_nonstateful_int_t> vec(list.begin(), list.end());

    EXPECT_EQ(AllocationStats::allocation_count(), expected_allocation_count);
    EXPECT_EQ(AllocationStats::total_allocated(), expected_allocated);
    EXPECT_EQ(AllocationStats::deallocation_count(), expected_allocation_count - 1);
    EXPECT_EQ(
        AllocationStats::total_deallocated(), expected_allocated - expected_capacity * sizeof(int));
    EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

    EXPECT_EQ(vec.size(), 10);
    EXPECT_GE(vec.capacity(), 10);
    EXPECT_FALSE(vec.empty());
    std::size_t i = 0;
    for(auto it = list.begin(); it != list.end(); ++it, ++i) { EXPECT_EQ(vec[i], *it); }
  }

  EXPECT_EQ(AllocationStats::allocation_count(), expected_allocation_count);
  EXPECT_EQ(AllocationStats::total_allocated(), expected_allocated);
  EXPECT_EQ(AllocationStats::deallocation_count(), expected_allocation_count);
  EXPECT_EQ(AllocationStats::total_deallocated(), expected_allocated);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, InitializerListConstructorEmpty) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{};
  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_TRUE(vec.empty());

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, InitializerListConstructorWithDynamicMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{1, 2, 3, 4, 5, 6, 7, 8};
  EXPECT_EQ(vec.size(), 8);
  EXPECT_GE(vec.capacity(), 8);
  EXPECT_FALSE(vec.empty());
  for(std::size_t i = 0; i < 8; ++i) { EXPECT_EQ(vec[i], static_cast<int>(i + 1)); }

  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_GE(AllocationStats::total_allocated(), 8 * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
}

TEST_F(SmallVectorTest, CopyAssignmentWithStaticToStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec1{1, 2, 3};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec2{10, 20};

  vec2 = vec1;
  EXPECT_EQ(vec2.size(), 3);
  EXPECT_EQ(vec2.capacity(), 4);
  for(std::size_t i = 0; i < 3; ++i) { EXPECT_EQ(vec2[i], vec1[i]); }

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
}

TEST_F(SmallVectorTest, CopyAssignmentWithStaticToDynamicMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec1{1, 2, 3};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec2{1, 2, 3, 4, 5, 6};

  // vec2 has dynamic memory, vec1 has static memory
  EXPECT_EQ(AllocationStats::allocation_count(), 1);

  vec2 = vec1;
  EXPECT_EQ(vec2.size(), 3);
  EXPECT_GE(vec2.capacity(), 6);
  for(std::size_t i = 0; i < 3; ++i) { EXPECT_EQ(vec2[i], vec1[i]); }

  // Dynamic memory should be deallocated when copying static to dynamic
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
}

TEST_F(SmallVectorTest, CopyAssignmentWithDynamicToDynamicMemorySmallToLarge) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec1{1, 2, 3, 4, 5, 6};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec2{10, 20, 30, 40, 50, 60, 70, 80};

  // Both vectors have dynamic memory
  EXPECT_EQ(AllocationStats::allocation_count(), 2);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 2);

  vec2 = vec1;
  EXPECT_EQ(vec2.size(), 6);
  EXPECT_GE(vec2.capacity(), 6);
  for(std::size_t i = 0; i < 6; ++i) { EXPECT_EQ(vec2[i], vec1[i]); }

  // vec2 should reuse its existing allocation if it has sufficient capacity,
  // or allocate new memory if needed
  EXPECT_EQ(AllocationStats::allocation_count(), 2);
  EXPECT_EQ(AllocationStats::total_allocated(), (6 + 8) * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 2);
}

TEST_F(SmallVectorTest, CopyAssignmentWithDynamicToDynamicMemoryLargeToSmall) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec1{1, 2, 3, 4, 5, 6, 7, 8};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec2{10, 20, 30, 40, 50, 60};

  // Both vectors have dynamic memory
  EXPECT_EQ(AllocationStats::allocation_count(), 2);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 2);

  vec2 = vec1;
  EXPECT_EQ(vec2.size(), 8);
  EXPECT_GE(vec2.capacity(), 8);
  for(std::size_t i = 0; i < 8; ++i) { EXPECT_EQ(vec2[i], vec1[i]); }

  // vec2 should allocate new memory to hold larger data
  EXPECT_EQ(AllocationStats::allocation_count(), 3); // One for vec2, one for vec1, one for new data
  EXPECT_EQ(AllocationStats::total_allocated(), (6 + 8 + 8) * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 1); // One deallocation for old vec2 memory
  EXPECT_EQ(AllocationStats::total_deallocated(), 6 * sizeof(int)); // Old vec2 memory
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 2);         // vec1 and new vec2 memory
}

TEST_F(SmallVectorTest, CopyAssignmentWithDynamicToStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec1{1, 2, 3, 4, 5, 6};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec2{10, 20};

  // vec1 has dynamic memory, vec2 has static memory
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

  vec2 = vec1;
  EXPECT_EQ(vec2.size(), 6);
  EXPECT_GE(vec2.capacity(), 6);
  for(std::size_t i = 0; i < 6; ++i) { EXPECT_EQ(vec2[i], vec1[i]); }

  // New allocation should occur for vec2 to hold dynamic data
  EXPECT_EQ(AllocationStats::allocation_count(), 2);
  EXPECT_EQ(AllocationStats::total_allocated(), (6 + 6) * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 2);
}

TEST_F(SmallVectorTest, MoveAssignmentWithStaticToDynamicMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec1{1, 2, 3};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec2{10, 20, 30, 40, 50, 60, 70};
  std::vector<int> vec1_copy(vec1.begin(), vec1.end());

  // vec1 has static memory, vec2 has dynamic memory
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

  size_t original_capacity = vec2.capacity();
  size_t new_capacity      = vec1.capacity();
  EXPECT_NE(original_capacity, new_capacity);

  vec2 = std::move(vec1);
  EXPECT_EQ(vec2.size(), 3);
  EXPECT_GE(vec2.capacity(), vec2.static_capacity);
  for(std::size_t i = 0; i < 3; ++i) { EXPECT_EQ(vec2[i], vec1_copy[i]); }

  EXPECT_TRUE(vec1.empty());
  EXPECT_EQ(vec1.size(), 0);
  EXPECT_EQ(vec1.capacity(), vec1.static_capacity);

  // One deallocation should occur (vec2's original memory)
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_EQ(AllocationStats::total_allocated(), 7 * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
}

TEST_F(SmallVectorTest, MoveAssignmentWithDynamicToStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec1{1, 2, 3, 4, 5, 6};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec2{10, 20};
  std::vector<int> vec1_copy(vec1.begin(), vec1.end());

  // vec1 has dynamic memory, vec2 has static memory
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

  vec2 = std::move(vec1);
  EXPECT_EQ(vec2.size(), 6);
  EXPECT_EQ(vec2.capacity(), 6);
  for(std::size_t i = 0; i < 6; ++i) { EXPECT_EQ(vec2[i], vec1_copy[i]); }

  EXPECT_TRUE(vec1.empty());
  EXPECT_EQ(vec1.size(), 0);
  EXPECT_EQ(vec1.capacity(), vec1.static_capacity);

  // No deallocation should occur since vec2 is static memory
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_EQ(AllocationStats::total_allocated(), 6 * sizeof(int));
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
}

TEST_F(SmallVectorTest, MoveAssignmentWithStaticToStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec1{1, 2, 3};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec2{10, 20};
  std::vector<int> vec1_copy(vec1.begin(), vec1.end());

  vec2 = std::move(vec1);
  EXPECT_EQ(vec2.size(), 3);
  EXPECT_EQ(vec2.capacity(), 4);
  EXPECT_TRUE(vec1.empty());
  for(std::size_t i = 0; i < 3; ++i) { EXPECT_EQ(vec2[i], vec1_copy[i]); }

  EXPECT_TRUE(vec1.empty());
  EXPECT_EQ(vec1.size(), 0);
  EXPECT_EQ(vec1.capacity(), vec1.static_capacity);

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, MoveAssignmentWithDynamicToDynamicMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec1{1, 2, 3, 4, 5, 6};
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec2{10, 20, 30, 40, 50, 60, 70};
  std::vector<int> vec1_copy(vec1.begin(), vec1.end());

  // Both vectors have dynamic memory
  EXPECT_EQ(AllocationStats::allocation_count(), 2);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 2);

  size_t original_capacity = vec2.capacity();
  size_t new_capacity      = vec1.capacity();
  EXPECT_NE(original_capacity, new_capacity);

  vec2 = std::move(vec1);
  EXPECT_EQ(vec2.size(), 6);
  EXPECT_GE(vec2.capacity(), new_capacity);
  EXPECT_TRUE(vec1.empty());
  for(std::size_t i = 0; i < 6; ++i) { EXPECT_EQ(vec2[i], vec1_copy[i]); }

  EXPECT_TRUE(vec1.empty());
  EXPECT_EQ(vec1.size(), 0);
  EXPECT_EQ(vec1.capacity(), vec1.static_capacity);

  // One deallocation should occur (vec2's original memory)
  EXPECT_EQ(AllocationStats::allocation_count(), 2);
  EXPECT_EQ(AllocationStats::deallocation_count(), 1);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
}

TEST_F(SmallVectorTest, PushBack) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec;

  for(int i = 1; i <= 4; ++i) {
    vec.push_back(i);
    EXPECT_EQ(vec.size(), static_cast<std::size_t>(i));
    EXPECT_EQ(vec.capacity(), 4);

    for(size_t j = 0; j < vec.size(); ++j) { EXPECT_EQ(vec[j], static_cast<int>(j + 1)); }
  }

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);

  // Push back more elements to trigger growth.
  for(int i = 5; i <= 100; ++i) {
    size_t cur_capacity      = vec.capacity();
    size_t cur_alloc_count   = AllocationStats::allocation_count();
    size_t cur_dealloc_count = AllocationStats::deallocation_count();
    vec.push_back(i);
    if(vec.size() > cur_capacity) {
      EXPECT_GT(vec.capacity(), cur_capacity);
      EXPECT_EQ(AllocationStats::allocation_count(), cur_alloc_count + 1);
      EXPECT_EQ(
          AllocationStats::deallocation_count(), cur_dealloc_count + (cur_alloc_count > 0 ? 1 : 0));
    } else {
      EXPECT_EQ(vec.capacity(), cur_capacity);
      EXPECT_EQ(AllocationStats::allocation_count(), cur_alloc_count);
      EXPECT_EQ(AllocationStats::deallocation_count(), cur_dealloc_count);
    }
    EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
    EXPECT_EQ(vec.size(), static_cast<std::size_t>(i));

    for(size_t j = 0; j < vec.size(); ++j) { EXPECT_EQ(vec[j], static_cast<int>(j + 1)); }
  }
}

TEST_F(SmallVectorTest, EmplaceBack) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec;

  for(int i = 1; i <= 4; ++i) {
    vec.emplace_back(i);
    EXPECT_EQ(vec.size(), static_cast<std::size_t>(i));
    EXPECT_EQ(vec.capacity(), 4);

    for(size_t j = 0; j < vec.size(); ++j) { EXPECT_EQ(vec[j], static_cast<int>(j + 1)); }
  }

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);

  // Push back more elements to trigger growth.
  for(int i = 5; i <= 100; ++i) {
    size_t cur_capacity      = vec.capacity();
    size_t cur_alloc_count   = AllocationStats::allocation_count();
    size_t cur_dealloc_count = AllocationStats::deallocation_count();
    vec.emplace_back(i);
    if(vec.size() > cur_capacity) {
      EXPECT_GT(vec.capacity(), cur_capacity);
      EXPECT_EQ(AllocationStats::allocation_count(), cur_alloc_count + 1);
      EXPECT_EQ(
          AllocationStats::deallocation_count(), cur_dealloc_count + (cur_alloc_count > 0 ? 1 : 0));
    } else {
      EXPECT_EQ(vec.capacity(), cur_capacity);
      EXPECT_EQ(AllocationStats::allocation_count(), cur_alloc_count);
      EXPECT_EQ(AllocationStats::deallocation_count(), cur_dealloc_count);
    }
    EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
    EXPECT_EQ(vec.size(), static_cast<std::size_t>(i));

    for(size_t j = 0; j < vec.size(); ++j) { EXPECT_EQ(vec[j], static_cast<int>(j + 1)); }
  }
}

TEST_F(SmallVectorTest, PopBackWithStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{1, 2, 3, 4};

  vec.pop_back();
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_EQ(vec[0], 1);
  EXPECT_EQ(vec[1], 2);
  EXPECT_EQ(vec[2], 3);

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, PopBackWithDynamicMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{1, 2, 3, 4, 5, 6};

  size_t init_capacity = vec.capacity();

  EXPECT_EQ(vec.size(), 6);

  for(size_t n = vec.size(); n > 0; --n) {
    EXPECT_EQ(vec.size(), n);
    EXPECT_EQ(vec.back(), static_cast<int>(n));
    vec.pop_back();
    EXPECT_EQ(vec.size(), n - 1);
    EXPECT_GE(vec.capacity(), init_capacity); // Capacity should not shrink
    if(n > 1) { EXPECT_EQ(vec.back(), static_cast<int>(n - 1)); }
  }

  EXPECT_EQ(vec.size(), 0);
  EXPECT_TRUE(vec.empty());

  // Memory should still be allocated (pop_back doesn't deallocate)
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
}

TEST_F(SmallVectorTest, ClearWithStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{1, 2, 3};

  vec.clear();
  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_TRUE(vec.empty());

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, ClearWithDynamicMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{1, 2, 3, 4, 5, 6};
  size_t capacity_before = vec.capacity();

  vec.clear();
  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec.capacity(), capacity_before); // Capacity should remain unchanged
  EXPECT_TRUE(vec.empty());

  // Memory should still be allocated (clear doesn't deallocate)
  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
}

TEST_F(SmallVectorTest, Resize) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec;

  vec.resize(2, 1);
  EXPECT_EQ(vec.size(), 2);
  EXPECT_EQ(vec.capacity(), 4);
  for(size_t i = 0; i < 2; ++i) { EXPECT_EQ(vec[i], 1); }
  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);

  vec.resize(0);
  EXPECT_EQ(vec.size(), 0);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_TRUE(vec.empty());

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);

  vec.resize(4);

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);

  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec.capacity(), 4);
  // Elements values are undefined after resize
  std::vector<int> cur{vec.begin(), vec.end()};

  vec.resize(6, 23);
  EXPECT_EQ(vec.size(), 6);
  EXPECT_GE(vec.capacity(), 6);
  size_t i = 0;
  for(; i < 4; ++i) { EXPECT_EQ(vec[i], cur[i]); }
  for(; i < 6; ++i) { EXPECT_EQ(vec[i], 23); }

  EXPECT_EQ(AllocationStats::allocation_count(), 1);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

  vec.resize(10, 42);
  EXPECT_EQ(vec.size(), 10);
  EXPECT_GE(vec.capacity(), 10);
  i = 0;
  for(; i < 4; ++i) { EXPECT_EQ(vec[i], cur[i]); }
  for(; i < 6; ++i) { EXPECT_EQ(vec[i], 23); }
  for(; i < 10; ++i) { EXPECT_EQ(vec[i], 42); }

  EXPECT_EQ(AllocationStats::allocation_count(), 2);
  EXPECT_EQ(AllocationStats::deallocation_count(), 1);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

  vec.resize(2, 99);
  EXPECT_EQ(vec.size(), 2);
  EXPECT_GE(vec.capacity(), 10);
  i = 0;
  for(; i < 2; ++i) { EXPECT_EQ(vec[i], cur[i]); }

  EXPECT_EQ(AllocationStats::allocation_count(), 2);
  EXPECT_EQ(AllocationStats::deallocation_count(), 1);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);

  vec.resize(0);

  EXPECT_EQ(vec.size(), 0);
  EXPECT_GE(vec.capacity(), 10);
  EXPECT_TRUE(vec.empty());

  EXPECT_EQ(AllocationStats::allocation_count(), 2);
  EXPECT_EQ(AllocationStats::deallocation_count(), 1);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
}

TEST_F(SmallVectorTest, ReserveWithStaticMemory) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{1, 2};

  vec.reserve(4);
  EXPECT_EQ(vec.size(), 2);
  EXPECT_EQ(vec.capacity(), 4);
  EXPECT_EQ(vec[0], 1);
  EXPECT_EQ(vec[1], 2);

  EXPECT_EQ(AllocationStats::allocation_count(), 0);
  EXPECT_EQ(AllocationStats::deallocation_count(), 0);
  EXPECT_EQ(AllocationStats::total_allocated(), 0);
  EXPECT_EQ(AllocationStats::total_deallocated(), 0);
  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, Reserve) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{1, 2};

  for(size_t i = 0; i < vec.static_capacity; ++i) {
    vec.reserve(i);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_GE(vec.capacity(), vec.static_capacity);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);

    EXPECT_EQ(AllocationStats::allocation_count(), 0);
    EXPECT_EQ(AllocationStats::deallocation_count(), 0);
    EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
  }

  // Reserve more than current capacity
  for(size_t i = vec.static_capacity + 1; i < 10; ++i) {
    size_t cur_capacity      = vec.capacity();
    size_t cur_alloc_count   = AllocationStats::allocation_count();
    size_t cur_dealloc_count = AllocationStats::deallocation_count();

    vec.reserve(i);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    if(i > cur_capacity) {
      EXPECT_GT(vec.capacity(), cur_capacity);
      EXPECT_EQ(AllocationStats::allocation_count(), cur_alloc_count + 1);
      EXPECT_EQ(
          AllocationStats::deallocation_count(), cur_dealloc_count + (cur_alloc_count > 0 ? 1 : 0));
    } else {
      EXPECT_EQ(vec.capacity(), cur_capacity);
      EXPECT_EQ(AllocationStats::allocation_count(), cur_alloc_count);
      EXPECT_EQ(AllocationStats::deallocation_count(), cur_dealloc_count);
    }
    EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
  }

  // Reserve to zero
  {
    size_t cur_capacity      = vec.capacity();
    size_t cur_alloc_count   = AllocationStats::allocation_count();
    size_t cur_dealloc_count = AllocationStats::deallocation_count();

    vec.reserve(0);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], 1);
    EXPECT_EQ(vec[1], 2);
    EXPECT_EQ(vec.capacity(), cur_capacity); // Capacity should remain unchanged
    EXPECT_EQ(AllocationStats::allocation_count(), cur_alloc_count);
    EXPECT_EQ(AllocationStats::deallocation_count(), cur_dealloc_count);
    EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
  }
}

TEST_F(SmallVectorTest, PushBackMoveSemantics) {
  {
    jacl::small_vector<std::unique_ptr<int>, 4, alloc_nonstateful_int_ptr_t> vec;

    for(int i = 1; i <= 4; ++i) {
      auto ptr = std::make_unique<int>(i);
      vec.push_back(std::move(ptr));
      EXPECT_EQ(vec.size(), static_cast<std::size_t>(i));
      EXPECT_EQ(vec.capacity(), 4);
      EXPECT_EQ(*vec[i - 1], i);
      EXPECT_EQ(ptr, nullptr); // Moved from
    }

    EXPECT_EQ(AllocationStats::allocation_count(), 0);
    EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);

    // Push back more elements to trigger growth
    for(int i = 5; i <= 10; ++i) {
      size_t total_allocated   = AllocationStats::total_allocated();
      size_t total_deallocated = AllocationStats::total_deallocated();
      size_t alloc_count       = AllocationStats::allocation_count();
      size_t dealloc_count     = AllocationStats::deallocation_count();

      size_t cur_capacity = vec.capacity();
      auto ptr            = std::make_unique<int>(i);
      vec.push_back(std::move(ptr));

      if(vec.size() > cur_capacity) {
        EXPECT_GT(vec.capacity(), cur_capacity);
        EXPECT_EQ(AllocationStats::allocation_count(), alloc_count + 1);
        EXPECT_EQ(AllocationStats::deallocation_count(),
            dealloc_count + (alloc_count > 0 ? 1 : 0)); // One deallocation if reallocating
        EXPECT_EQ(AllocationStats::total_allocated(),
            total_allocated + vec.capacity() * sizeof(std::unique_ptr<int>));
        EXPECT_EQ(AllocationStats::total_deallocated(),
            total_deallocated +
                (alloc_count > 0 ? cur_capacity * sizeof(std::unique_ptr<int>) : 0));
      }
      EXPECT_EQ(vec.size(), static_cast<std::size_t>(i));
      EXPECT_EQ(*vec[i - 1], i);
      EXPECT_EQ(ptr, nullptr); // Moved from

      EXPECT_EQ(AllocationStats::outstanding_allocations(), 1);
    }
  }

  EXPECT_EQ(AllocationStats::outstanding_allocations(), 0);
}

TEST_F(SmallVectorTest, EmplaceBackReturnValue) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec;

  for(int i = 1; i <= 4; ++i) {
    int& ref = vec.emplace_back(i);
    EXPECT_EQ(ref, i);
    EXPECT_EQ(&ref, &vec.back());
    EXPECT_EQ(vec.size(), static_cast<std::size_t>(i));
  }

  // Test with dynamic memory allocation
  for(int i = 5; i <= 10; ++i) {
    int& ref = vec.emplace_back(i);
    EXPECT_EQ(ref, i);
    EXPECT_EQ(&ref, &vec.back());
    EXPECT_EQ(vec.size(), static_cast<std::size_t>(i));
  }
}

TEST_F(SmallVectorTest, PushBackCopySemantics) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec;

  for(int i = 1; i <= 4; ++i) {
    int value = i;
    vec.push_back(value);
    EXPECT_EQ(vec.size(), static_cast<std::size_t>(i));
    EXPECT_EQ(vec.capacity(), 4);
    EXPECT_EQ(vec[i - 1], i);
    EXPECT_EQ(value, i); // Original value unchanged
  }

  // Test reallocation behavior
  int value           = 42;
  size_t old_capacity = vec.capacity();
  vec.push_back(value);
  EXPECT_GT(vec.capacity(), old_capacity);
  EXPECT_EQ(vec.back(), 42);
  EXPECT_EQ(value, 42); // Original value unchanged
}

TEST_F(SmallVectorTest, EmplaceBackWithMultipleArgs) {
  struct TestStruct {
    int a, b, c;
    TestStruct(int x, int y, int z) : a(x), b(y), c(z) {}
    bool operator==(const TestStruct& other) const {
      return a == other.a && b == other.b && c == other.c;
    }
  };

  jacl::small_vector<TestStruct, 2, MockAllocator<TestStruct, NonstatefulPolicy>> vec;

  vec.emplace_back(1, 2, 3);
  EXPECT_EQ(vec.size(), 1);
  EXPECT_EQ(vec[0].a, 1);
  EXPECT_EQ(vec[0].b, 2);
  EXPECT_EQ(vec[0].c, 3);

  vec.emplace_back(4, 5, 6);
  EXPECT_EQ(vec.size(), 2);
  EXPECT_EQ(vec[1].a, 4);
  EXPECT_EQ(vec[1].b, 5);
  EXPECT_EQ(vec[1].c, 6);

  // Test with dynamic allocation
  vec.emplace_back(7, 8, 9);
  EXPECT_EQ(vec.size(), 3);
  EXPECT_GT(vec.capacity(), 2);
  EXPECT_EQ(vec[2].a, 7);
  EXPECT_EQ(vec[2].b, 8);
  EXPECT_EQ(vec[2].c, 9);
}

TEST_F(SmallVectorTest, PushBackSelfReference) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{1, 2, 3};

  // Push back a reference to an existing element
  vec.push_back(vec[0]);
  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec[3], 1);
  EXPECT_EQ(vec[0], 1); // Original element unchanged

  // Force reallocation and test self-reference
  vec.push_back(vec[1]);
  EXPECT_EQ(vec.size(), 5);
  EXPECT_GT(vec.capacity(), 4);
  EXPECT_EQ(vec[4], 2);
  EXPECT_EQ(vec[1], 2); // Original element unchanged
}

TEST_F(SmallVectorTest, EmplaceBackSelfReference) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec{10, 20, 30};

  // Emplace back using a reference to an existing element
  vec.emplace_back(vec[0]);
  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec[3], 10);

  // Force reallocation and test self-reference
  vec.emplace_back(vec[1]);
  EXPECT_EQ(vec.size(), 5);
  EXPECT_GT(vec.capacity(), 4);
  EXPECT_EQ(vec[4], 20);
}

TEST_F(SmallVectorTest, PushBackEmptyVector) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec;
  EXPECT_TRUE(vec.empty());

  vec.push_back(42);
  EXPECT_FALSE(vec.empty());
  EXPECT_EQ(vec.size(), 1);
  EXPECT_EQ(vec[0], 42);
  EXPECT_EQ(vec.front(), 42);
  EXPECT_EQ(vec.back(), 42);
}

TEST_F(SmallVectorTest, EmplaceBackEmptyVector) {
  jacl::small_vector<int, 4, alloc_nonstateful_int_t> vec;
  EXPECT_TRUE(vec.empty());

  int& ref = vec.emplace_back(99);
  EXPECT_FALSE(vec.empty());
  EXPECT_EQ(vec.size(), 1);
  EXPECT_EQ(vec[0], 99);
  EXPECT_EQ(ref, 99);
  EXPECT_EQ(&ref, &vec[0]);
}

TEST_F(SmallVectorTest, PushBackCapacityGrowth) {
  jacl::small_vector<int, 2, alloc_nonstateful_int_t> vec;

  // Fill initial capacity
  vec.push_back(1);
  vec.push_back(2);
  EXPECT_EQ(vec.size(), 2);
  EXPECT_EQ(vec.capacity(), 2);
  EXPECT_EQ(AllocationStats::allocation_count(), 0);

  // Trigger first growth
  vec.push_back(3);
  EXPECT_EQ(vec.size(), 3);
  EXPECT_GT(vec.capacity(), 2);
  size_t first_growth = vec.capacity();
  EXPECT_EQ(AllocationStats::allocation_count(), 1);

  // Fill to capacity again
  while(vec.size() < vec.capacity()) { vec.push_back(static_cast<int>(vec.size() + 1)); }

  size_t pre_growth_count = AllocationStats::allocation_count();

  // Trigger second growth
  vec.push_back(static_cast<int>(vec.size() + 1));
  EXPECT_GT(vec.capacity(), first_growth);
  EXPECT_EQ(AllocationStats::allocation_count(), pre_growth_count + 1);
  EXPECT_EQ(AllocationStats::deallocation_count(), pre_growth_count);
}
