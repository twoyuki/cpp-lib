#include <array>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

constexpr size_t DYNAMIC_CAPACITY = std::numeric_limits<size_t>::max();

namespace Utilities {

template <typename FirstForwardIt, typename SecondForwardIt>
void move_to(FirstForwardIt first, FirstForwardIt last, SecondForwardIt to) {
  std::uninitialized_move(first, last, to);
  std::destroy(first, last);
}

template <typename FirstForwardIt, typename SecondForwardIt>
void swap_ranges(FirstForwardIt first_begin, FirstForwardIt first_end,
                 SecondForwardIt second_begin, SecondForwardIt second_end) {
  while (first_begin != first_end && second_begin != second_end) {
    std::swap(*first_begin, *second_begin);
    ++first_begin;
    ++second_begin;
  }

  if (first_begin == first_end) {
    move_to(second_begin, second_end, first_end);
  } else {
    move_to(first_begin, first_end, second_end);
  }
}

template <typename T, size_t Capacity>
class CircularBufferBase {
 public:
  CircularBufferBase() = default;

  explicit CircularBufferBase(size_t capacity) {
    if (capacity != Capacity) {
      throw std::invalid_argument("Uncorrect capacity");
    }
  }

  ~CircularBufferBase() = default;

  size_t capacity() const { return Capacity; }

 protected:
  CircularBufferBase(const CircularBufferBase&) {}

  T* data() { return reinterpret_cast<T*>(buffer_.data()); }

  const T* data() const { return reinterpret_cast<const T*>(buffer_.data()); }

 private:
  alignas(std::max_align_t)
      std::array<std::byte, (Capacity + 1) * sizeof(T)> buffer_;
};

template <typename T>
class CircularBufferBase<T, DYNAMIC_CAPACITY> {
 public:
  CircularBufferBase() = delete;

  explicit CircularBufferBase(size_t capacity)
      : data_(reinterpret_cast<T*>(new std::byte[(capacity + 1) * sizeof(T)])),
        capacity_(capacity) {}

  ~CircularBufferBase() { delete[] reinterpret_cast<std::byte*>(data_); }

  size_t capacity() const { return capacity_; }

 protected:
  CircularBufferBase(const CircularBufferBase& another)
      : CircularBufferBase(another.capacity_) {}

  T* data() { return data_; }

  const T* data() const { return data_; }

  void swap(CircularBufferBase& another) {
    std::swap(data_, another.data_);
    std::swap(capacity_, another.capacity_);
  }

 private:
  T* data_;
  size_t capacity_;
};

template <size_t Capacity>
class IteratorBase {
 protected:
  IteratorBase() = default;

  IteratorBase(size_t) {}

  size_t capacity() const { return Capacity; }

  void set_capacity(size_t) {}
};

template <>
class IteratorBase<DYNAMIC_CAPACITY> {
 protected:
  IteratorBase() = default;

  IteratorBase(size_t capacity) : capacity_(capacity) {}

  size_t capacity() const { return capacity_; }

  void set_capacity(size_t capacity) { capacity_ = capacity; }

 private:
  size_t capacity_;
};

}  // namespace Utilities

template <typename T, size_t Capacity = DYNAMIC_CAPACITY>
class CircularBuffer : private Utilities::CircularBufferBase<T, Capacity> {
 private:
  template <bool IsConst>
  class Iterator : private Utilities::IteratorBase<Capacity> {
   private:
    using Base = Utilities::IteratorBase<Capacity>;
    using Base::capacity;
    using Base::set_capacity;

    using ValueType = std::conditional_t<IsConst, const T, T>;

   public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using reference = ValueType&;
    using pointer = ValueType*;

    Iterator() = default;

    Iterator(const Iterator<false>& another)
        : Base(another.capacity()),
          data_(another.data_),
          translation_(another.translation_) {}

    pointer base() const {
      return data_ + math_mod(translation_, capacity() + 1);
    }

    reference operator*() const { return *base(); }

    reference operator[](size_t index) { return *(*this + index); }

    pointer operator->() const { return base(); }

    Iterator& operator=(const Iterator<false>& other) {
      set_capacity(other.capacity());
      data_ = other.data_;
      translation_ = other.translation_;
      return *this;
    }

    Iterator& operator++() { return *this += 1; }

    Iterator& operator--() { return *this += -1; }

    Iterator& operator+=(difference_type number) {
      translation_ += number;
      return *this;
    }

    Iterator& operator-=(difference_type number) { return *this += -number; }

    Iterator operator++(int) {
      auto copy = *this;
      ++*this;
      return copy;
    }

    Iterator operator--(int) {
      auto copy = *this;
      --*this;
      return copy;
    }

    Iterator operator+(difference_type number) const {
      auto copy = *this;
      copy += number;
      return copy;
    }

