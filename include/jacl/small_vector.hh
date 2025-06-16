#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deferral.hh>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>

#if defined(__has_include) && __has_include(<version>)
#include <version>
#endif // defined(__has_include) && __has_include(<version>)

#define JACL_LIKELY(x)   __builtin_expect(!!(x), 1)
#define JACL_UNLIKELY(x) __builtin_expect(!!(x), 0)

#if defined(_MSC_VER)
#define JACL_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define JACL_FORCE_INLINE __attribute__((always_inline)) inline
#else
// Default fallback
#define JACL_FORCE_INLINE inline
#endif

// attribute hidden
#if defined(_MSC_VER)
#define JACL_VISIBILITY_HIDDEN
#elif defined(__GNUC__)
#define JACL_VISIBILITY_HIDDEN __attribute__((__visibility__("hidden")))
#else
#define JACL_VISIBILITY_HIDDEN
#endif // defined(_MSC_VER)

// JACL_MAYBE_UNUSED suppresses compiler warnings on unused entities, if any.
#if defined(__clang__)
#if (__clang_major__ * 10 + __clang_minor__) >= 39 && __cplusplus >= 201703L
#define JACL_MAYBE_UNUSED [[maybe_unused]]
#else
#define JACL_MAYBE_UNUSED __attribute__((__unused__))
#endif
#elif defined(__GNUC__)
#if __GNUC__ >= 7 && __cplusplus >= 201703L
#define JACL_MAYBE_UNUSED [[maybe_unused]]
#else
#define JACL_MAYBE_UNUSED __attribute__((__unused__))
#endif
#elif defined(_MSC_VER)
#if _MSC_VER >= 1911 && defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
#define JACL_MAYBE_UNUSED [[maybe_unused]]
#else
#define JACL_MAYBE_UNUSED __pragma(warning(suppress : 4100 4101 4189))
#endif
#else
#define JACL_MAYBE_UNUSED
#endif

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define JACL_RESTRICT     __restrict
#define JACL_HAS_RESTRICT 1
#elif defined(__APPLE__) || defined(__clang__) || defined(__GNUC__)
#define JACL_RESTRICT     __restrict__
#define JACL_HAS_RESTRICT 1
#else
#define JACL_RESTRICT
#define JACL_HAS_RESTRICT 0
#endif

#if defined(__cpp_if_constexpr) && __cpp_if_constexpr >= 201606
#define JACL_IF_CONSTEXPR if constexpr
#else
#define JACL_IF_CONSTEXPR if
#endif

#if !defined(JACL_NO_EXCEPTIONS)
#if !__cpp_exceptions
#define JACL_NO_EXCEPTIONS 1
#else
#define JACL_NO_EXCEPTIONS 0
#endif // !__cpp_exceptions
#endif // !defined(JACL_NO_EXCEPTIONS)

#if defined(__cpp_concepts) && __cpp_concepts >= 201907
#include <concepts>
#define JACL_CONCEPTS_SUPPORTED 1
#else
#define JACL_CONCEPTS_SUPPORTED 0
#endif // defined(__cpp_concepts) && __cpp_concepts >= 201907

#if defined(__cpp_lib_allocate_at_least) && __cpp_lib_allocate_at_least >= 202102
#define JACL_ALLOCATE_AT_LEAST_SUPPORTED 1
#else
#define JACL_ALLOCATE_AT_LEAST_SUPPORTED 0
#endif // defined(__cpp_lib_allocate_at_least) && __cpp_lib_allocate_at_least >=
       // 202102

#if defined(__cpp_lib_to_address) && __cpp_lib_to_address >= 201711
#define JACL_TO_ADDRESSES_SUPPORTED 1
#else
#define JACL_TO_ADDRESSES_SUPPORTED 0
#endif // defined(__cpp_lib_to_address) && __cpp_lib_to_address >= 201711

