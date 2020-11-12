#pragma once

#include <functional>
#include <stdexcept>
#include <exception>
#include <type_traits>

#include <iostream>

#include "bimap-helper.h"
#include "splay.h"

template <typename Left, typename Right, typename CompareLeft = std::less<Left>,
          typename CompareRight = std::less<Right>>
struct bimap
: private bimap_helper::tagged_comparator<CompareLeft>
, private bimap_helper::tagged_comparator<CompareRight, splay::default_tag2_t<CompareRight>>
{
  using left_t = Left;
  using right_t = Right;

	using node_t = bimap_helper::node_t<Left, Right>;
	using left_comparator_holder = bimap_helper::tagged_comparator<CompareLeft>;
	using right_comparator_holder = bimap_helper::tagged_comparator<CompareRight, splay::default_tag2_t<CompareRight>>;

	using left_iterator = bimap_helper::bimap_iterator<node_t, typename node_t::left_holder>;
	using right_iterator = bimap_helper::bimap_iterator<node_t, typename node_t::right_holder>;

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

  explicit bimap(CompareLeft&& cl = CompareLeft(), CompareRight&& cr = CompareRight()) noexcept
	: left_comparator_holder(std::move(cl))
	, right_comparator_holder(std::move(cr))
	, root(nullptr)
	, sz(0)
	{}

  bimap(bimap const &other) : bimap()
	{
		operator=(other);
	}
  bimap(bimap &&other) noexcept : root(other.root), sz(other.sz)
	{
		other.root = nullptr;
		other.sz = 0;
	}

  bimap& operator=(bimap const &other)
	{
		if (this == &other)
			return *this;
		clear();
		for (auto iter = other.begin_left(); iter != other.end_left(); ++iter)
			insert(*iter, *iter.flip());
		std::cout <<  " !!!!" << sz << std::endl;
		return *this;
	}
  bimap& operator=(bimap &&other) noexcept
	{
		std::swap(root, other.root);
		std::swap(sz, other.sz);
		return *this;
	}

	void clear()
	{
		if (size() == 0)
			return;
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
  ~bimap()
	{
		clear();
	}

  left_iterator begin_left() const
	{
		if (root == nullptr)
			return left_iterator(&root, nullptr);
		auto rt = root->left_node();
		rt->splay();
		return left_iterator(&root, node_t::cast(rt->call(&std::remove_pointer_t<decltype(rt)>::left_most)));
	}
  left_iterator end_left() const
	{
		return left_iterator(&root, nullptr);
	}

  right_iterator begin_right() const
	{
		if (root == nullptr)
			return right_iterator(&root, nullptr);
		auto rt = root->right_node();
		rt->splay();
		return right_iterator(&root, node_t::cast(rt->call(&std::remove_pointer_t<decltype(rt)>::left_most)));
	}
  right_iterator end_right() const
	{
		return right_iterator(&root, nullptr);
	}

	template<typename T1, typename T2>
	left_iterator insert_impl(T1&& l, T2&& r)
	{
		if (root == nullptr)
		{
			root = new node_t(std::forward<T1>(l), std::forward<T2>(r));
			sz = 1;
			return left_iterator(&root, root);
		}

		auto fl = root->left_node()->template find_ge<CompareLeft>(l, left_comparator());
		if (fl != nullptr && !left_comparator()(l, fl->data))
			return end_left();
		auto fr = root->right_node()->template find_ge<CompareRight>(r, right_comparator());
		if (fr != nullptr && !CompareRight()(r, fr->data))
			return end_left();

		auto node = new node_t(std::forward<T1>(l), std::forward<T2>(r));

		sz++;

		typename node_t::left_holder::node_t const* mll;
		decltype(mll) mrl;
		if (fl == nullptr)
		{
			mll = root->left_node()->as_holder();
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
			mlr = root->right_node()->as_holder();
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

  left_iterator insert(left_t const &left, right_t const &right)
	{
		return insert_impl(left, right);
	}
  left_iterator insert(left_t const &left, right_t &&right)
	{
		return insert_impl(left, std::move(right));
	}
  left_iterator insert(left_t &&left, right_t const &right)
	{
		return insert_impl(std::move(left), right);
	}
  left_iterator insert(left_t &&left, right_t &&right)
	{
		return insert_impl(std::move(left), std::move(right));
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
  right_iterator erase_right(right_iterator it)
	{
		return erase_impl(it);
	}

  // Возвращает итератор по элементу. Если не найден - соответствующий end()
  left_iterator find_left(left_t const &left) const
	{
		auto found = root->left_node()->find_ge(left, left_comparator());
		// found >= left
		if (found != nullptr && left_comparator()(left, found->data))
			return end_left();
		return left_iterator(&root, node_t::cast(found));
	}
  right_iterator find_right(right_t const &right) const
	{
		auto found = root->right_node()->find_ge(right, right_comparator());
		if (found != nullptr && right_comparator()(right, found->data))
			return end_right();
		return right_iterator(&root, node_t::cast(found));
	}

  bool erase_left(left_t const &left)
	{
		auto found = find_left(left);
		if (found == end_left())
			return false;
		erase_left(found);
		return true;
	}
  bool erase_right(right_t const &right)
	{
		auto found = find_right(right);
		if (found == end_right())
			return false;
		erase_right(found);
		return true;
	}

	template<typename T>
	T erase_range_impl(T f, T l)
	{
		while (f != l)
			f = erase_impl(f);
		return f;
	}
  left_iterator erase_left(left_iterator first, left_iterator last)
	{
		return erase_range_impl(first, last);
	}
  right_iterator erase_right(right_iterator first, right_iterator last)
	{
		return erase_range_impl(first, last);
	}

  right_t const &at_left(left_t const &key) const
	{
		auto iter = find_left(key);
		if (iter == end_left())
			throw std::out_of_range("at_left bad");
		return *iter.flip();
	}
  left_t const &at_right(right_t const &key) const
	{
		auto iter = find_right(key);
		if (iter == end_right())
			throw std::out_of_range("at_right bad");
		return *iter.flip();
	}
  right_t at_left_or_default(left_t const &key) const
	{
		auto iter = find_left(key);
		if (iter == end_left())
			return left_t();
		return *iter.flip();
	}
  left_t at_right_or_default(right_t const &key) const
	{
		auto iter = find_right(key);
		if (iter == end_right())
			return right_t();
		return *iter.flip();
	}

  left_iterator lower_bound_left(const left_t &left) const
	{
		if (root == nullptr)
			return end_left();
		return left_iterator(&root, node_t::cast(root->left_node()->find_ge(left, left_comparator())));
	}
  left_iterator upper_bound_left(const left_t &left) const
	{
		if (root == nullptr)
			return end_left();
		auto found = root->left_node()->find_ge(left, left_comparator());
		// found >= left
		if (found == nullptr || left_comparator()(left, found->data))
			return left_iterator(&root, node_t::cast(found));
		constexpr auto next = &std::remove_pointer_t<decltype(found)>::node_t::next;
		return left_iterator(&root, node_t::cast(found->call(next)));
	}

  right_iterator lower_bound_right(const right_t &right) const
	{
		if (root == nullptr)
			return end_right();
		return right_iterator(&root, node_t::cast(root->right_node()->find_ge(right, right_comparator())));
	}
  right_iterator upper_bound_right(const right_t &right) const
	{
		if (root == nullptr)
			return end_right();
		auto found = root->right_node()->find_ge(right, right_comparator());
		if (found == nullptr || right_comparator()(right, found->data))
			return right_iterator(&root, node_t::cast(found));
		constexpr auto next = &std::remove_pointer_t<decltype(found)>::node_t::next;
		return right_iterator(&root, node_t::cast(found->call(next)));
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

