#pragma once
#include <utility>

#include "concepts.hpp"
#include "return_previous.hpp"
#include "task.hpp"
#include "uninitialized.hpp"

namespace mini_async
{
	struct WhenAllCtlBlock
	{
		std::size_t mCount{};
		std::coroutine_handle<> mPrevious{};
		std::exception_ptr mException{};
		WhenAllCtlBlock operator ++(int)
		{
			mCount++;
			return *this;
		}
		WhenAllCtlBlock operator --(int)
		{
			mCount--;
			return *this;
		}
	};

	struct WhenAllAwaiter
	{
		WhenAllCtlBlock& mControl;
		std::span<const ReturnPreviousTask> mTasks;

		bool await_ready() noexcept
		{
			return false;
		}

		std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) noexcept
		{
			if (mTasks.empty())
			{
				return coroutine;
			}
			mControl.mPrevious = coroutine;
			for (auto const& t : mTasks.subspan(0, mTasks.size() - 1))
			{
				t.mCoroutine.resume();
			}
			return mTasks.back().mCoroutine;
		}

		void await_resume() const
		{
			if (mControl.mException) [[unlikely]]
				{
					std::rethrow_exception(mControl.mException);
				}
		}
	};

	template<class T>
	ReturnPreviousTask whenAllHelper(auto&& t, WhenAllCtlBlock& control, Uninitialized<T>& result)
	{
		try
		{
			result.putValue(co_await std::forward<decltype(t)>(t));
		}
		catch (...)
		{
			control.mException = std::current_exception();
			co_return control.mPrevious;
		}
		//在不进行co_await时，协程都是单线程在运行，所以不需要担心原子性的问题。
		//在这里，co_await t之前的不是原子性，在其之后的代码具有原子性。
		control--;
		if (control.mCount == 0)
		{
			co_return control.mPrevious;
		}
		co_return std::noop_coroutine();
	}

	template<class T = void>
	ReturnPreviousTask whenAllHelper(auto&& t, WhenAllCtlBlock& control, Uninitialized<void>&)
	{
		try
		{
			co_await std::forward<decltype(t)>(t);
		}
		catch (...)
		{
			control.mException = std::current_exception();
			co_return control.mPrevious;
		}
		
		control--;
		if (control.mCount == 0)
		{
			co_return control.mPrevious;
		}
		co_return std::noop_coroutine();
	}

	template<std::size_t ...Is, class ...Ts>
	Task<std::tuple<typename AwaitableTraits<Ts>::NonVoidRetType...>>
		whenAllImpl(std::index_sequence<Is...>, Ts&& ...ts)
	{
		WhenAllCtlBlock control{ sizeof... (Ts) };
		std::tuple<Uninitialized<typename AwaitableTraits<Ts>::RetType>...> result;
		ReturnPreviousTask taskArray[]{ whenAllHelper(ts, control, std::get<Is>(result))... };
		co_await WhenAllAwaiter(control, taskArray);
		co_return std::tuple<typename AwaitableTraits<Ts>::NonVoidRetType...>(
			std::get<Is>(result).moveValue()...);
	}

	template<Awaitable ...Ts> requires(sizeof...(Ts) != 0)
		auto when_all(Ts&& ...ts)
	{
		return whenAllImpl(std::make_index_sequence<sizeof...(Ts)>{},
			std::forward<Ts>(ts)...);
	}

	template<Awaitable T, class Alloc = std::allocator<T>>
	Task<std::conditional_t<
		std::same_as<void, typename AwaitableTraits<T>::RetType>,
		std::vector<typename AwaitableTraits<T>::RetType, Alloc>, void>>
	when_all(const std::vector<T, Alloc>& tasks)
	{
		WhenAllCtlBlock control(tasks.size());
		Alloc alloc = tasks.get_allocator();
		std::vector<Uninitialized<typename AwaitableTraits<T>::RetType>, Alloc>
			result(tasks.size(), alloc);
		{
			std::vector<ReturnPreviousTask, Alloc> taskArray(alloc);
			taskArray.reserve(tasks.size());
			for(std::size_t i = 0; i < tasks.size(); i++)
			{
				taskArray.push_back(whenAllHelper(tasks[i], control, result[i]));
			}
			co_await WhenAllAwaiter(control, taskArray);
		}
	}
}