namespace jacl {
namespace internal {

template <typename iterT>
struct remove_restrict {
  using type = iterT;
}; // struct remove_restrict

#if JACL_HAS_RESTRICT

template <typename ptrT>
struct remove_restrict<ptrT * JACL_RESTRICT> {
  using type = ptrT*;
}; // struct remove_restrict

template <typename ptrT>
struct remove_restrict<ptrT * JACL_RESTRICT const> {
  using type = ptrT* const;
}; // struct remove_restrict

template <typename ptrT>
struct remove_restrict<ptrT * JACL_RESTRICT volatile> {
  using type = ptrT* volatile;
}; // struct remove_restrict

template <typename ptrT>
struct remove_restrict<ptrT * JACL_RESTRICT const volatile> {
  using type = ptrT* const volatile;
}; // struct remove_restrict

#endif // JACL_HAS_RESTRICT

template <typename ptrT>
using remove_restrict_t = typename remove_restrict<ptrT>::type;

} // namespace internal

/**
 * @brief A small vector that stores elements on the stack.
 *
 * The `small_vector` is an array that stores elements on the stack. If the
 * number of elements is less than or equal to the capacity of the
 * 1small_vector1, the elements are stored on the stack. Otherwise, the elements
 * are stored on the heap.
 *
 * @tparam valueT The type of the elements.
 * @tparam capacityN The capacity of the small vector.
 */
template <typename valueT, size_t sizeN, typename allocT = std::allocator<valueT>>
class small_vector : public allocT {
  using allocator_traits = std::allocator_traits<allocT>;

public:
  using value_type             = valueT;
  using allocator_type         = allocT;
  using reference              = value_type&;
  using const_reference        = const value_type&;
  using size_type              = typename allocator_traits::size_type;
  using difference_type        = typename allocator_traits::difference_type;
  using pointer                = typename allocator_traits::pointer;
  using const_pointer          = typename allocator_traits::const_pointer;
  using iterator               = pointer;
  using const_iterator         = const_pointer;
  using reverse_iterator       = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  /**
   * @brief The static capacity of the small vector.
   *
   * This is the number of elements that can be stored on the stack. If the size
   * of the vector exceeds this `extent`, the elements are stored on the heap.
   *
   * @note Even if the size of the vector is less than or equal to `extent`,
   * elements may still be stored on the heap. This can happen if the vector was
   * previously heap-allocated and then resized to a size less than or equal to
   * `extent`.
   */
#if __cplusplus >= 201703L
  static constexpr size_type extent = sizeN;
#else
  enum { extent = sizeN };
#endif // __cplusplus >= 201703L

private:
  using this_type          = small_vector<valueT, sizeN, allocT>;
  using internal_size_type = uint32_t;

  pointer data_{reinterpret_cast<pointer>(inline_data_)};
  internal_size_type size_{};
  union {
    alignas(value_type) uint8_t inline_data_[sizeof(value_type) * sizeN];
    internal_size_type capacity_;
  };

  static_assert(sizeN > 0, "small_vector: sizeN must be greater than 0");

  static constexpr bool value_is_trivially_move_assignable =
      std::is_trivially_move_assignable<value_type>::value;
  static constexpr bool value_is_trivially_move_constructible =
      std::is_trivially_move_constructible<value_type>::value;
  static constexpr bool value_is_trivially_copy_assignable =
      std::is_trivially_copy_assignable<value_type>::value;
  static constexpr bool value_is_trivially_copy_constructible =
      std::is_trivially_copy_constructible<value_type>::value;
  static constexpr bool value_is_trivially_constructible =
      std::is_trivially_constructible<value_type>::value;
  static constexpr bool value_is_trivially_destructible =
      std::is_trivially_destructible<value_type>::value;

  /**
   * @brief Check if the data is heap-allocated.
   *
   * @return `true` if the data is heap-allocated, `false` otherwise.
   */
  int is_heap_allocated() const noexcept {
    return int(data_ != reinterpret_cast<const_pointer>(inline_data_));
  }

  allocator_type& allocator() noexcept { return static_cast<allocator_type&>(*this); }
  const allocator_type& allocator() const noexcept {
    return static_cast<const allocator_type&>(*this);
  }

