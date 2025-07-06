**small_vector** is a C++ container that combines the efficiency of static allocation with the flexibility of dynamic resizing. It maintains a fixed-size internal buffer (like `std::array`) for small data sets, but seamlessly switches to dynamic heap allocation when the number of elements exceeds the static capacity. Its API closely matches `std::vector` for ease of use and compatibility. `small_vector` supports C++11, C++14, C++17, and C++20 standards.

**Key Features:**
- Statically allocates a fixed-size buffer for small data sets (no heap allocation).
- Automatically switches to dynamic allocation when capacity is exceeded.
- API closely matches `std::vector` (iterators, `push_back`, `emplace_back`, etc.).
- Supports all major C++ standards from C++11 to C++20.
- Provides efficient memory usage and performance for small and large collections.
- Optimize data move and copy operations to skip construction/destruction of
  trivial types (e.g. `int`, `char`, `void*`, _etc._).

**Example Usage:**
```cpp
jacl::small_vector<int, 8> numbers; // 8 elements stored inline, more triggers heap allocation
numbers.push_back(1);
numbers.push_back(2);
// ... use like std::vector
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
