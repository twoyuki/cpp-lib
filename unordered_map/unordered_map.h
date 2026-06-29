#include <cmath>
#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace Utilities {

template <typename Allocator, typename ForwardIt>
void destroy(Allocator& allocator, ForwardIt first, ForwardIt last) {
  using AllocatorTraits = std::allocator_traits<Allocator>;

  for (; first != last; ++first) {
    AllocatorTraits::destroy(allocator, std::addressof(*first));
  }
}

template <typename Allocator, typename ForwardIterator>
void uninitialized_default_construct(Allocator& allocator,
                                     ForwardIterator first,
                                     ForwardIterator last) {
  using AllocatorTraits = std::allocator_traits<Allocator>;

  ForwardIterator current = first;

  try {
    for (; current != last; ++current) {
      AllocatorTraits::construct(allocator, std::addressof(*current));
    }
  } catch (...) {
    destroy(allocator, first, current);
    throw;
  }
}

template <typename T>
struct AlignedBuffer {
  alignas(T) std::byte storage[sizeof(T)];

  void* address() { return static_cast<void*>(storage); }

  const void* address() const { return static_cast<const void*>(storage); }

  T* pointer() { return static_cast<T*>(address()); }

  const T* pointer() const { return static_cast<const T*>(address()); }

  T& value() { return *pointer(); }

  const T& value() const { return *pointer(); }
};

}  // namespace Utilities

template <typename Key, typename T, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<std::pair<const Key, T>>>
class UnorderedMap {
 public:
  using NodeType = std::pair<const Key, T>;

 private:
  struct BaseNode {
    BaseNode* next = nullptr;

    BaseNode() = default;

    BaseNode(const BaseNode&) = default;

    BaseNode(BaseNode&&) = default;

    BaseNode& operator=(const BaseNode&) = default;

    BaseNode& operator=(BaseNode&&) = default;
  };

  struct Node : public BaseNode {
    Utilities::AlignedBuffer<NodeType> storage;
    size_t hash;

    NodeType& pair() { return storage.value(); }

    const NodeType& pair() const { return storage.value(); }
  };

  template <bool IsConst>
  class Iterator {
   private:
    using ValueType = std::conditional_t<IsConst, const NodeType, NodeType>;

   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = NodeType;
    using difference_type = std::ptrdiff_t;
    using pointer = ValueType*;
    using reference = ValueType&;

    Iterator() = default;

    Iterator(const Iterator<false>& other) : node_(other.node_) {}

    pointer operator->() const { return std::addressof(**this); }

    reference operator*() const { return get_node()->pair(); }

    Iterator& operator++() {
      node_ = node_->next;
      return *this;
    }

    Iterator operator++(int) {
      auto copy = *this;
      ++*this;
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

    friend class UnorderedMap;

   private:
    BaseNode* node_ = nullptr;

    explicit Iterator(BaseNode* node) : node_(node) {}

    Node* get_node() const { return static_cast<Node*>(node_); }

    size_t get_hash() const { return get_node()->hash; }
  };

 public:
  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;

 private:
  static iterator const_cast_iterator(const_iterator position) {
    return iterator(position.node_);
  }

  using AllocatorTraits = std::allocator_traits<Allocator>;
  using NodeAllocator = typename AllocatorTraits::template rebind_alloc<Node>;
  using NodeAllocatorTrairs =
      typename AllocatorTraits::template rebind_traits<Node>;
  using HashTableAllocator =
      typename AllocatorTraits::template rebind_alloc<iterator>;
  using HashTableAllocatorTrairs =
      typename AllocatorTraits::template rebind_traits<iterator>;

 public:
  UnorderedMap() = default;

  UnorderedMap(const UnorderedMap& other)
      : hasher_(other.hasher_),
        comparator_(other.comparator_),
        allocator_(AllocatorTraits::select_on_container_copy_construction(
            other.allocator_)),
        bucket_count_(other.bucket_count_),
        hash_table_(allocate_table(allocator_, bucket_count_)),
        max_load_factor_(other.max_load_factor_) {
    init_table();

    try {
      insert(other.begin(), other.end());
    } catch (...) {
      destroy();
      throw;
    }
  }