  std::pair<pointer, size_type> allocate(internal_size_type n) {
    check_max_size(n);

#if JACL_ALLOCATE_AT_LEAST_SUPPORTED && JACL_CONCEPTS_SUPPORTED
    constexpr bool has_allocate_at_least_in_traits =
        requires(allocator_type a, size_type n) { allocator_traits::alloca_at_least(a, n); };
    constexpr bool has_allocate_at_least_in_allocator =
        requires(allocator_type a, size_type n) { a.allocate_at_least(n); };

    if constexpr(has_allocate_at_least_in_traits) {
      auto result = allocator_traits::allocate_at_least(allocator(), n);
      return {result.ptr, result.count};
    } else if constexpr(has_allocate_at_least_in_allocator) {
      auto result = allocator().allocate_at_least(n);
      return {result.ptr, result.count};
    }
#endif // JACL_ALLOCATE_AT_LEAST_SUPPORTED && JACL_CONCEPTS_SUPPORTED

    return {allocator_traits::allocate(allocator(), n), n};
  }

  JACL_FORCE_INLINE void deallocate(pointer p, internal_size_type n) {
    if(p != reinterpret_cast<pointer>(inline_data_))
      allocator_traits::deallocate(allocator(), p, n);
  }

  template <typename... argTs>
  JACL_FORCE_INLINE pointer construct_at(pointer JACL_RESTRICT p, argTs&&... args) {
    JACL_IF_CONSTEXPR(!value_is_trivially_constructible || sizeof...(argTs) > 0) {
      allocator_traits::construct(allocator(), p, std::forward<argTs>(args)...);
    }
    return p;
  }

  JACL_FORCE_INLINE void destroy_at(pointer p) noexcept {
    JACL_IF_CONSTEXPR(!value_is_trivially_destructible) { p->~value_type(); }
  }

  JACL_FORCE_INLINE void destroy_n(pointer first, internal_size_type n) noexcept {
    JACL_IF_CONSTEXPR(!value_is_trivially_destructible) {
      for(size_type i = 0; i < n; ++i) destroy_at(first + i);
    }
  }

  void check_max_size(const size_type sz) const {
#if !defined(JACL_SMALL_VECTOR_DISABLE_MAX_SIZE_CHECK)
    if(JACL_UNLIKELY(sz > max_size())) {
#if !JACL_NO_EXCEPTIONS
      throw std::length_error{"small_vector: new size exceeds max_size"};
#else
      std::abort();
#endif // JACL_NO_EXCEPTIONS
    }
#endif // JACL_SMALL_VECTOR_DISABLE_MAX_SIZE_CHECK
  }

