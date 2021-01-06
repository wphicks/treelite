#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <vector>

namespace treelite {
namespace containers {

/*!
 * \brief "Hash" used to pass through keys that are already valid hashes
 */
template <typename Key>
struct PassHash {
  size_t operator()(const Key& key) { return static_cast<size_t>(key); }
};

template <typename Key, typename T>
struct FastMapEntry {
  Key key;
  T value;
};

/*!
 * \brief Fast hash table with open addressing and linear probing
 */
template <typename Key, typename T, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator =
              std::allocator<std::pair<std::pair<Key, T>, bool> > >
class FastMap {
 public:
  typedef Key key_type;
  typedef T mapped_type;
  typedef std::pair<const Key, T> value_type;
  typedef size_t size_type;

  FastMap(const size_type hinted_size)
      : data{}, size_hint_{hinted_size}, hash{}, key_equal{}, size_{0} {
    data.reserve(size_hint_);
  };
  FastMap() : FastMap(2048){};  // Default to 2048 entries as target size
  auto get_allocator() { return data.get_allocator(); }

  // Iterators
  template <typename base_iter_type>
  struct Iterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, T&>;
    using pointer = value_type*;
    using reference = value_type;

    Iterator(base_iter_type wrapped_iter, base_iter_type wrapped_end)
        : base_iter{wrapped_iter}, base_end{wrapped_end} {}

    reference operator*() const {
      return value_type(base_iter->first.first, base_iter->first.second);
    }
    pointer operator->() const { return (pointer) & (base_iter->first); }

    Iterator& operator++() {
      while (base_iter != base_end) {
        ++base_iter;
        if (base_iter->second) {
          return *this;
        }
      }
      return *this;
    }
    Iterator operator++(int) {
      Iterator tmp(base_iter, base_end);
      ++(*this);
      return tmp;
    }

    friend bool operator==(const Iterator& a, const Iterator& b) {
      return a.base_iter == b.base_iter;
    }
    friend bool operator!=(const Iterator& a, const Iterator& b) {
      return a.base_iter != b.base_iter;
    }

   private:
    base_iter_type base_iter;
    base_iter_type base_end;
  };

  auto begin() {
    return Iterator<typename std::vector<value_type_>::iterator>(data.begin(),
                                                                 data.end());
  }
  /* auto cbegin() {
    return Iterator<typename std::vector<value_type_>::const_iterator>(
        data.cbegin(), data.cend());
  } */
  auto end() {
    return Iterator<typename std::vector<value_type_>::iterator>(data.end(),
                                                                 data.end());
  }
  /* auto cend() {
    return Iterator<typename std::vector<value_type_>::const_iterator>(
        data.cend(), data.cend());
  } */

  // Capacity
  auto empty() { return size_ == 0; }
  auto size() { return size_; }
  auto max_size() { return data.max_size(); }
  auto size_hint() { return size_hint_; }

  // Modifiers
  void clear() {
    data.clear();
    size_ = 0;
  }

  size_type erase(const key_type& key) {
    const size_type offset = hash(key) % size_hint_;
    size_type loc = data.size();
    size_type i = offset;
    for (size_type i = offset; i < data.size(); ++i) {
      if (data[i].second) {
        if (key_equal(key, data[i].first.first)) {
          loc = i;
          break;
        }
      } else {
        return 0;
      }
    }
    if (loc == data.size()) {
      return 0;
    }

    data[loc].second = false;  // Mark entry as invalid
    --size_;
    auto data_begin = data.begin();
    // Move "empty" entry ahead to end of nearest non-full segment
    for (size_type i = loc + 1; i < data.size(); ++i) {
      if (data[i].second && hash(data[i].first.first) % size_hint_ > i) {
        std::iter_swap(data_begin + i - 1, data_begin + i);
      } else {
        break;
      }
    }
    return 1;
  }

  // Lookup
  T& operator[](const key_type& key) {
    const size_type offset = hash(key) % size_hint_;
    if (offset >= data.size()) {
      while (data.size() <= offset) {
        data.emplace_back();
      }
      data.back().first.first = key;
      data.back().second = true;
      ++size_;
      return data.back().first.second;
    }

    /* Find the entry matching the given key. If this key has not been used,
     * set the key of the nearest empty entry to the target location to that
     * key and mark the entry as valid */
    int empty_loc = data.size();
    for (size_type i = offset; i < data.size(); ++i) {
      if (key_equal(key, data[i].first.first)) {
        return data[i].first.second;
      } else if (!data[i].second && empty_loc == data.size()) {
        empty_loc = i;
      }
    }

    if (empty_loc == data.size()) {
      data.emplace_back();
    }

    if (!data[empty_loc].second) {
      ++size_;
      data[empty_loc].second = true;
    }
    data[empty_loc].first.first = key;
    return data[empty_loc].first.second;
  }

  T& at(const key_type& key) {
    const size_type offset = hash(key) % size_hint_;

    auto entry = find_if(
        data.begin() + offset, data.end(), [&](const value_type_& entry_) {
          return entry_.second && key_equal(key, entry_.first.first);
        });
    if (entry == data.end()) {
      throw std::out_of_range("invalid key");
    } else {
      return entry->first.second;
    }
  }

  size_type count(const key_type& key) {
    const size_type offset = hash(key) % size_hint_;

    for (size_type i = offset; i < data.size(); ++i) {
      if (data[i].second) {
        if (key_equal(key, data[i].first.first)) {
          return 1;
        }
      } else {
        return 0;
      }
    }

    return 0;
  }

 private:
  const size_type size_hint_;
  size_type size_;
  typedef std::pair<std::pair<Key, T>, bool> value_type_;
  /* value_type_ provides a bool field to determine whether any given entry was
   * default-constructed (default of false) or explicitly set (true).  This
   * provides a type-independent way to distinguish placeholder entries in the
   * underlying vector from inserted ones at the cost of one byte per entry*/
  std::vector<value_type_, Allocator> data;
  Hash hash;
  KeyEqual key_equal;
};

} // namespace containers
} // namespace treelite