  UnorderedMap(UnorderedMap&& other)
      : hasher_(std::move(other.hasher_)),
        comparator_(std::move(other.comparator_)),
        allocator_(std::move(other.allocator_)),
        size_(other.size_),
        before_begin_(other.before_begin_),
        bucket_count_(other.bucket_count_),
        hash_table_(other.hash_table_),
        max_load_factor_(other.max_load_factor_) {
    if (size_) {
      iterator first = begin();
      hash_table_[bucket_number(first.get_hash())] = before_begin();
    }
    other.reset();
  }

  ~UnorderedMap() { destroy(); }

  bool empty() const { return size_ == 0; }

  double max_load_factor() const { return max_load_factor_; }

  double load_factor() const {
    return static_cast<double>(size_) / static_cast<double>(bucket_count_);
  }

  size_t size() const { return size_; }

  iterator begin() { return iterator(before_begin_.next); }

  const_iterator begin() const { return const_iterator(before_begin_.next); }

  iterator end() { return iterator(); }

  const_iterator end() const { return const_iterator(); }

  const_iterator cbegin() const { return begin(); }

  const_iterator cend() const { return end(); }

  iterator find(const Key& key) {
    return const_cast_iterator(std::as_const(*this).find(key));
  }

  const_iterator find(const Key& key) const {
    const_iterator last = end();

    if (empty()) {
      return last;
    }

    size_t hash = get_hash(key);

    const_iterator position = hash_table_[bucket_number(hash)];

    if (position == last) {
      return last;
    }

    ++position;

    for (; position != last &&
           bucket_number(position.get_hash()) == bucket_number(hash);
         ++position) {
      if (is_equal(position->first, key)) {
        return position;
      }
    }

    return last;
  }

  iterator erase(const_iterator position) {
    iterator last = end();

    Node* node = position.get_node();
    size_t hash = node->hash;

    iterator& bucket = hash_table_[bucket_number(hash)];
    iterator previous = bucket;
    iterator next = const_cast_iterator(std::next(position));

    for (auto current = std::next(previous); current != position; ++current) {
      ++previous;
    }

    erase_after(previous);
    destroy_node(node);
    --size_;

    if (next != last && bucket_number(next.get_hash()) != bucket_number(hash)) {
      hash_table_[bucket_number(next.get_hash())] = previous;
    }

    const_iterator first_element = std::next(bucket);

    if (first_element == last ||
        bucket_number(first_element.get_hash()) != bucket_number(hash)) {
      bucket = last;
    }

    return const_cast_iterator(next);
  }

  iterator erase(const_iterator first, const_iterator last) {
    while (first != last) {
      first = erase(first);
    }
    return const_cast_iterator(last);
  }

  std::pair<iterator, bool> insert(const NodeType& value) {
    return emplace(value);
  }

  std::pair<iterator, bool> insert(NodeType&& value) {
    return emplace(std::move(value));
  }

  template <typename Pair>
  std::pair<iterator, bool> insert(Pair&& value) {
    return emplace(std::forward<Pair>(value));
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    if (!hash_table_) {
      rehash(11);
    }

    Node* new_node = construct_node(std::forward<Args>(args)...);

    iterator position = find(new_node->pair().first);

    if (position != end()) {
      destroy_node(new_node);
      return std::make_pair(position, false);
    }

    insert(new_node);

    ++size_;

    if (load_factor() >= max_load_factor()) {
      rehash(bucket_count_ * 2);
    }

    return std::make_pair(iterator(new_node), true);
  }

  T& at(const Key& key) { return const_cast<T&>(std::as_const(*this).at(key)); }

  const T& at(const Key& key) const {
    const_iterator position = find(key);

    if (position == end()) {
      throw std::out_of_range("UnorderedMap::at");
    }

    return position->second;
  }

  void reserve(size_t count) {
    rehash(std::ceil(static_cast<double>(count) / max_load_factor_));
  }

