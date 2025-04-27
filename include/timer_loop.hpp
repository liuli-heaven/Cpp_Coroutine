#pragma once
#include <chrono>
#include <coroutine>

#include "rbtree.hpp"
#include "task.hpp"

namespace mini_async
{
	struct SleepUntilPromise : RbTree<SleepUntilPromise>::RbNode, Promise<void>
	{
		std::chrono::system_clock::time_point mExpiretime;

		auto get_return_object()
		{
			return std::coroutine_handle<SleepUntilPromise>::from_promise(*this);
		}

		SleepUntilPromise& operator=(SleepUntilPromise&&) = delete;

		friend bool operator<(SleepUntilPromise const& lhs, SleepUntilPromise const& rhs) noexcept
		{
			return lhs.mExpiretime < lhs.mExpiretime;
		}
	};

	struct TimerLoop
	{
		RbTree<SleepUntilPromise> mRbTimer{};
		bool hasEvent() const noexcept
		{
			return !mRbTimer.empty();
		}
		void addTimer(SleepUntilPromise& promise)
		{
			mRbTimer.insert(promise);
		}
		std::optional<std::chrono::system_clock::duration> run()
		{
			while (!mRbTimer.empty())
			{
				auto nowTime = std::chrono::system_clock::now();
				auto& promise = mRbTimer.front();
				if (promise.mExpiretime < nowTime)
				{
					mRbTimer.erase(promise);
					std::coroutine_handle <SleepUntilPromise>::from_promise(promise).resume();
				}
				else
				{
					return promise.mExpiretime - nowTime;
				}
			}
			return std::nullopt;
		}

		TimerLoop& operator=(TimerLoop&&) = delete;
	};

	struct SleepAwaiter
	{
		using ClockType = std::chrono::system_clock;
		
		TimerLoop& m_loop;
		ClockType::time_point mExpireTime;

		bool await_ready() const
		{
			//如果当前时间大于等于设定时间，则开始继续执行，否则挂起。
			return false;
		}
		void await_suspend(std::coroutine_handle<SleepUntilPromise> coroutine) const
		{
			//调度器中记录设定时间和对应的协程，以便到达时间后返回。
			/*getLoop().addTimer(mExpireTime, coroutine);*/
			auto& promise = coroutine.promise();
			promise.mExpiretime = mExpireTime;
			m_loop.addTimer(promise);
		}

		void await_resume() const noexcept
		{

		}
	};
	template<class Clock, class Dur>
	inline Task<void, SleepUntilPromise>
	sleep_until(TimerLoop& loop, std::chrono::time_point<Clock, Dur> expireTime)
	{
		co_await SleepAwaiter(
			loop, std::chrono::time_point_cast<SleepAwaiter::ClockType::duration>(expireTime)
		);
	}
	template<class Rep, class Period>
	inline Task<void, SleepUntilPromise>
	sleep_for(TimerLoop& loop, std::chrono::time_point<Rep, Period> duration)
	{
		auto d = std::chrono::duration_cast<SleepAwaiter::ClockType::duration>(duration);
		if(d.count() > 0)
		{
			co_await SleepAwaiter(loop, SleepAwaiter::ClockType::now() + d);
		}
	}
}