  /**
   * @brief Insert new elements into the vector at the specified position.
   *
   * This function inserts `n` new elements into the vector at the specified
   * position, which can be at any position within the range of the vector. The
   * new elements are constructed using the provided construction callback. The
   * capacity growth callback is used to determine the new capacity of the
   * vector.
   *
   * @tparam construct_cbT The type of the new-element construction callback.
   * @tparam grow_cbT The type of the capacity growth callback.
   * @param position The position to insert the new elements.
   * @param n The number of new elements to insert.
   * @param construct_cb The new-element construction callback.
   * @param grow_cb The capacity growth callback.
   * @return iterator The iterator pointing to the first inserted element.
   */
  template <typename construct_cbT, typename grow_cbT>
  iterator insert_impl(const_iterator position, const internal_size_type n,
      construct_cbT&& construct_cb, grow_cbT&& grow_cb) {
    // Check for valid size and new size.
    if(JACL_UNLIKELY(n == 0)) return const_cast<iterator>(position);
    internal_size_type new_size = size_ + n;
    internal_size_type cur_cap  = capacity();

    if(new_size <= cur_cap) {
      // Handle cases where the new size fits in the current capacity.

      // Set pointers for source and destination ranges, and the number of
      // elements to move. All offsets are relative to these two pointers, so
      // the source and destination ranges are [src, src + move_cnt) and [dest,
      // dest + move_cnt), respectively. These ranges may overlap.
      pointer JACL_RESTRICT const src_first = const_cast<pointer>(position);
      pointer JACL_RESTRICT src_last        = cend();
      pointer JACL_RESTRICT const dest      = const_cast<pointer>(position) + n;
      const internal_size_type move_cnt     = src_last - position;

      JACL_IF_CONSTEXPR(
          value_is_trivially_move_assignable && value_is_trivially_move_constructible) {
        std::memmove(dest, src_first, move_cnt * sizeof(value_type));
      }
      else if(dest < src_last) {
        // source and destination ranges overlap.

        // Move data in two stages:
        //  1. Non-overlapping elements past the end that are to be moved past
        //  the end
        //  2. Overlapping elements within the original vector extent (in
        //  reverse order)
        pointer JACL_RESTRICT const src_mid  = src_last - n;
        dest                                += move_cnt;

        while(src_last != src_mid) {
          --src_last;
          --dest;
          construct_at(dest, std::move(*src_last));
        }
        while(src_last != src_first) {
          --src_last;
          --dest;
          *dest = std::move(*src_last);
          destroy_at(src_last);
        }
      }
      else {
        // source and destination ranges do not overlap.
        for(; src_first != src_last; ++src_first, ++dest) {
          construct_at(dest, std::move(*src_first));
          destroy_at(src_first);
        }
      }

      // Construct the new elements.
      construct_cb(src_first, n);
    } else {
      // Handle cases where we need to allocate a new buffer.
      const internal_size_type new_cap = grow_cb(size_, new_size);
      const size_type lo_size          = position - cbegin();
      const size_type hi_size          = size_ - lo_size;
      alloc_assign_internal(new_cap, cur_cap, [&](pointer dest) {
        pointer dest_position = dest + lo_size;
        construct_cb(dest_position, n);
        move_data(dest, data_, lo_size);
        move_data(dest_position + n, position, hi_size);
        position = dest_position;
        return new_size;
      });
    }

    return const_cast<iterator>(position);
  }

  template <typename construct_cbT>
  iterator insert_grow(const_iterator position, size_type n, construct_cbT&& construct_cb) {
    return insert_impl(position, n, construct_cb, [](size_type cur_size, size_type min_size) {
      return std::min(std::max(cur_size + (cur_size >> 1) + 1, min_size), max_size());
    });
  }

  template <typename construct_cbT>
  iterator insert_n(const_iterator position, size_type n, construct_cbT&& construct_cb) {
    return insert_impl(
        position, n, construct_cb, [](size_type, size_type min_size) { return min_size; });
  }

  void copy_data(
      pointer JACL_RESTRICT dest, const_pointer JACL_RESTRICT src, internal_size_type n) {
    JACL_IF_CONSTEXPR(value_is_trivially_copy_constructible && value_is_trivially_copy_assignable) {
      std::memcpy(dest, src, n * sizeof(value_type));
    }
    else {
      internal_size_type i = 0;
      defer_fail { destroy_n(dest, i); };
      for(; i < n; ++i) { construct_at(dest + i, src[i]); }
    }
  }

  void move_data(pointer JACL_RESTRICT dest, pointer JACL_RESTRICT src, internal_size_type n) {
    JACL_IF_CONSTEXPR(value_is_trivially_move_constructible && value_is_trivially_move_assignable) {
      std::memcpy(dest, src, n * sizeof(value_type));
    }
    else {
      internal_size_type i = 0;
      defer_fail { destroy_n(dest, i); };
      for(; i < n; ++i) { construct_at(dest + i, std::move(src[i])); }
    }
  }

  template <typename... argTs>
  void fill_data(pointer JACL_RESTRICT dest, internal_size_type n, argTs&&... value) {
    JACL_IF_CONSTEXPR(sizeof...(argTs) == 0 && value_is_trivially_constructible) return;
    internal_size_type i = 0;
    defer_fail { destroy_n(dest, i); };
    for(; i < n; ++i) { construct_at(dest + i, std::forward<argTs>(value)...); }
  }

