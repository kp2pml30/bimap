#pragma once

#include <utility>
#include <cassert>

#include <string>

namespace splay
{
	template<typename T>
	struct default_tag_t {};
	template<typename T>
	struct default_tag2_t {};

	template<typename Tag>
	struct splay_node
	{
		mutable splay_node
			*left = nullptr,
			*right = nullptr,
			*up = nullptr;

#if 0
		template<splay_node* splay_node::* getter>
		static constexpr splay_node* splay_node::* cogetter_v =  getter ^ &splay_node::left ^ &splay_node::right;
#else
		template<splay_node* splay_node::* getter>
		static constexpr splay_node* splay_node::* cogetter_f()
		{
			if constexpr (getter == &splay_node::left)
				return &splay_node::right;
			else if constexpr(getter == &splay_node::right)
				return &splay_node::left;
			else
				return ""; // generate error, static assert fails anyway
		}
		template<splay_node* splay_node::* getter>
		static constexpr auto cogetter_v = cogetter_f<getter>();
#endif

		template<splay_node* splay_node::* getter>
		void rotate() const noexcept
		{
			constexpr auto cogetter = cogetter_v<getter>;
			auto p = up;
			auto r = this->*cogetter;
			if (p != nullptr)
			{
				if (p->*getter == this)
					p->*getter = r;
				else
					p->*cogetter = r;
			}
			auto unconst_this = const_cast<splay_node*>(this);
			auto tmp = r->*getter;
			r->*getter = unconst_this;
			unconst_this->*cogetter = tmp;

			up = r;
			r->up = p;
			if (auto got = this->*cogetter; got != nullptr)
				got->up = const_cast<splay_node*>(this);
		}
		splay_node const* splay() const noexcept
		{
			while (up != nullptr)
				if (this == up->left)
				{
					if (up->up == nullptr)
					{
						up->rotate<&splay_node::right>();
					}
					else if (up == up->up->left)
					{
						up->up->template rotate<&splay_node::right>();
						up->rotate<&splay_node::right>();
					}
					else
					{
						up->rotate<&splay_node::right>();
						up->rotate<&splay_node::left>();
					}
				}
				else
				{
					if (up->up == nullptr)
					{
						up->rotate<&splay_node::left>();
					}
					else if (up == up->up->right)
					{
						up->up->template rotate<&splay_node::left>();
						up->rotate<&splay_node::left>();
					}
					else
					{
						up->rotate<&splay_node::left>();
						up->rotate<&splay_node::right>();
					}
				}
			return this;
		}

		template<splay_node* splay_node::* getter>
		splay_node const* left_right_most() const noexcept
		{
			auto cur = this;
			while (cur->*getter != nullptr)
				cur = cur->*getter;
			return cur->splay();
		}

		template<splay_node* splay_node::* getter>
		splay_node const* next_prev_impl() const noexcept
		{
			constexpr auto cogetter = cogetter_v<getter>;
			if (auto r = this->*getter; r != nullptr)
				return r->template left_right_most<cogetter>();
			if (up == nullptr)
				return nullptr;
			auto prev = this;
			auto cur = up;
			while (cur != nullptr && cur->*getter == prev)
			{
				prev = cur;
				cur = cur->up;
			}
			return cur ?: prev;
		}
		splay_node const* next() const noexcept
		{
			return next_prev_impl<&splay_node::right>();
		}
		splay_node const* prev() const noexcept
		{
			return next_prev_impl<&splay_node::left>();
		}

		splay_node const* left_most() const noexcept
		{
			return left_right_most<&splay_node::left>();
		}
		splay_node const* right_most() const noexcept
		{
			return left_right_most<&splay_node::right>();
		}

		/**
		 * cuts as {[0..cur), [cur..end]}
		 */
		std::pair<splay_node const*, splay_node const*> cut() const
		{
			splay();
			auto l = left;
			if (left != nullptr)
			{
				left->up = nullptr;
				left = nullptr;
			}
			return {l, this};
		}
		/**
		 * cuts as {[0..cur), cur, (cur..end]}
		 */
		std::tuple<splay_node const*, splay_node const*, splay_node const*> cutcut() const
		{
			auto [l, c] = cut();
			auto r = c->right;
			if (c->right != nullptr)
			{
				c->right->up = nullptr;
				c->right = nullptr;
			}
			return {l, c, r};
		}
		template<splay_node* splay_node::*getter>
		void merge_side(splay_node const* tree) const
		{
			if (tree == nullptr)
				return;
			assert(tree->up == nullptr);
			splay();
			auto cur = left_right_most<getter>();
			const_cast<splay_node*>(cur)->*getter = const_cast<splay_node*>(tree);
			if (tree != nullptr)
				tree->up = const_cast<splay_node*>(cur);
			splay();
		}
		void mergel(splay_node const* tree) const
		{
			merge_side<&splay_node::left>(tree);
		}
		void merger(splay_node const* tree) const
		{
			merge_side<&splay_node::right>(tree);
		}
		void merge(splay_node const* treel, splay_node const* treer) const
		{
			mergel(treel);
			merger(treer);
		}

		splay_node const* cutcutmerge() const
		{
			auto [l, cur, r] = cutcut();
			if (r == nullptr)
				return l;
			r->mergel(l);
			return r;
		}
	};

	template<typename T, typename Tag = default_tag_t<T>>
	class splay_holder : public splay_node<Tag>
	{
	public:
		using node_t = splay_node<Tag>;
		using value_type = T;

		T data;

		explicit splay_holder(T const& data)
		: data(data)
		{}
		explicit splay_holder(T&& data)
		: data(std::move(data))
		{}

		constexpr node_t const* as_holder() const noexcept 
		{
			return static_cast<node_t const*>(this);
		}

		static splay_holder const* cast(splay_node<Tag> const* from) noexcept
		{
			if (from == nullptr)
				return nullptr;
			return static_cast<splay_holder const*>(from);
		}

		template<typename F, typename ...A>
		splay_holder const* call(F f, A&&... a) const
		{
			return cast((this->*f)(std::forward<A>(a)...));
		}

		template<typename C>
		splay_holder const* find_ge(T const& e, C const& c) const
		{
			this->splay();
			splay_holder const* cur = this;
			splay_holder const* best = nullptr;
			do
			{
				auto const& cd = cast(cur)->data;
				if (c(e, cd))
				{
					best = cur;
					cur = cast(cur->left);
				}
				else if (!c(cd, e)) // eq
				{
					return cast(cur->splay());
				}
				else
				{
					if (cur->right == nullptr)
					{
						if (best == nullptr)
							return nullptr;
						else
							return cast(best->splay());
					}
					cur = cast(cur->right);
				}
			} while (cur != nullptr);
			if (best == nullptr)
				return nullptr;
			return cast(best->splay());
		}

		std::string ToMermaid(int& counter, std::string& res) const noexcept
		{
			auto mestr = std::to_string(counter++);
			if (this == nullptr)
			{
				res += "_" + mestr + "[nil];\n";
				return mestr;
			}
			res += "_" + mestr + "[" + std::to_string(data)+ "];\n";
			res += "_" + mestr + " ---> _" + cast(node_t::left)->ToMermaid(counter, res) + ";\n";
			res += "_" + mestr + " ---> _" + cast(node_t::right)->ToMermaid(counter, res) + ";\n";
			return mestr;
		}
	};
	template<typename C, typename N>
	N const* get_insert_pos_ge_checked(N const* n, typename N::value_type const& v)
	{
		auto res = n->template find_ge<C>(v);
		C c;
		// res >= v
		if (!c(v, res->data))
			return nullptr;
		return res;
	}
}


