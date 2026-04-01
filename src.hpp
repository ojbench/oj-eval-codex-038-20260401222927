#pragma once
#include <cstddef>
#include <memory>
#include <utility>
#include <stdexcept>

namespace sjtu {

template <class T>
class vector {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using reference = value_type &;
  using const_reference = const value_type &;
  using pointer = value_type *;
  using const_pointer = const value_type *;

  vector() noexcept : data_(nullptr), size_(0), cap_(0) {}

  explicit vector(size_type n, const T &value = T()) : data_(nullptr), size_(0), cap_(0) {
    if (n) {
      data_ = alloc_.allocate(n);
      size_type i = 0;
      try {
        for (; i < n; ++i) new (data_ + i) T(value);
      } catch (...) {
        for (size_type j = 0; j < i; ++j) data_[j].~T();
        alloc_.deallocate(data_, n);
        throw;
      }
      size_ = cap_ = n;
    }
  }

  vector(const vector &other) : data_(nullptr), size_(0), cap_(0) {
    if (other.size_) {
      data_ = alloc_.allocate(other.size_);
      size_type i = 0;
      try {
        for (; i < other.size_; ++i) new (data_ + i) T(other.data_[i]);
      } catch (...) {
        for (size_type j = 0; j < i; ++j) data_[j].~T();
        alloc_.deallocate(data_, other.size_);
        throw;
      }
      size_ = cap_ = other.size_;
    }
  }

  vector(vector &&other) noexcept : data_(other.data_), size_(other.size_), cap_(other.cap_) {
    other.data_ = nullptr;
    other.size_ = other.cap_ = 0;
  }

  ~vector() { destroy_and_dealloc(); }

  vector &operator=(const vector &other) {
    if (this == &other) return *this;
    if (other.size_ <= cap_) {
      size_type i = 0;
      // Assign or construct up to other.size_
      for (; i < size_ && i < other.size_; ++i) data_[i] = other.data_[i];
      if (i < other.size_) {
        size_type j = i;
        try {
          for (; j < other.size_; ++j) new (data_ + j) T(other.data_[j]);
        } catch (...) {
          // Rollback newly constructed
          for (size_type k = i; k < j; ++k) data_[k].~T();
          throw;
        }
      } else if (i < size_) {
        // Destroy extra
        for (size_type j = other.size_; j < size_; ++j) data_[j].~T();
      }
      size_ = other.size_;
      return *this;
    }
    vector tmp(other);
    swap(tmp);
    return *this;
  }

  vector &operator=(vector &&other) noexcept {
    if (this == &other) return *this;
    destroy_and_dealloc();
    data_ = other.data_;
    size_ = other.size_;
    cap_ = other.cap_;
    other.data_ = nullptr;
    other.size_ = other.cap_ = 0;
    return *this;
  }

  // element access
  reference operator[](size_type i) noexcept { return data_[i]; }
  const_reference operator[](size_type i) const noexcept { return data_[i]; }
  reference at(size_type i) {
    if (i >= size_) throw std::out_of_range("sjtu::vector::at");
    return data_[i];
  }
  const_reference at(size_type i) const {
    if (i >= size_) throw std::out_of_range("sjtu::vector::at");
    return data_[i];
  }
  reference front() noexcept { return data_[0]; }
  const_reference front() const noexcept { return data_[0]; }
  reference back() noexcept { return data_[size_ - 1]; }
  const_reference back() const noexcept { return data_[size_ - 1]; }
  pointer data() noexcept { return data_; }
  const_pointer data() const noexcept { return data_; }

  // capacity
  bool empty() const noexcept { return size_ == 0; }
  size_type size() const noexcept { return size_; }
  size_type capacity() const noexcept { return cap_; }

  void reserve(size_type new_cap) {
    if (new_cap <= cap_) return;
    reallocate(new_cap);
  }

  void resize(size_type n) { resize(n, T()); }

  void resize(size_type n, const T &value) {
    if (n < size_) {
      for (size_type i = n; i < size_; ++i) data_[i].~T();
      size_ = n;
      return;
    }
    if (n > cap_) {
      size_type newcap = cap_ ? cap_ : 1;
      while (newcap < n) newcap <<= 1;
      reallocate(newcap);
    }
    size_type i = size_;
    try {
      for (; i < n; ++i) new (data_ + i) T(value);
    } catch (...) {
      // roll back constructs
      for (size_type j = size_; j < i; ++j) data_[j].~T();
      throw;
    }
    size_ = n;
  }

  // modifiers
  void clear() noexcept {
    for (size_type i = 0; i < size_; ++i) data_[i].~T();
    size_ = 0;
  }

  void push_back(const T &value) {
    ensure_capacity_for_one();
    new (data_ + size_) T(value);
    ++size_;
  }

  void push_back(T &&value) {
    ensure_capacity_for_one();
    new (data_ + size_) T(std::move(value));
    ++size_;
  }

  template <class... Args>
  reference emplace_back(Args &&...args) {
    ensure_capacity_for_one();
    new (data_ + size_) T(std::forward<Args>(args)...);
    ++size_;
    return back();
  }

  void pop_back() {
    if (size_ == 0) return;
    --size_;
    data_[size_].~T();
  }

  void swap(vector &other) noexcept {
    using std::swap;
    swap(data_, other.data_);
    swap(size_, other.size_);
    swap(cap_, other.cap_);
  }

 private:
  std::allocator<T> alloc_{};
  T *data_;
  size_type size_;
  size_type cap_;

  void ensure_capacity_for_one() {
    if (size_ == cap_) {
      size_type newcap = cap_ ? (cap_ << 1) : 1;
      reallocate(newcap);
    }
  }

  void reallocate(size_type newcap) {
    T *newdata = alloc_.allocate(newcap);
    size_type i = 0;
    try {
      for (; i < size_; ++i) new (newdata + i) T(std::move_if_noexcept(data_[i]));
    } catch (...) {
      for (size_type j = 0; j < i; ++j) newdata[j].~T();
      alloc_.deallocate(newdata, newcap);
      throw;
    }
    for (size_type j = 0; j < size_; ++j) data_[j].~T();
    if (data_) alloc_.deallocate(data_, cap_);
    data_ = newdata;
    cap_ = newcap;
  }

  void destroy_and_dealloc() noexcept {
    for (size_type i = 0; i < size_; ++i) data_[i].~T();
    if (data_) alloc_.deallocate(data_, cap_);
    data_ = nullptr; size_ = cap_ = 0;
  }
};

}  // namespace sjtu

