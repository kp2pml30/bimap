#pragma once

#include "splay.h"
#include <cstddef>
#include <iterator>
#include <type_traits>

template <typename Left, typename Right, typename CompareLeft,
          typename CompareRight>
struct bimap;

namespace bimap_helper {
template <typename Left, typename Right>
struct node_t : splay::splay_holder<Left>,
                splay::splay_holder<Right, splay::default_tag2_t<Right>> {
  using left_holder = splay::splay_holder<Left>;
  using right_holder = splay::splay_holder<Right, splay::default_tag2_t<Right>>;

  using left_t = Left;
  using right_t = Right;

  template <typename T,
            typename = std::enable_if_t<std::is_same_v<T, left_holder> ||
                                        std::is_same_v<T, right_holder>>>
  constexpr static node_t const *cast(T const *a) noexcept {
    return static_cast<node_t const *>(a);
  }

  template <typename T1, typename T2>
  node_t(T1 &&l, T2 &&r)
      : left_holder(std::forward<T1>(l)), right_holder(std::forward<T2>(r)) {}

  left_holder const *left_node() const {
    return static_cast<left_holder const *>(this);
  }
  right_holder const *right_node() const {
    return static_cast<right_holder const *>(this);
  }
};

template <typename node_t, typename storage_type>
using coholder_t = std::conditional_t<
    std::is_same_v<storage_type, typename node_t::left_holder>,
    typename node_t::right_holder, typename node_t::left_holder>;

template <typename node_t, typename storage_type> struct bimap_iterator {
  node_t const *const *root;
  node_t const *node;

  using value_type = typename storage_type::value_type;
  using pointer_type = value_type const *;
  using reference_type = value_type const &;
  using iterator_category = std::bidirectional_iterator_tag;

  template <typename, typename, typename, typename> friend class bimap;

  bimap_iterator() = default;
  bimap_iterator(decltype(root) root, node_t const *node) noexcept
      : root(root), node(node) {}

  pointer_type operator->() const { return &node->storage_type::data; }
  reference_type operator*() const noexcept { return *operator->(); }

  bimap_iterator &operator++() noexcept {
    node = node_t::cast(node->storage_type::call(&storage_type::node_t::next));

    return *this;
  }
  bimap_iterator operator++(int) noexcept {
    auto copy = *this;
    operator++();
    return copy;
  }

  bimap_iterator &operator--() noexcept {
    if (node == nullptr) {
      auto rt = static_cast<storage_type const *>(*root);
      rt->splay();
      return rt->call(&storage_type::node_t::right_most);
    }
    node = node_t::cast(node->storage_type::call(&storage_type::node_t::prev));

    return *this;
  }
  bimap_iterator operator--(int) noexcept {
    auto copy = *this;
    operator--();
    return copy;
  }

  auto flip() const noexcept {
    return bimap_iterator<node_t, coholder_t<node_t, storage_type>>(root, node);
  }

  bool operator==(bimap_iterator const &r) const noexcept {
    return node == r.node;
  }
  bool operator!=(bimap_iterator const &r) const noexcept {
    return !operator==(r);
  }
};

// for zero base optimization
template <typename T, typename Tag = splay::default_tag_t<T>>
struct tagged_comparator : public T {
  tagged_comparator() = default;

  explicit tagged_comparator(T const &t) noexcept(
      std::is_nothrow_constructible_v<T, T const &>)
      : T(t) {}

  explicit tagged_comparator(T &&t) noexcept(
      std::is_nothrow_constructible_v<T, T &&>)
      : T(std::move(t)) {}

  explicit constexpr operator T const &() const noexcept {
    return static_cast<T const &>(*this);
  }
};
} // namespace bimap_helper