  void max_load_factor(double new_max_load_factor) {
    max_load_factor_ = new_max_load_factor;
  }

  template <typename InputIterator>
  void insert(InputIterator first, InputIterator last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  void swap(UnorderedMap& other) {
    std::swap(hasher_, other.hasher_);
    std::swap(comparator_, other.comparator_);

    if constexpr (AllocatorTraits::propagate_on_container_swap::value) {
      std::swap(allocator_, other.allocator_);
    }

    std::swap(size_, other.size_);
    std::swap(before_begin_, other.before_begin_);
    std::swap(bucket_count_, other.bucket_count_);
    std::swap(hash_table_, other.hash_table_);
    std::swap(max_load_factor_, other.max_load_factor_);
  }

  T& operator[](const Key& key) { return get_value(key); }

  T& operator[](Key&& key) { return get_value(std::move(key)); }

  UnorderedMap& operator=(const UnorderedMap& other) {
    if (this == std::addressof(other)) {
      return *this;
    }

    using pocca =
        typename AllocatorTraits::propagate_on_container_copy_assignment;
    using ioe = typename AllocatorTraits::is_always_equal;

    if constexpr (pocca::value) {
      if constexpr (!ioe::value) {
        if (allocator_ != other.allocator_) {
          destroy();
          reset();
        }
      }

      allocator_ = other.allocator_;
    }

    assign_dispatch(other.begin(), other.end());

    bucket_count_ = other.bucket_count_;
    max_load_factor_ = other.max_load_factor_;

    rehash(bucket_count_);

    return *this;
  }

  UnorderedMap& operator=(UnorderedMap&& other) {
    if (this == std::addressof(other)) {
      return *this;
    }

    using pocma =
        typename AllocatorTraits::propagate_on_container_move_assignment;
    using ioe = typename AllocatorTraits::is_always_equal;

    destroy();

    hasher_ = std::move(other.hasher_);
    comparator_ = std::move(other.comparator_);
    max_load_factor_ = other.max_load_factor_;

    if constexpr (pocma::value) {
      allocator_ = std::move(other.allocator_);
    } else if constexpr (!pocma::value && !ioe::value) {
      if (allocator_ != other.allocator_) {
        insert(std::move_iterator(other.begin()),
               std::move_iterator(other.end()));
        other.reset();
        return *this;
      }
    }

    before_begin_ = other.before_begin_;
    size_ = other.size_;
    bucket_count_ = other.bucket_count_;
    hash_table_ = other.hash_table_;

    if (size_) {
      iterator first = begin();
      hash_table_[bucket_number(first.get_hash())] = before_begin();
    }

    other.reset();

    return *this;
  }

 private:
  [[no_unique_address]] Hash hasher_;
  [[no_unique_address]] KeyEqual comparator_;
  [[no_unique_address]] Allocator allocator_;

  size_t size_ = 0;
  BaseNode before_begin_;
  size_t bucket_count_ = 1;
  iterator* hash_table_ = nullptr;
  double max_load_factor_ = 1;

  bool is_equal(const Key& lhs, const Key& rhs) const {
    return comparator_(lhs, rhs);
  }

  size_t get_hash(const Key& key) const { return hasher_(key); }

  template <typename... Args>
  Node* construct_node(Args&&... args) {
    NodeAllocator node_allocator = allocator_;

    Node* node = allocate_node(node_allocator);

    NodeAllocatorTrairs::construct(node_allocator, node);

    try {
      auto ptr = node->storage.pointer();
      AllocatorTraits::construct(allocator_, ptr, std::forward<Args>(args)...);
    } catch (...) {
      NodeAllocatorTrairs::destroy(node_allocator, node);
      deallocate_node(node_allocator, node);
      throw;
    }

    node->hash = get_hash(node->pair().first);

    return node;
  }

  void destroy_node(Node* node) {
    NodeAllocator node_allocator = allocator_;
    AllocatorTraits::destroy(allocator_, node->storage.pointer());
    NodeAllocatorTrairs::destroy(node_allocator, node);
    deallocate_node(node_allocator, node);
  }

  void insert(Node* node) {
    iterator position = hash_table_[bucket_number(node->hash)];

    if (position != end()) {
      insert_after(position, node);
      return;
    }

    insert_after(before_begin(), node);
    hash_table_[bucket_number(node->hash)] = before_begin();
    iterator next = std::next(begin());

    if (next != end()) {
      hash_table_[bucket_number(next.get_hash())] = begin();
    }
  }

  void insert_after(const_iterator position, Node* node) {
    node->next = position.node_->next;
    position.node_->next = node;
  }

  void erase_after(const_iterator position) {
    position.node_->next = position.node_->next->next;
  }

  template <typename KeyType>
  T& get_value(KeyType&& key) {
    auto position = find(key);

    if (position != end()) {
      return position->second;
    }

    auto result = emplace(std::piecewise_construct,
                          std::forward_as_tuple(std::forward<KeyType>(key)),
                          std::forward_as_tuple());
    return result.first->second;
  }

  size_t bucket_number(size_t hash) const { return hash % bucket_count_; }

  iterator before_begin() { return iterator(std::addressof(before_begin_)); }

  void reset() {
    size_ = 0;
    before_begin_.next = nullptr;
    bucket_count_ = 1;
    hash_table_ = nullptr;
  }

  void assign_dispatch(const_iterator other_first, const_iterator other_last) {
    iterator first = begin();
    iterator last = end();

    while (first != last && other_first != other_last) {
      *first = *other_first;
      first.get_node()->hash = other_first.get_hash();

      ++first;
      ++other_first;
    }

    if (first == last) {
      insert(other_first, other_last);
    } else {
      erase(first, last);
    }
  }

  void rehash(size_t count) {
    iterator before_first = before_begin();

    iterator* new_hash_table = allocate_table(allocator_, count);

    if (hash_table_) {
      destroy_table();
      deallocate_table(allocator_, hash_table_, bucket_count_);
    }

    hash_table_ = new_hash_table;
    bucket_count_ = count;

    init_table();

    iterator previous = before_first;
    iterator current = begin();

    while (current != end()) {
      size_t hash = current.get_hash();

      if (previous == before_first || previous.get_hash() != hash) {
        hash_table_[bucket_number(hash)] = previous;
      }

      ++previous;
      ++current;
    }
  }

  auto table_begin() const { return hash_table_; }

  auto table_end() const { return hash_table_ + bucket_count_; }

  void init_table() {
    using Utilities::uninitialized_default_construct;
    HashTableAllocator copy = allocator_;
    uninitialized_default_construct(copy, table_begin(), table_end());
  }

  void destroy_table() {
    HashTableAllocator copy = allocator_;
    Utilities::destroy(copy, table_begin(), table_end());
  }

  void clear() { erase(begin(), end()); }

  void destroy() {
    if (!hash_table_) {
      return;
    }

    clear();
    destroy_table();
    deallocate_table(allocator_, hash_table_, bucket_count_);
  }

  static iterator* allocate_table(Allocator& allocator, size_t n) {
    HashTableAllocator copy = allocator;
    return HashTableAllocatorTrairs::allocate(copy, n);
  }

  static void deallocate_table(Allocator& allocator, iterator* ptr, size_t n) {
    HashTableAllocator copy = allocator;
    HashTableAllocatorTrairs::deallocate(copy, ptr, n);
  }

  static Node* allocate_node(Allocator& allocator) {
    NodeAllocator node_allocator = allocator;
    return allocate_node(node_allocator);
  }

  static void deallocate_node(Allocator& allocator, Node* ptr) {
    NodeAllocator node_allocator = allocator;
    deallocate_node(node_allocator, ptr);
  }

  static Node* allocate_node(NodeAllocator& allocator) {
    return NodeAllocatorTrairs::allocate(allocator, 1);
  }

  static void deallocate_node(NodeAllocator& allocator, Node* ptr) {
    NodeAllocatorTrairs::deallocate(allocator, ptr, 1);
  }
};