    Iterator operator-(difference_type number) const {
      return *this + (-number);
    }

    friend Iterator operator+(difference_type number, Iterator iterator) {
      Iterator result = iterator + number;
      return result;
    }

    friend difference_type operator-(const Iterator& lhs, const Iterator& rhs) {
      return lhs.translation_ - rhs.translation_;
    }

    friend auto operator<=>(const Iterator& lhs, const Iterator& rhs) {
      return lhs.translation_ <=> rhs.translation_;
    }

    friend bool operator==(const Iterator& lhs, const Iterator& rhs) {
      return lhs.data_ == rhs.data_ && lhs.translation_ == rhs.translation_;
    }

    friend bool operator!=(const Iterator& lhs, const Iterator& rhs) {
      return !(lhs == rhs);
    }

    friend class CircularBuffer;

   private:
    pointer data_;
    difference_type translation_;

    explicit Iterator(pointer data, size_t translation, size_t capacity)
        : Base(capacity), data_(data), translation_(translation) {}

    static difference_type math_mod(difference_type lhs, difference_type rhs) {
      return ((lhs % rhs) + rhs) % rhs;
    }
  };

  using Base = Utilities::CircularBufferBase<T, Capacity>;

 public:
  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  using Base::capacity;
  using Base::CircularBufferBase;

  CircularBuffer(const CircularBuffer& other)
      : Base(other), size_(other.size_), translation_(0) {
    std::uninitialized_copy(other.begin(), other.end(), begin());
  }

  ~CircularBuffer() { std::destroy(begin(), end()); }

  size_t size() const { return size_; }

  size_t empty() const { return size_ == 0; }

  size_t full() const { return size_ == capacity(); }

  T& at(size_t index) {
    check_index(index);
    return (*this)[index];
  }

  const T& at(size_t index) const {
    check_index(index);
    return (*this)[index];
  }

  iterator begin() { return iterator(data(), translation_, capacity()); }

  const_iterator begin() const {
    return const_iterator(data(), translation_, capacity());
  }

  iterator end() { return iterator(data(), translation_ + size_, capacity()); }

  const_iterator end() const {
    return const_iterator(data(), translation_ + size_, capacity());
  }

  const_iterator cbegin() const { return begin(); }

  const_iterator cend() const { return end(); }

  reverse_iterator rbegin() { return reverse_iterator(end()); }

  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }

  reverse_iterator rend() { return reverse_iterator(begin()); }

  const_reverse_iterator rend() const { return const_reverse_iterator(end()); }

  const_reverse_iterator crbegin() const { return rbegin(); }

  const_reverse_iterator crend() const { return rend(); }

  void push_back(const T& value) {
    std::construct_at(end().base(), value);

    if (full()) {
      pop_front();
    }

    ++size_;
  }

  void push_front(const T& value) {
    std::construct_at((--begin()).base(), value);

    if (full()) {
      pop_back();
    }

    decrement_translation();
    ++size_;
  }

  void pop_back() {
    --size_;
    std::destroy_at(end().base());
  }

  void pop_front() {
    --size_;
    std::destroy_at(begin().base());
    increment_translation();
  }

  void insert(iterator position, const T& value) {
    if (full() && position == begin()) {
      return;
    }

    iterator current = end();
    iterator previous = current - 1;

    std::construct_at(current.base(), value);

    while (current != position) {
      std::swap(*previous, *current);

      --current;
      --previous;
    }

    if (full()) {
      pop_front();
    }

    ++size_;
  }

  void erase(iterator position) {
    iterator next = position + 1;
    iterator end_position = end();

    while (next != end_position) {
      std::swap(*position, *next);

      ++position;
      ++next;
    }

    std::destroy_at(position.base());
    --size_;
  }

  void swap(CircularBuffer& another) {
    if constexpr (Capacity == DYNAMIC_CAPACITY) {
      Base::swap(another);
    } else {
      Utilities::swap_ranges(begin(), end(), another.begin(), another.end());
    }

    std::swap(size_, another.size_);
    std::swap(translation_, another.translation_);
  }

  T& operator[](size_t index) { return begin()[index]; }

  const T& operator[](size_t index) const { return begin()[index]; }

  CircularBuffer& operator=(const CircularBuffer& another) {
    auto copy = another;
    swap(copy);
    return *this;
  }

 private:
  using Base::data;

  size_t size_ = 0;
  size_t translation_ = 0;

  void increment_translation() { (++translation_) %= capacity() + 1; }

  void decrement_translation() {
    (translation_ += capacity()) %= capacity() + 1;
  }

  void check_index(size_t index) const {
    if (index >= size_) {
      throw std::out_of_range("Index out of range");
    }
  }
};