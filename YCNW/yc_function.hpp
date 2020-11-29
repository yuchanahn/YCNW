#pragma once
#include <mutex>
#include <ranges>
#include <optional>
#include <queue>

static auto is_act_true = [](auto& i) { return i.is_active; };
static auto pis_act_true = [](auto& i) { return i->is_active; };
static auto is_act_false = [](auto& i) { return !i.is_active; };
static auto pis_act_false = [](auto& i) { return !i->is_active; };

namespace yc {

	template< class, class = std::void_t<> >
	struct needs_unapply : std::true_type { };

	template< class T >
	struct needs_unapply<T, std::void_t<decltype(std::declval<T>()())>> : std::false_type { };

	template <typename F> auto
		curry(F&& f) {
		if constexpr (needs_unapply<decltype(f)>::value) {
			return [=](auto&& x) {
				return curry(
					[=](auto&&...xs) -> decltype(f(x, xs...)) {
						return f(x, xs...);
					}
				);
			};
		}
		else {
			return f();
		}
	}

	template <typename Fs>
	void invoke_all(Fs fs)
	{
		for (auto& f : fs) {
			f();
		}
	}

}