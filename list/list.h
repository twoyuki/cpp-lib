#include <cstddef>
#include <iterator>
#include <memory>
#include <new>
#include <type_traits>

void* align(size_t alignment, size_t size, void*& ptr, size_t& space) {
  if (space < size) {
    return nullptr;
  }

  auto intptr = reinterpret_cast<unsigned long long>(ptr);
  auto aligned = (intptr - 1u + alignment) & -alignment;
  auto difference = aligned - intptr;

  if (difference > space - size) {
    return nullptr;
  }

  space -= difference;
  ptr = reinterpret_cast<void*>(aligned);
  return ptr;
}

template <size_t N>
class StackStorage {
 public:
  StackStorage() = default;

  StackStorage(const StackStorage&) = delete;

  void* allocate(size_t size, size_t alignment) {
    void* ptr = buffer_ + offset_;
    size_t space = N - offset_;

    if (!align(alignment, size, ptr, space)) {
      throw std::bad_alloc();
    }

    offset_ = N - space + size;
    return ptr;
  }

  StackStorage& operator=(const StackStorage&) = delete;

 private:
  alignas(std::max_align_t) std::byte buffer_[N];
  size_t offset_ = 0;
};

template <typename T, size_t N>
class StackAllocator {
 public:
  using value_type = T;

  template <typename U>
  struct rebind {
    using other = StackAllocator<U, N>;
  };

  explicit StackAllocator(StackStorage<N>& storage) : storage_(&storage) {}

  template <typename U>
  StackAllocator(const StackAllocator<U, N>& other)
      : storage_(other.storage_) {}

  T* allocate(size_t count) {
    return static_cast<T*>(storage_->allocate(sizeof(T) * count, alignof(T)));
  }

  void deallocate(T*, size_t) {}

  friend bool operator==(const StackAllocator& lhs, const StackAllocator& rhs) {
    return lhs.storage_ == rhs.storage_;
  }

  friend bool operator!=(const StackAllocator& lhs, const StackAllocator& rhs) {
    return !(lhs == rhs);
  }

  template <typename, size_t>
  friend class StackAllocator;

 private:
  StackStorage<N>* storage_;
};

template <typename T, typename Allocator = std::allocator<T>>
class List {
 private:
  struct BaseNode {
    BaseNode* previous;
    BaseNode* next;

    BaseNode() { init(); }

    BaseNode(BaseNode* previous, BaseNode* next)
        : previous(previous), next(next) {}

    void init() { previous = next = this; }
  };

  struct Node : public BaseNode {
    T value;

    template <typename... Args>
    Node(BaseNode* previous, BaseNode* next, Args&&... args)
        : BaseNode(previous, next), value(std::forward<Args>(args)...) {}
  };

  template <bool IsConst>
  class Iterator {
   private:
    using ReturnType = std::conditional_t<IsConst, const T, T>;

   public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = ReturnType*;
    using reference = ReturnType&;

    Iterator() = default;

    Iterator(const Iterator<false>& other) : node_(other.node_) {}

    pointer operator->() const { return std::addressof(get_value()); }

    reference operator*() const { return get_value(); }

    Iterator& operator++() {
      node_ = node_->next;
      return *this;
    }

    Iterator operator++(int) {
      Iterator copy = *this;
      ++*this;
      return copy;
    }

    Iterator& operator--() {
      node_ = node_->previous;
      return *this;
    }

    Iterator operator--(int) {
      Iterator copy = *this;
      --*this;
      return copy;
    }

    Iterator& operator=(const Iterator<false>& other) {
      node_ = other.node_;
      return *this;
    }

    friend bool operator==(const Iterator& lhs, const Iterator& rhs) {
      return lhs.node_ == rhs.node_;
    }

    friend bool operator!=(const Iterator& lhs, const Iterator& rhs) {
      return !(lhs == rhs);
    }

    friend class List;

   private:
    BaseNode* node_;

    Iterator(BaseNode* node) : node_(node) {}

    reference get_value() const { return static_cast<Node*>(node_)->value; }
  };

  using NodeAllocator =
      typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
  using NodeAllocatorTraits = std::allocator_traits<NodeAllocator>;

 public:
  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  List() = default;

  explicit List(const Allocator& allocator) : allocator_(allocator) {}

  explicit List(size_t count, const Allocator& allocator = Allocator())
      : allocator_(allocator) {
    exception_safety_construct(
        [this](size_t count) {
          for (; count != 0; --count) {
            this->emplace_back();
          }
        },
        count);
  }

  List(size_t count, const T& value, const Allocator& allocator = Allocator())
      : allocator_(allocator) {
    exception_safety_construct(
        [this](size_t count, const T& value) {
          for (; count != 0; --count) {
            this->emplace_back(value);
          }
        },
        count, value);
  }

