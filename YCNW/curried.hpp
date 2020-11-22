#pragma once

#include <optional>
#include <queue>

namespace yc {

	template< class, class = std::void_t<> >
	struct needs_unapply : std::true_type { };

	template< class T >
	struct needs_unapply<T, std::void_t<decltype(std::declval<T>()())>> : std::false_type { };

	template <typename F> auto
		curry(F&& f) {
		/// Check if f() is a valid function call. If not we need 
		/// to curry at least one argument:
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
			/// If 'f()' is a valid call, just call it, we are done.
			return f();
		}
	}

}