#include <memory>
#include <utility>

namespace Implementation {

namespace Utilities {

template <typename T>
struct AlignedBuffer {
  alignas(T) std::byte storage[sizeof(T)];

  void* address() { return static_cast<void*>(storage); }

  const void* address() const { return static_cast<const void*>(storage); }

  T* pointer() { return static_cast<T*>(address()); }

  const T* pointer() const { return static_cast<const T*>(address()); }
};

template <typename T, typename Allocator>
class AllocatorHelper {
 private:
  using AllocatorTraits = std::allocator_traits<Allocator>;
  using TAllocator = typename AllocatorTraits::template rebind_alloc<T>;
  using TAllocatorTraits = typename AllocatorTraits::template rebind_traits<T>;

 public:
  static T* allocate(const Allocator& allocator, size_t n) {
    TAllocator copy = allocator;
    return TAllocatorTraits::allocate(copy, n);
  }

  static void deallocate(const Allocator& allocator, T* ptr, size_t n) {
    TAllocator copy = allocator;
    TAllocatorTraits::deallocate(copy, ptr, n);
  }

  template <typename... Args>
  static void construct(const Allocator& allocator, T* ptr, Args&&... args) {
    TAllocator copy = allocator;
    TAllocatorTraits::construct(copy, ptr, std::forward<Args>(args)...);
  }

  static void destroy(const Allocator& allocator, T* ptr) {
    TAllocator copy = allocator;
    TAllocatorTraits::destroy(copy, ptr);
  }
};

}  // namespace Utilities

template <typename T>
class WeakPtr;

struct CountedBase {
  size_t use_count = 1;
  size_t weak_count = 1;

  virtual ~CountedBase() = default;

  virtual void dispose() = 0;

  virtual void destroy() = 0;
};

template <typename T, typename Deleter, typename Allocator>
struct CountedPtr : public CountedBase, private Deleter, private Allocator {
  using ThisAllocator = Utilities::AllocatorHelper<CountedPtr, Allocator>;

  T* ptr;

  CountedPtr(T* ptr, Deleter deleter, Allocator allocator)
      : Deleter(std::move(deleter)),
        Allocator(std::move(allocator)),
        ptr(ptr) {}

  void dispose() override {
    Deleter& deleter = *this;
    deleter(ptr);
  }

  void destroy() override {
    using AllocatorTraits = std::allocator_traits<Allocator>;
    using ThisAllocator =
        typename AllocatorTraits::template rebind_alloc<CountedPtr>;
    using ThisAllocatorTraits =
        typename AllocatorTraits::template rebind_traits<CountedPtr>;

    ThisAllocator allocator = static_cast<Allocator&&>(*this);
    std::destroy_at(this);
    ThisAllocatorTraits::deallocate(allocator, this, 1);
  }
};

template <typename T, typename Allocator>
struct CountedAllocator : public CountedBase, private Allocator {
  using ThisAllocator = Utilities::AllocatorHelper<CountedAllocator, Allocator>;
  using Builder = Utilities::AllocatorHelper<T, Allocator>;

  Utilities::AlignedBuffer<T> storage;

  CountedAllocator(Allocator allocator) : Allocator(std::move(allocator)) {}

  void dispose() override { Builder::destroy(*this, storage.pointer()); }

  void destroy() override {
    using AllocatorTraits = std::allocator_traits<Allocator>;
    using ThisAllocator =
        typename AllocatorTraits::template rebind_alloc<CountedAllocator>;
    using ThisAllocatorTraits =
        typename AllocatorTraits::template rebind_traits<CountedAllocator>;

    ThisAllocator allocator = static_cast<Allocator&&>(*this);
    std::destroy_at(this);
    ThisAllocatorTraits::deallocate(allocator, this, 1);
  }
};

template <typename T>
class SharedPtr {
 public:
  SharedPtr() = default;

  SharedPtr(std::nullptr_t) {}

