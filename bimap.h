#pragma once

#include <functional>
#include <stdexcept>
#include <exception>

#include <iostream>
#include <type_traits>
#include <utility>

#include "bimap-helper.h"
#include "splay.h"

/**
 * allocation exception is treated as noexcept
 */
template <typename Left, typename Right,
          typename CompareLeft = std::less<Left>,
          typename CompareRight = std::less<Right>>
struct bimap
: private bimap_helper::tagged_comparator<CompareLeft>
, private bimap_helper::tagged_comparator<CompareRight, splay::default_tag2_t<CompareRight>>
{
  using left_t = Left;
  using right_t = Right;

private:
	using node_t = bimap_helper::node_t<Left, Right>;
	using left_comparator_holder = bimap_helper::tagged_comparator<CompareLeft>;
	using right_comparator_holder = bimap_helper::tagged_comparator<CompareRight, splay::default_tag2_t<CompareRight>>;
public:
	using left_iterator = bimap_helper::bimap_iterator<node_t, typename node_t::left_holder>;
	using right_iterator = bimap_helper::bimap_iterator<node_t, typename node_t::right_holder>;

private:
	node_t const* root;
	std::size_t sz;

	CompareLeft const& left_comparator() const noexcept
	{
		return static_cast<CompareLeft const&>(static_cast<left_comparator_holder const&>(*this));
	}
	CompareRight const& right_comparator() const noexcept
	{
		return static_cast<CompareRight const&>(static_cast<right_comparator_holder const&>(*this));
	}

	void copy_elements(bimap const& other) noexcept(noexcept(insert(std::declval<left_t const&>(), std::declval<right_t const&>())))
	{
		if (this == &other)
			return;
		clear();
		// iterating over all elements of splay tree is O(n), insert biggest would be O(1) here + rebalance in fututre
		for (auto iter = other.begin_left(); iter != other.end_left(); ++iter)
			insert(*iter, *iter.flip());
	}
public:
  bimap()
	: left_comparator_holder()
	, right_comparator_holder()
	, root(nullptr)
	, sz(0)
	{}
  bimap(CompareLeft const& cl, CompareRight const& cr)
	: left_comparator_holder(cl)
	, right_comparator_holder(cr)
	, root(nullptr)
	, sz(0)
	{}
  bimap(CompareLeft&& cl, CompareRight&& cr) noexcept
	: left_comparator_holder(std::move(cl))
	, right_comparator_holder(std::move(cr))
	, root(nullptr)
	, sz(0)
	{}

  bimap(bimap const &other) : bimap(other.left_comparator(), other.right_comparator())
	{
		copy_elements(other);
	}
  bimap(bimap &&other) noexcept : root(other.root), sz(other.sz)
	{
		other.root = nullptr;
		other.sz = 0;
	}

  bimap& operator=(bimap const &other) noexcept(noexcept(copy_elements(other)))
	{
		copy_elements(other);
		return *this;
	}
  bimap& operator=(bimap &&other) noexcept
	{
		std::swap(root, other.root);
		std::swap(sz, other.sz);
		return *this;
	}

	void clear() noexcept
	{
		if (size() == 0)
			return;
		// iterating over splay tree in increasing order is O(n)
		// Static Finger Theorem
		auto iter = begin_left();
		while (true)
		{
			auto del = iter;
			++iter;
			delete del.node;
			if (iter == end_left())
				break;
			assert(iter.node->left_node()->up == nullptr);
			iter.node->left_node()->left = nullptr;
		}
		root = nullptr;
		sz = 0;
	}
  ~bimap() noexcept
	{
		clear();
	}

  left_iterator begin_left() const noexcept
	{
		if (root == nullptr)
			return left_iterator(&root, nullptr);
		auto rt = root->left_node();
		rt->splay();
		return left_iterator(&root, node_t::cast(rt->call(&std::remove_pointer_t<decltype(rt)>::left_most)));
	}
  left_iterator end_left() const noexcept
	{
		return left_iterator(&root, nullptr);
	}

  right_iterator begin_right() const noexcept
	{
		if (root == nullptr)
			return right_iterator(&root, nullptr);
		auto rt = root->right_node();
		rt->splay();
		return right_iterator(&root, node_t::cast(rt->call(&std::remove_pointer_t<decltype(rt)>::left_most)));
	}
  right_iterator end_right() const noexcept
	{
		return right_iterator(&root, nullptr);
	}

	template<typename T1, typename T2>
	left_iterator insert(T1&& l, T2&& r)
	noexcept(std::is_nothrow_constructible_v<T1, T1&&> && std::is_nothrow_constructible_v<T2, T2&&>)
	{
		if (root == nullptr)
		{
			root = new node_t(std::forward<T1>(l), std::forward<T2>(r));
			sz = 1;
			return left_iterator(&root, root);
		}

		auto fl = root->left_node()->find_ge(l, left_comparator());
		if (fl != nullptr && !left_comparator()(l, fl->data))
			return end_left();
		auto fr = root->right_node()->find_ge(r, right_comparator());
		if (fr != nullptr && !right_comparator()(r, fr->data))
			return end_left();

		auto node = new node_t(std::forward<T1>(l), std::forward<T2>(r));

		sz++;

		typename node_t::left_holder::node_t const* mll;
		decltype(mll) mrl;
		if (fl == nullptr)
		{
			mll = root->left_node()->as_node();
			mrl = nullptr;
		}
		else
		{
			auto res = fl->cut();
			mll = res.first;
			mrl = res.second;
		}

		typename node_t::right_holder::node_t const* mlr;
		decltype(mlr) mrr;
		if (fr == nullptr)
		{
			mlr = root->right_node()->as_node();
			mrr = nullptr;
		}
		else
		{
			auto res = fr->cut();
			mlr = res.first;
			mrr = res.second;
		}
		node->right_node()->merge(mlr, mrr);
		node->left_node()->merge(mll, mrl);

		root = node;

		return left_iterator(&root, node);
	}

	template<typename holder_t>
  auto erase_impl(bimap_helper::bimap_iterator<node_t, holder_t> it) noexcept -> decltype(it)
	{
		auto ret = it;
		++ret;

		static_cast<bimap_helper::coholder_t<node_t, holder_t> const*>(it.node)->cutcutmerge();
		root = node_t::cast(static_cast<holder_t const*>(it.node)->call(&holder_t::node_t::cutcutmerge));
		sz--;
		delete it.node;
		return ret;
	}

  left_iterator erase_left(left_iterator it) noexcept
	{
		return erase_impl(it);
	}
  right_iterator erase_right(right_iterator it) noexcept
	{
		return erase_impl(it);
	}

  left_iterator find_left(left_t const &left) const
	noexcept(is_nothrow_comparable_v<left_t,CompareLeft>)
	{
		auto found = root->left_node()->find_ge(left, left_comparator());
		// found >= left
		if (found != nullptr && left_comparator()(left, found->data))
			return end_left();
		return left_iterator(&root, node_t::cast(found));
	}
  right_iterator find_right(right_t const &right) const
	noexcept(is_nothrow_comparable_v<right_t,CompareRight>)
	{
		auto found = root->right_node()->find_ge(right, right_comparator());
		if (found != nullptr && right_comparator()(right, found->data))
			return end_right();
		return right_iterator(&root, node_t::cast(found));
	}

  bool erase_left(left_t const &left) noexcept(noexcept(find_left(left)))
	{
		auto found = find_left(left);
		if (found == end_left())
			return false;
		erase_left(found);
		return true;
	}
  bool erase_right(right_t const &right) noexcept(noexcept(find_right(right)))
	{
		auto found = find_right(right);
		if (found == end_right())
			return false;
		erase_right(found);
		return true;
	}

	template<typename T>
	T erase_range(T f, T l) noexcept
	{
		// optimize?
		// because of dynamic finger theroem it is not so bad as it is
		while (f != l)
			f = erase_impl(f);
		return f;
	}
	left_iterator erase_left(left_iterator f, left_iterator l) noexcept
	{
		return erase_range(f, l);
	}
	right_iterator erase_right(right_iterator f, right_iterator l) noexcept
	{
		return erase_range(f, l);
	}

  right_t const &at_left(left_t const &key) const noexcept(noexcept(find_left(key)))
	{
		auto iter = find_left(key);
		if (iter == end_left())
			throw std::out_of_range("at_left bad");
		return *iter.flip();
	}
  left_t const &at_right(right_t const &key) const noexcept(noexcept(find_right(key)))
	{
		auto iter = find_right(key);
		if (iter == end_right())
			throw std::out_of_range("at_right bad");
		return *iter.flip();
	}
  right_t at_left_or_default(left_t const &key) const
  noexcept(noexcept(find_left(key)) && std::is_nothrow_default_constructible_v<left_t>)
	{
		auto iter = find_left(key);
		if (iter == end_left())
			return left_t();
		return *iter.flip();
	}
  left_t at_right_or_default(right_t const &key) const
  noexcept(noexcept(find_right(key)) && std::is_nothrow_default_constructible_v<right_t>)
	{
		auto iter = find_right(key);
		if (iter == end_right())
			return right_t();
		return *iter.flip();
	}

  left_iterator lower_bound_left(const left_t &left) const
	noexcept(noexcept(root->left_node()->find_ge(left, left_comparator())))
	{
		if (root == nullptr)
			return end_left();
		return left_iterator(&root, node_t::cast(root->left_node()->find_ge(left, left_comparator())));
	}
  left_iterator upper_bound_left(const left_t &left) const
	noexcept(noexcept(lower_bound_left(left)))
	{
		auto it = lower_bound_left(left);
		if (it == end_left())
			return it;
		// *it >= left
		if (!left_comparator()(left, *it))
			++it;
		return it;
	}

  right_iterator lower_bound_right(const right_t &right) const
	noexcept(noexcept(root->right_node()->find_ge(right, right_comparator())))
	{
		if (root == nullptr)
			return end_right();
		return right_iterator(&root, node_t::cast(root->right_node()->find_ge(right, right_comparator())));
	}
  right_iterator upper_bound_right(const right_t &right) const
	noexcept(noexcept(lower_bound_right(right)))
	{
		auto it = lower_bound_right(right);
		if (it == end_right())
			return it;
		// *it >= left
		if (!right_comparator()(right, *it))
			++it;
		return it;
	}

  [[nodiscard]] bool empty() const noexcept
	{
		return size() == 0;
	}
  [[nodiscard]] std::size_t size() const noexcept
	{
		return sz;
	}

  bool operator==(bimap const &b) const
	{
		auto it1 = begin_left();
		auto it2 = b.begin_left();
		auto const& l = left_comparator();
		auto const& r = right_comparator();
		while (it1 != end_left() && it2 != b.end_left())
			if (l(*it1, *it2) || l(*it2, *it1) || r(*it1.flip(), *it2.flip()) || r(*it2.flip(), *it1.flip()))
			{
				return false;
			}
			else
			{
				++it1;
				++it2;
			}
		if (it1 != end_left() || it2 != b.end_left())
			return false;
		return true;
	}
  bool operator!=(bimap const &b) const
	{
		return !operator==(b);
	}
};