  template <typename callbackT>
  void alloc_assign_internal(internal_size_type sz, internal_size_type cur_cap, callbackT&& cb) {
    auto alloc_result      = allocate(sz);
    pointer new_data       = alloc_result.first;
    size_type new_capacity = alloc_result.second;
    defer {
      // On success: deallocate the old buffer.
      // On failure: deallocate the new buffer.
      deallocate(new_data, new_capacity);
    };
    size_ = cb(new_data);

    std::swap(data_, new_data);
    capacity_    = new_capacity;
    new_capacity = cur_cap;
  }

  template <typename callbackT>
  void assign_internal(internal_size_type sz, internal_size_type cur_cap, callbackT&& cb) {
    if(sz > cur_cap) {
      alloc_assign_internal(sz, cur_cap, std::forward<callbackT>(cb));
    } else {
      destroy_n(data_, size_);
      size_ = cb(data_);
    }
  }

  void move_internal(small_vector&& other) {
    clear();

    if(other.is_heap_allocated()) {
      deallocate(data_, capacity());

      // Take ownership of the heap-allocated data from the other vector.
      data_     = std::exchange(other.data_, reinterpret_cast<pointer>(other.inline_data_));
      capacity_ = other.capacity_;
    } else {
      // Copy into the existing buffer. `other` is using inline data, so this
      // vecor is guaranteed to have enough capacity.
      move_data(data_, other.data_, other.size_);
      other.data_ = reinterpret_cast<pointer>(other.inline_data_);
    }

    size_ = std::exchange(other.size_, 0);
  }

  template <typename... argTs>
  void fill_internal(internal_size_type sz, internal_size_type cur_cap, argTs&&... args) {
    assign_internal(sz, cur_cap, [&](pointer JACL_RESTRICT dest) {
      fill_data(dest, sz, std::forward<argTs>(args)...);
      return sz;
    });
  }

  template <typename iterT>
  void assign_iter(iterT first, iterT last, internal_size_type cur_cap) {
    JACL_IF_CONSTEXPR(std::is_base_of<std::random_access_iterator_tag,
        typename std::iterator_traits<iterT>::iterator_category>::value) {
      size_t sz = std::distance(first, last);
      assign_internal(sz, cur_cap, [&](pointer JACL_RESTRICT dest) {
        size_type i = 0;
        defer_fail { destroy_n(dest, i); };
        for(; i < sz; ++i) construct_at(dest + i, *first++);
        return i;
      });
    }
    else {
      // Handle input, forward, or bidirectional iterators.
      clear();
      for(; first != last; ++first) emplace_back(*first);
    }
  }

public:
  /**
   * @brief The static capacity of the small vector.
   */
  static constexpr size_type static_capacity = sizeN;

  small_vector() noexcept(std::is_nothrow_default_constructible<allocator_type>::value) = default;

  explicit small_vector(const allocator_type& a) noexcept(
      std::is_nothrow_copy_constructible<allocator_type>::value) : allocator_type{a} {}

  explicit small_vector(size_type n) noexcept(
      std::is_nothrow_default_constructible<value_type>::value &&
      std::is_nothrow_default_constructible<allocator_type>::value) :
      small_vector{n, allocator_type{}} {}

  explicit small_vector(size_type n, const allocator_type& a) : allocator_type{a} {
    assign_internal(n, static_capacity, [&](pointer JACL_RESTRICT dest) {
      fill_data(dest, n);
      return n;
    });
  }

  small_vector(size_type n, const value_type& value,
      const allocator_type& a =
          allocator_type{}) noexcept(std::is_nothrow_copy_constructible<allocator_type>::value) :
      allocator_type{a} {
    assign_internal(n, static_capacity, [&](pointer JACL_RESTRICT dest) {
      fill_data(dest, n, value);
      return n;
    });
  }

  template <typename iterT,
      typename = typename std::enable_if<std::is_base_of<std::input_iterator_tag,
          typename std::iterator_traits<iterT>::iterator_category>::value>::type>
  small_vector(iterT first, iterT last, const allocator_type& a = allocator_type{}) noexcept(
      std::is_nothrow_copy_constructible<allocator_type>::value) : allocator_type{a} {
    assign_iter(first, last, static_capacity);
  }

