#pragma once
#include <std.hpp>
#include "non_void_helper.hpp"

namespace mini_async
{
	//定义概念Awaiter，要求类型A包含这三个成员函数
	template<class A>
	concept	Awaiter = requires(A a, std::coroutine_handle<> coroutine)
	{
		{ a.await_ready() };
		{ a.await_suspend(coroutine) };
		{ a.await_resume() };
	};

	template<class A>
	concept Awaitable = Awaiter<A> || requires(A a)
	{
		{ a.operator co_await() } ->Awaiter;
	};

	template<class A>
	struct AwaitableTraits;

	template<Awaiter A>
	struct AwaitableTraits<A>
	{
		using RetType = decltype(std::declval<A>().await_resume());

		using NonVoidRetType = NonVoidHelper<RetType>::Type;
		using Type = RetType;
		using AwaiterType = A;
	};

	template<class A> requires(!Awaiter<A>&& Awaitable<A>)
		struct AwaitableTraits<A>
		: AwaitableTraits<decltype(std::declval<A>().operator co_await())> {};

	template<class... Ts>
	struct TypeList{};

	template<class Last>
	struct TypeList<Last>
	{
		using FirstType = Last;
		using LastType = Last;
	};

	template<class First, class... Ts>
	struct TypeList<First, Ts...>
	{
		using FirstType = First;
		using LastType = typename TypeList<Ts...>::LastType;//
	};
}