  List(const List& other)
      : allocator_(NodeAllocatorTraits::select_on_container_copy_construction(
            other.allocator_)) {
    exception_safety_construct(
        [this](const_iterator first, const_iterator last) {
          for (; first != last; ++first) {
            this->emplace_back(*first);
          }
        },
        other.begin(), other.end());
  }

  ~List() { clear(); }

  bool empty() const { return size_ == 0; }

  size_t size() const { return size_; }

  iterator begin() { return iterator(begin_node()); }

  const_iterator begin() const { return const_iterator(begin_node()); }

  iterator end() { return iterator(end_node()); }

  const_iterator end() const { return const_iterator(end_node()); }

  const_iterator cbegin() const { return begin(); }

  const_iterator cend() const { return end(); }

  reverse_iterator rbegin() { return reverse_iterator(end()); }

  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }

  reverse_iterator rend() { return reverse_iterator(begin()); }

  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  const_reverse_iterator crbegin() const { return rbegin(); }

  const_reverse_iterator crend() const { return rend(); }

  Allocator get_allocator() const { return allocator_; }

  void push_back(const T& value) { emplace(end(), value); }

  void push_front(const T& value) { emplace(begin(), value); }

  void pop_back() { erase(--end()); }

  void pop_front() { erase(begin()); }

  iterator insert(const_iterator position, const T& value) {
    return emplace(position, value);
  }

  iterator erase(const_iterator position) {
    Node* node = static_cast<Node*>(position.node_);
    BaseNode* previous = node->previous;
    BaseNode* next = node->next;
    NodeAllocatorTraits::destroy(allocator_, node);
    put_node(node);
    previous->next = next;
    next->previous = previous;
    --size_;
    return iterator(next);
  }

  List& operator=(const List& other) {
    if (this == std::addressof(other)) {
      return *this;
    }

    constexpr bool pocca =
        NodeAllocatorTraits::propagate_on_container_copy_assignment::value;
    constexpr bool iae = NodeAllocatorTraits::is_always_equal::value;

    if constexpr (pocca) {
      if (!iae && allocator_ != other.allocator_) {
        clear();
      }

      allocator_ = other.allocator_;
    }

    assign_dispatch(other.begin(), other.end());

    return *this;
  }

 private:
  BaseNode end_;
  size_t size_ = 0;
  [[no_unique_address]] NodeAllocator allocator_;

  BaseNode* begin_node() const { return end_.next; }

  BaseNode* end_node() const { return const_cast<BaseNode*>(&end_); }

  Node* get_node() { return NodeAllocatorTraits::allocate(allocator_, 1); }

  void put_node(Node* node) {
    NodeAllocatorTraits::deallocate(allocator_, node, 1);
  }

  template <typename... Args>
  iterator emplace(const_iterator position, Args&&... args) {
    Node* new_node = get_node();
    BaseNode* previous = position.node_->previous;
    BaseNode* next = position.node_;

    try {
      NodeAllocatorTraits::construct(allocator_, new_node, previous, next,
                                     std::forward<Args>(args)...);
    } catch (...) {
      put_node(new_node);
      throw;
    }

    previous->next = new_node;
    next->previous = new_node;
    ++size_;
    return iterator(new_node);
  }

  template <typename... Args>
  iterator emplace_back(Args&&... args) {
    return emplace(end(), std::forward<Args>(args)...);
  }

  void default_initialize(size_t count) {
    for (; count != 0; --count) {
      emplace_back();
    }
  }

  void fill_initialize(size_t count, const T& value) {
    for (; count != 0; --count) {
      emplace_back(value);
    }
  }

  void copy_initialize(const_iterator first, const_iterator last) {
    for (; first != last; ++first) {
      emplace_back(*first);
    }
  }

  void clear() { erase(begin(), end()); }

  void assign_dispatch(const_iterator other_first, const_iterator other_last) {
    iterator first = begin();
    iterator last = end();

    while (first != last && other_first != other_last) {
      *first = *other_first;

      ++first;
      ++other_first;
    }

    if (first == last) {
      insert(last, other_first, other_last);
    } else {
      erase(first, last);
    }
  }

  void erase(const_iterator first, const_iterator last) {
    while (first != last) {
      first = erase(first);
    }
  }

  void insert(const_iterator pos, const_iterator first, const_iterator last) {
    for (; first != last; ++first) {
      insert(pos, *first);
    }
  }

  template <typename Function, typename... Args>
  void exception_safety_construct(Function function, Args&&... args) {
    try {
      function(std::forward<Args>(args)...);
    } catch (...) {
      clear();
      throw;
    }
  }
};