  small_vector(const small_vector& other) noexcept(
      std::is_nothrow_copy_constructible<value_type>::value &&
      std::is_nothrow_copy_constructible<allocator_type>::value) :
      allocator_type{allocator_traits::select_on_container_copy_construction(other.allocator())} {
    assign_internal(other.size_, static_capacity, [&](pointer JACL_RESTRICT dest) {
      copy_data(dest, other.data_, other.size_);
      return other.size_;
    });
  }

  small_vector(small_vector&& other) noexcept(
      std::is_nothrow_move_constructible<allocator_type>::value) :
      allocator_type{std::move(static_cast<allocator_type&>(other))} {
    move_internal(std::move(other));
  }

  small_vector(std::initializer_list<value_type> il) noexcept(
      std::is_nothrow_copy_constructible<value_type>::value &&
      std::is_nothrow_copy_constructible<allocator_type>::value) :
      small_vector{il.begin(), il.end(), allocator_type{}} {}

  small_vector(std::initializer_list<value_type> il, const allocator_type& a) noexcept(
      std::is_nothrow_copy_constructible<value_type>::value &&
      std::is_nothrow_copy_constructible<allocator_type>::value) :
      small_vector{il.begin(), il.end(), a} {}

  ~small_vector() {
    destroy_n(data_, size_);
    deallocate(data_, capacity_);
  }

  small_vector& operator=(const small_vector& other) {
    if(this == &other) return *this;

    JACL_IF_CONSTEXPR(allocator_traits::propagate_on_container_copy_assignment::value) {
      if(allocator() != other.allocator()) {
        clear();
        deallocate(data_, capacity_);
        data_       = reinterpret_cast<pointer>(inline_data_);
        allocator() = other.allocator();
      }
    }

    assign_internal(other.size_, capacity(), [&](pointer JACL_RESTRICT dest) {
      copy_data(dest, other.data_, other.size_);
      return other.size_;
    });

    return *this;
  }

  small_vector& operator=(small_vector&& other)
#if __cplusplus >= 201703L
      noexcept(allocator_traits::propagate_on_container_move_assignment::value ||
               allocator_traits::is_always_equal::value)
#endif // __cplusplus >= 201703L
  {
    // Destroy existing elements to make room for the new elements.

    JACL_IF_CONSTEXPR(allocator_traits::propagate_on_container_move_assignment::value) {
      if(allocator() != other.allocator()) {
        clear();
        deallocate(data_, capacity_);
        data_       = reinterpret_cast<pointer>(inline_data_);
        allocator() = std::move(other.allocator());
      }
    }

    move_internal(std::move(other));

    return *this;
  }

  small_vector& operator=(std::initializer_list<value_type> il) {
    assign(il.begin(), il.end());
    return *this;
  }

  template <typename iterT>
  void assign(iterT first, iterT last) {
    static_assert(
        std::is_constructible<value_type, typename std::iterator_traits<iterT>::reference>::value,
        "small_vector::assign: value type is not constructible from the "
        "dereferenced iterator type");
    static_assert(!std::is_base_of<std::output_iterator_tag,
                      typename std::iterator_traits<iterT>::iterator_category>::value,
        "small_vector::assign: output iterators are not allowed");
    assign_iter(first, last, capacity());
  }

  void assign(size_type sz, const value_type& val) { fill_internal(sz, capacity(), val); }

  void assign(std::initializer_list<value_type> il) { assign(il.begin(), il.end()); }

  const allocator_type& get_allocator() const noexcept { return *this; }

  iterator begin() noexcept { return data_; }

  const_iterator begin() const noexcept { return data_; }

  iterator end() noexcept { return data_ + size_; }

  const_iterator end() const noexcept { return data_ + size_; }

  reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

  reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

  const_iterator cbegin() const noexcept { return begin(); }

  const_iterator cend() const noexcept { return end(); }

