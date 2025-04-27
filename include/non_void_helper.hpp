#pragma once
#include "std.hpp"

namespace mini_async
{

	template <class T = void>
	struct NonVoidHelper {
		using Type = T;
	};

	template <>
	struct NonVoidHelper<void> {
		using Type = NonVoidHelper;

		explicit NonVoidHelper() = default;

		template <class T>
		constexpr friend T&& operator,(T&& t, NonVoidHelper) {
			return std::forward<T>(t);
		}

		char const* repr() const noexcept {
			return "NonVoidHelper";
		}
	};
	/*struct Void final
	{
		explicit Void() = default;

		template<class T>
		friend constexpr T&& operator,(T&& t, Void)
		{
			return std::forward<T>(t);
		}

		template<class T>
		friend constexpr T&& operator,(Void, T&&)
		{
			return std::forward<T>(t);
		}

		friend constexpr void operator|(Void, Void) {}

		const char* prep()const noexcept
		{
			return "void";
		}
	};

	template<class T = void>
	struct AvoidVoidTrait
	{
		using Type = T;
		using RefType = std::reference_wrapper<T>;
		using CRefType = std::reference_wrapper<T const>;
	};

	template<>
	struct AvoidVoidTrait<void>
	{
		using Type = Void;
		using RefType = Void;
		using CRefType = Void;
	};

	template<class T>
	using Avoid = typename AvoidVoidTrait<T>::Type;
	using AvoidRef = typename AvoidVoidTrait<T>::RefType;
	using AvoidCRef = typename AvoidVoidTrait<T>::CRefType;*/

}