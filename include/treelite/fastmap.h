#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <utility>
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
  std::pair<Key, T> entry;
  bool valid;
  /* FastMapEntry() : entry{}, valid(false){};
  FastMapEntry(std::pair<Key, T> new_entry, bool new_validity)
      : entry{new_entry}, valid{new_validity} {};
  FastMapEntry(FastMapEntry<Key, T>&& other)
      : entry{std::move(other.entry.first), std::move(other.entry.second)},
        valid{other.valid} {};
  FastMapEntry& operator=(FastMapEntry<Key, T>&& other) {
    entry = std::make_pair(std::move(other.entry.first),
                           std::move(other.entry.second));
    valid = other.valid;
    return *this;
  }; */
};

/*!
 * \brief Fast hash table with open addressing and linear probing
 */
template <typename Key, typename T, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<FastMapEntry<Key, T> > >
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
  template <typename U>
  struct Iterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::pair<const Key, U&>;
    using wrapped_iter_type =
        typename std::vector<FastMapEntry<Key, T>>::iterator;

    Iterator(wrapped_iter_type base_iter, wrapped_iter_type base_end)
        : wrapped_iter{base_iter}, wrapped_end{base_end} {};

    auto operator*() {
      return value_type(wrapped_iter->entry.first, wrapped_iter->entry.second);
    }
    auto operator->() {
      return std::make_shared<value_type>(wrapped_iter->entry.first,
                                          wrapped_iter->entry.second);
    }

    Iterator& operator++() {
      while (wrapped_iter != wrapped_end) {
        ++wrapped_iter;
        if (wrapped_iter->valid) {
          return *this;
        }
      }
      return *this;
    }
    Iterator operator++(int) {
      Iterator tmp(wrapped_iter, wrapped_end);
      ++(*this);
      return tmp;
    }

    friend bool operator==(const Iterator& a, const Iterator& b) {
      return a.wrapped_iter == b.wrapped_iter;
    }
    friend bool operator!=(const Iterator& a, const Iterator& b) {
      return a.wrapped_iter != b.wrapped_iter;
    }

   private:
    wrapped_iter_type wrapped_iter;
    wrapped_iter_type wrapped_end;
  };

  auto begin() { return Iterator<mapped_type>(data.begin(), data.end()); }
  /* auto cbegin() {
    return Iterator<typename std::vector<entry_type_>::const_iterator>(
        data.cbegin(), data.cend());
  } */
  auto end() { return Iterator<mapped_type>(data.end(), data.end()); }
  /* auto cend() {
    return Iterator<typename std::vector<entry_type_>::const_iterator>(
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
      if (data[i].valid) {
        if (key_equal(key, data[i].entry.first)) {
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

    data[loc].valid = false;  // Mark entry as invalid
    --size_;
    auto data_begin = data.begin();
    // Move "empty" entry ahead to end of nearest non-full segment
    for (size_type i = loc + 1; i < data.size(); ++i) {
      if (data[i].valid && hash(data[i].entry.first) % size_hint_ > i) {
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
      data.back().entry.first = key;
      data.back().valid = true;
      ++size_;
      return data.back().entry.second;
    }

    /* Find the entry matching the given key. If this key has not been used,
     * set the key of the nearest empty entry to the target location to that
     * key and mark the entry as valid */
    int empty_loc = data.size();
    for (size_type i = offset; i < data.size(); ++i) {
      if (key_equal(key, data[i].entry.first)) {
        return data[i].entry.second;
      } else if (!data[i].valid && empty_loc == data.size()) {
        empty_loc = i;
      }
    }

    if (empty_loc == data.size()) {
      data.emplace_back();
    }

    if (!data[empty_loc].valid) {
      ++size_;
      data[empty_loc].valid = true;
    }
    data[empty_loc].entry.first = key;
    return data[empty_loc].entry.second;
  }

  T& at(const key_type& key) {
    auto entry = find(key);
    if (entry == this->end()) {
      throw std::out_of_range("invalid key");
    } else {
      return entry->second;
    }
  }

  auto find(const key_type& key) {
    const size_type offset = hash(key) % size_hint_;

    auto entry = find_if(
        data.begin() + offset, data.end(), [&](const entry_type_& entry_) {
          return entry_.valid && key_equal(key, entry_.entry.first);
        });
    return Iterator<mapped_type>(entry, data.end());
  }

  size_type count(const key_type& key) {
    const size_type offset = hash(key) % size_hint_;

    for (size_type i = offset; i < data.size(); ++i) {
      if (data[i].valid) {
        if (key_equal(key, data[i].entry.first)) {
          return 1;
        }
      } else {
        return 0;
      }
    }

    return 0;
  }

 private:
  size_type size_hint_;
  size_type size_;
  typedef FastMapEntry<key_type, mapped_type> entry_type_;
  typedef std::pair<key_type, mapped_type> value_type_;
  std::vector<entry_type_, Allocator> data;
  Hash hash;
  KeyEqual key_equal;
};

} // namespace containers
} // namespace treelite