  const_reverse_iterator crbegin() const noexcept { return rbegin(); }

  const_reverse_iterator crend() const noexcept { return rend(); }

  size_type size() const noexcept { return size_; }

  static constexpr size_type max_size() noexcept {
    return std::numeric_limits<internal_size_type>::max() / sizeof(value_type);
  }

  size_type capacity() const noexcept { return is_heap_allocated() ? capacity_ : static_capacity; }

  bool empty() const noexcept { return size_ == 0; }

  reference operator[](size_type n) { return data_[n]; }
  const_reference operator[](size_type n) const {
    const_cast<const_reference>(
        const_cast<small_vector<valueT, sizeN, allocT>*>(this)->operator[](n));
  }
  reference at(size_type n) {
    if(n >= size_) {
#if !JACL_NO_EXCEPTIONS
      throw std::out_of_range{"small_vector::at"};
#else
      std::abort();
#endif // JACL_NO_EXCEPTIONS
    }
    return data_[n];
  }
  const_reference at(size_type n) const {
    return const_cast<const_reference>(
        const_cast<small_vector<valueT, sizeN, allocT>*>(this)->at(n));
  }

  reference front() { return data_[0]; }
  const_reference front() const {
    return const_cast<const_reference>(
        const_cast<small_vector<valueT, sizeN, allocT>*>(this)->front());
  }
  reference back() { return data_[size_ - 1]; }
  const_reference back() const {
    return const_cast<const_reference>(
        const_cast<small_vector<valueT, sizeN, allocT>*>(this)->back());
  }

  pointer data() noexcept { return data_; }
  const_pointer data() const noexcept { return data_; }

  void push_back(const value_type& x) { emplace_back(x); }
  void push_back(value_type&& x) { emplace_back(std::move(x)); }
  template <class... Args>
  reference emplace_back(Args&&... args) {
    auto cur_cap = capacity();
    if(JACL_UNLIKELY(size_ == cur_cap)) {
      auto new_size = std::min<internal_size_type>(size_ + (size_ >> 1) + 1, max_size());
      reserve(new_size);
    }

    reference result = *construct_at(data_ + size_, std::forward<Args>(args)...);
    ++size_;
    return result;
  }

  void pop_back() {
    const size_type offset = size_ - 1;
    destroy_at(data_ + offset);
    size_ = offset;
  }

  template <typename... Args>
  iterator emplace(const_iterator position, Args&&... args) {
    return insert_grow(position, 1, [&](pointer JACL_RESTRICT const p, const size_type) {
      construct_at(p, std::forward<Args>(args)...);
    });
  }

  template <typename iterT>
  iterator insert(const_iterator position, iterT first, iterT last) {
    return insert_n(position, std::distance(first, last),
        [&](pointer JACL_RESTRICT const p, size_type /* n */) {
          std::uninitialized_copy(first, last, p);
        });
  }

  iterator insert(const_iterator position, std::initializer_list<value_type> il) {
    return insert(position, il.begin(), il.end());
  }

  iterator erase(const_iterator position) { return erase(position, position + 1); }

  iterator erase(const_iterator first, const_iterator last) {
    if(first == last) return const_cast<iterator>(first);

    pointer JACL_RESTRICT const f = const_cast<pointer>(first);
    pointer JACL_RESTRICT const l = const_cast<pointer>(last);
    pointer JACL_RESTRICT const e = data_ + size_;
    size_type n                   = std::distance(f, l);
    destroy_n(f, n);

    std::move(l, e, f);
    size_ -= n;

    return f;
  }

  void clear() noexcept {
    destroy_n(data_, size_);
    size_ = 0;
  }

  void reserve(size_type sz) {
    size_type cur_cap = capacity();
    if(sz > cur_cap) {
      alloc_assign_internal(sz, cur_cap, [&](pointer JACL_RESTRICT const dest) {
        // Move the existing data to the new buffer.
        move_data(dest, data_, size_);
        return size_;
      });
    }
  }

