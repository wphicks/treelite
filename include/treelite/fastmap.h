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
  typedef std::pair<Key, T> value_type;  // TODO return keys const
  typedef size_t size_type;

  FastMap(const size_type requested_capacity)
      : data{}, initial_capacity{requested_capacity}, hash{}, key_equal{} {
    data.reserve(requested_capacity);
  };
  FastMap() : FastMap(2048){};  // Default to 2048 entries as target size
  auto get_allocator() { return data.get_allocator(); }

  // Iterators
  template <typename base_iter_type>
  struct Iterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    Iterator(base_iter_type wrapped_iter, base_iter_type wrapped_end)
        : base_iter{base_end} {}

    reference operator*() const { return base_iter->first; }
    pointer operator->() const { return &(base_iter->first); }

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
  auto cbegin() {
    return Iterator<typename std::vector<value_type_>::const_iterator>(
        data.cbegin(), data.cend());
  }
  auto end() {
    return Iterator<typename std::vector<value_type_>::iterator>(data.end(),
                                                                 data.end());
  }
  auto cend() {
    return Iterator<typename std::vector<value_type_>::const_iterator>(
        data.cend(), data.cend());
  }

  // Capacity
  auto empty() { return data.empty(); }
  auto size() { return data.size(); }
  auto max_size() { return data.max_size(); }

  // Modifiers
  void clear() { data.clear(); }

  size_t erase(const key_type& key) {
    const size_t offset = hash(key) % initial_capacity;
    size_t loc = data.size();
    size_t i = offset;
    for (size_t i = offset; i < data.size(); ++i) {
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
    auto data_begin = data.begin();
    // Move "empty" entry ahead to end of nearest non-full segment
    for (size_t i = loc + 1; i < data.size(); ++i) {
      if (data[i].second && hash(data[i].first.first) % initial_capacity > i) {
        std::iter_swap(data_begin + i - 1, data_begin + i);
      } else {
        break;
      }
    }
    return 1;
  }

  // Lookup
  T& operator[](const key_type& key) {
    const size_t offset = hash(key) % initial_capacity;
    if (offset > data.size()) {
      size_t i = offset;
      while (i > data.size()) {
        --i;
        data.emplace_back();
      }
      data.back().second = true;
      return data.back().first.second;
    }

    /* Find the entry matching the given key. If this key has not been used,
     * set the key of the nearest empty entry to the target location to that
     * key and mark the entry as valid */
    for (auto iter = data.begin() + offset; iter != data.end(); ++iter) {
      if (key_equal(key, (iter->first).first)) {
        return (iter->first).second;
      } else if (!iter->second) {
        (iter->first).first = key_type{key};
        iter->second = true;
        return (iter->first).second;
      }
    };

    data.emplace_back();
    return data.back().first.second;
  }

  T& at(const key_type& key) {
    const size_t offset = hash(key) % initial_capacity;

    for (size_t i = offset; i < data.size(); ++i) {
      if (data[i].second) {
        if (key_equal(key, data[i].first.first)) {
          return data[i].first.second;
        }
      } else {
        throw std::out_of_range("invalid key");
      }
    }

    throw std::out_of_range("invalid key");
  }

  size_t count(const key_type& key) {
    const size_t offset = hash(key) % initial_capacity;

    for (size_t i = offset; i < data.size(); ++i) {
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
  const size_t initial_capacity;
  typedef std::pair<value_type, bool> value_type_;
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