  template <typename Deleter>
  SharedPtr(std::nullptr_t, Deleter) {}

  template <typename Y, typename Deleter = std::default_delete<Y>,
            typename Allocator = std::allocator<T>>
  SharedPtr(Y* ptr, Deleter deleter = Deleter(),
            Allocator allocator = Allocator())
      : ptr_(ptr) {
    using Counter = CountedPtr<T, Deleter, Allocator>;
    using CounterAllocator = typename Counter::ThisAllocator;

    try {
      Counter* counter = CounterAllocator::allocate(allocator, 1);
      std::construct_at(counter, ptr, std::move(deleter), std::move(allocator));
      counter_ = counter;
    } catch (...) {
      deleter(ptr);
      throw;
    }
  }

  template <typename Deleter, typename Allocator>
  SharedPtr(std::nullptr_t, Deleter, Allocator) {}

  template <typename Y>
  SharedPtr(const SharedPtr<Y>& other, T* ptr)
      : SharedPtr(ptr, other.counter_) {
    if (counter_) {
      ++counter_->use_count;
    }
  }

  template <typename Y>
  SharedPtr(SharedPtr<Y>&& other, T* ptr) : SharedPtr(other, ptr) {
    other.reset();
  }

  SharedPtr(const SharedPtr& other) : SharedPtr(other, other.ptr_) {}

  template <typename Y>
  SharedPtr(const SharedPtr<Y>& other) : SharedPtr(other, other.ptr_) {}

  SharedPtr(SharedPtr&& other) : SharedPtr(std::move(other), other.ptr_) {}

  template <typename Y>
  SharedPtr(SharedPtr<Y>&& other) : SharedPtr(std::move(other), other.ptr_) {}

  template <class Y>
  explicit SharedPtr(const WeakPtr<Y>& r);

  ~SharedPtr() {
    if (!counter_) {
      return;
    }

    if (--counter_->use_count == 0) {
      counter_->dispose();

      if (--counter_->weak_count == 0) {
        counter_->destroy();
      }
    }
  }

  size_t use_count() const { return counter_ ? counter_->use_count : 0; }

  T* get() const { return ptr_; }

  void swap(SharedPtr& other) {
    std::swap(ptr_, other.ptr_);
    std::swap(counter_, other.counter_);
  }

  void reset() { SharedPtr().swap(*this); }

  template <typename Y>
  void reset(Y* ptr) {
    SharedPtr<T>(ptr).swap(*this);
  }

  T& operator*() const { return *get(); }

  T* operator->() const { return get(); }

  SharedPtr& operator=(const SharedPtr& other) {
    assign(other);
    return *this;
  }

  template <typename Y>
  SharedPtr& operator=(const SharedPtr<Y>& other) {
    assign(other);
    return *this;
  }

  SharedPtr& operator=(SharedPtr&& other) {
    assign(std::move(other));
    return *this;
  }

  template <typename Y>
  SharedPtr& operator=(SharedPtr<Y>&& other) {
    assign(std::move(other));
    return *this;
  }

  template <typename Y>
  friend class SharedPtr;

  template <typename Y>
  friend class WeakPtr;

  template <typename Y, typename Allocator, typename... Args>
  friend SharedPtr<Y> allocateShared(const Allocator&, Args&&...);

 private:
  T* ptr_ = nullptr;
  CountedBase* counter_ = nullptr;

  explicit SharedPtr(T* ptr, CountedBase* counter)
      : ptr_(ptr), counter_(counter) {}

  template <typename Y>
  void assign(const SharedPtr<Y>& other) {
    SharedPtr<T>(other).swap(*this);
  }