  void shrink_to_fit() noexcept {
    size_type cur_cap = capacity();
    if(size_ < static_capacity && cur_cap > static_capacity) {
      // Shrink to inline data.
      move_data(inline_data_, data_, size_);
      destroy_n(data_, size_);
      deallocate(data_, cur_cap);
      data_     = reinterpret_cast<pointer>(inline_data_);
      capacity_ = static_capacity;
    } else if(size_ != cur_cap) {
      // Shrink to new allocation.
      alloc_assign_internal(size_, cur_cap, [&](pointer JACL_RESTRICT const dest) {
        // Move the existing data to the new buffer.
        move_data(dest, data_, size_);
        return size_;
      });
    }
  }

  void resize(size_type sz) {
    if(sz > size_) {
      reserve(sz);
      fill_data(data_ + size_, sz - size_);
    } else if(sz < size_) {
      destroy_n(data_ + sz, size_ - sz);
    }
    size_ = sz;
  }

  void resize(size_type sz, const value_type& value) {
    if(sz > size_) {
      reserve(sz);
      fill_data(data_ + size_, sz - size_, value);
    } else if(sz < size_) {
      destroy_n(data_ + sz, size_ - sz);
    }
    size_ = sz;
  }

  void swap(small_vector& other) noexcept(allocator_traits::propagate_on_container_swap::value ||
                                          allocator_traits::is_always_equal::value) {
    auto swap_allocator = [](allocator_type& l, allocator_type& r) {
      JACL_IF_CONSTEXPR(allocator_traits::propagate_on_container_swap::value) { std::swap(l, r); }
    };

    auto swap_inline_x_inline = [&](small_vector& l, small_vector& r) {
      pointer JACL_RESTRICT const l_first = l.begin();
      pointer JACL_RESTRICT const l_last  = l.end();
      pointer JACL_RESTRICT const r_first = r.begin();
      pointer JACL_RESTRICT const r_last  = r.end();

      // Swap the elements in the inline data. `l_size` <= `r_size`:
      //   l: [0, ..., l_size)
      //   r: [0, ..., l_size, ..., r_size)
      for(; l_first != l_last; ++l_first, ++r_first) std::swap(*l_first, *r_first);
      for(; r_first != r_last; ++r_first, ++l_first) {
        construct_at(l_first, std::move(*r_first));
        destroy_at(r_first);
      }

      std::swap(l.size_, r.size_);
      swap_allocator(l, r);
    };

    auto swap_heap_x_inline = [&](small_vector& l, small_vector& r) {
      // l is heap-allocated, r is inline.
      const auto l_data     = l.data_;
      const auto l_size     = l.size_;
      const auto l_capacity = l.capacity_;

      l.data_ = reinterpret_cast<pointer>(l.inline_data_);
      for(size_type i = 0; i < l_size; ++i) construct_at(l.data_ + i, std::move(r.data_[i]));
      l.size_ = other.size_;

      r.data_     = l_data;
      r.size_     = l_size;
      r.capacity_ = l_capacity;
    };

    if(this == &other) return;
    switch(is_heap_allocated() | (other.is_heap_allocated() << 1)) {
    case 0x0:
      // Both are inline; swap the elements.
      if(size_ <= other.size_) {
        swap_inline_x_inline(*this, other);
      } else {
        swap_inline_x_inline(other, *this);
      }
      break;
    case 0x1:
      // `this` is heap-allocated and `other` is inline.
      swap_heap_x_inline(*this, other);
      break;
    case 0x2:
      // `this` is inline, `other` is heap-allocated.
      swap_heap_x_inline(other, *this);
      break;
    case 0x3:
      // Both are heap-allocated, swap the members.
      std::swap(data_, other.data_);
      std::swap(size_, other.size_);
      std::swap(capacity_, other.capacity_);
      swap_allocator(*this, other);
      break;
    }
  }
}; // class small_vector

} // namespace jacl

namespace std {

template <typename valueT, size_t sizeN, typename allocT>
void swap(jacl::small_vector<valueT, sizeN, allocT>& lhs,
    jacl::small_vector<valueT, sizeN, allocT>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

} // namespace std
