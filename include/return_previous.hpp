#pragma once
#include "previous_awaiter.hpp"
#include "task.hpp"

namespace mini_async
{
	struct ReturnPreviousPromise
	{
		auto initial_suspend() noexcept
		{
			return std::suspend_always();
		}

		auto final_suspend() noexcept
		{
			return PreviousAwaiter{ mPrevious };
		}

		void unhandled_exception()
		{
			throw;
		}

		void return_value(std::coroutine_handle<> coroutine) noexcept
		{
			mPrevious = coroutine;
		}

		auto get_return_object()
		{
			return std::coroutine_handle<ReturnPreviousPromise>::from_promise(*this);
		}

		std::coroutine_handle<> mPrevious{};
		ReturnPreviousPromise& operator=(ReturnPreviousPromise&&) = delete;
	};

	struct ReturnPreviousTask
	{
		using promise_type = ReturnPreviousPromise;
		std::coroutine_handle<promise_type> mCoroutine;

		ReturnPreviousTask(std::coroutine_handle<promise_type> coroutine) noexcept
			:mCoroutine(coroutine)
		{

		}
		ReturnPreviousTask(ReturnPreviousTask&&) = delete;

		~ReturnPreviousTask()
		{
			mCoroutine.destroy();
		}
		bool await_ready() const noexcept
		{
			return false;
		}
		std::coroutine_handle<promise_type> await_suspend(std::coroutine_handle<> coroutine) const noexcept
		{
			mCoroutine.promise().mPrevious = coroutine;
			return mCoroutine;
		}
		void await_resume() const
		{

		}
	};
}