  template <typename Y>
  void assign(SharedPtr<Y>&& other) {
    SharedPtr<T>(std::move(other)).swap(*this);
  }
};

template <typename T, typename Allocator, typename... Args>
SharedPtr<T> allocateShared(const Allocator& allocator, Args&&... args) {
  using Counter = CountedAllocator<T, Allocator>;
  using CounterAllocator = typename Counter::ThisAllocator;

  Counter* counter = CounterAllocator::allocate(allocator, 1);
  std::construct_at(counter, allocator);

  T* ptr = counter->storage.pointer();

  try {
    Counter::Builder::construct(allocator, ptr, std::forward<Args>(args)...);
  } catch (...) {
    counter->destroy();
    throw;
  }

  return SharedPtr<T>(ptr, static_cast<CountedBase*>(counter));
}

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
  return allocateShared<T>(std::allocator<T>(), std::forward<Args>(args)...);
}

template <typename T>
class WeakPtr {
 public:
  WeakPtr() = default;

  WeakPtr(const WeakPtr& other) { construct_from(other); }

  template <typename Y>
  WeakPtr(const WeakPtr<Y>& other) {
    construct_from(other);
  }

  WeakPtr(WeakPtr&& other) {
    construct_from(other);
    other.reset();
  }

  template <typename Y>
  WeakPtr(WeakPtr<Y>&& other) {
    construct_from(other);
    other.reset();
  }

  template <typename Y>
  WeakPtr(const SharedPtr<Y>& other) {
    construct_from(other);
  }

  ~WeakPtr() {
    if (counter_ && --counter_->weak_count == 0) {
      counter_->destroy();
    }
  }

  bool expired() const { return use_count() == 0; }

  size_t use_count() const { return counter_ ? counter_->use_count : 0; }

  SharedPtr<T> lock() const {
    return expired() ? SharedPtr<T>() : SharedPtr<T>(*this);
  }

  void swap(WeakPtr& other) {
    std::swap(ptr_, other.ptr_);
    std::swap(counter_, other.counter_);
  }

  void reset() { WeakPtr().swap(*this); }

  WeakPtr& operator=(const WeakPtr& other) {
    assign(other);
    return *this;
  }

  template <typename Y>
  WeakPtr& operator=(const WeakPtr<Y>& other) {
    assign(other);
    return *this;
  }

  template <typename Y>
  WeakPtr& operator=(const SharedPtr<Y>& other) {
    assign(other);
    return *this;
  }

  WeakPtr& operator=(WeakPtr&& other) {
    assign(std::move(other));
    return *this;
  }

  template <typename Y>
  WeakPtr& operator=(WeakPtr<Y>&& other) {
    assign(std::move(other));
    return *this;
  }

  template <typename Y>
  friend class SharedPtr;

  template <typename Y>
  friend class WeakPtr;

 private:
  T* ptr_ = nullptr;
  CountedBase* counter_ = nullptr;

  template <typename SmartPointer>
  void construct_from(const SmartPointer& smart_pointer) {
    ptr_ = smart_pointer.ptr_;
    counter_ = smart_pointer.counter_;

    if (counter_) {
      ++counter_->weak_count;
    }
  }

  template <typename SmartPointer>
  void assign(SmartPointer&& smart_pointer) {
    WeakPtr<T>(std::forward<SmartPointer>(smart_pointer)).swap(*this);
  }
};

template <typename T>
template <class Y>
SharedPtr<T>::SharedPtr(const WeakPtr<Y>& weak)
    : SharedPtr(weak.ptr_, weak.counter_) {
  if (counter_ && counter_->use_count != 0) {
    ++counter_->use_count;
  } else {
    ptr_ = nullptr;
    counter_ = nullptr;
  }
}

template <typename T>
class EnableSharedFromThis {
 public:
  SharedPtr<T> shared_from_this() { return SharedPtr<T>(weak_this_); }

 private:
  WeakPtr<T> weak_this_;
};

}  // namespace Implementation

using Implementation::allocateShared;
using Implementation::EnableSharedFromThis;
using Implementation::makeShared;
using Implementation::SharedPtr;
using Implementation::WeakPtr;