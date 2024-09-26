#include <iostream>
#include <chrono>
#include <coroutine>
#include "debug.hpp"
#include <stack>
#include <thread>
#include <variant>
/*
 * 规定名称格式：
 * 形如promise_type这样的使用匈牙利命名法的函数名称对应标准库
 * 使用小驼峰命名法的为自定义函数
 * 使用大驼峰命名法的为自定义类
 */

struct RepeatAwaiter
{
	bool await_ready() const noexcept { return false; }

	std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const noexcept {
		if (coroutine.done())
			return std::noop_coroutine();
		else
			return coroutine;
	}

	void await_resume() const noexcept {}
};

struct PreviousAwaiter {
	std::coroutine_handle<> mPrevious;

	bool await_ready() const noexcept { return false; }

	std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const noexcept {
		if (mPrevious)
		{
			debug(), "await suspend called, mPrevious: ", mPrevious.address();
			return mPrevious;
		}
		else
			return std::noop_coroutine();
	}

	void await_resume() const noexcept {}
};

template<class T>
struct Promise
{
	Promise() noexcept{}
	Promise(Promise&&) = delete;
	~Promise()noexcept{}
	//当协程启动时，决定是否立即挂起
	auto initial_suspend()
	{
		return std::suspend_always();
	}
	//当协程结束时，决定是否挂起。
	auto final_suspend() noexcept
	{
		//返回一个自定义的挂起器，来处理协程结束时的特定逻辑
		return PreviousAwaiter(mPrevious);
	}

	void unhandled_exception()
	{
		mException = std::current_exception();
	}

	auto yield_value(T ret) noexcept
	{
		std::construct_at(&mResult, std::move(ret));//等价于new (&mResult) T(std::move(ret));
		return std::suspend_always();
	}

	void return_value(T ret)
	{
		std::construct_at(&mResult, std::move(ret));
	}

	T result()
	{
		//如果存在返回值，则获取到指向返回值的指针。
		//如果错误信息指针存在，则抛出异常。这里告诉编译器该分支不常发生
		if (mException)[[unlikely]]
		{
			std::rethrow_exception(mException);
		}
		//错误信息指针不存在时，将返回值所有权移交后返回。
		T ret = std::move(mResult);
		std::destroy_at(&mResult);//等价于mResult.~T();
		return ret;
	}
	std::coroutine_handle<Promise> get_return_object()
	{
		return std::coroutine_handle<Promise>::from_promise(*this);
	}

	//可能会没有返回值，此时返回会报错。
	//exception_ptr是记录异常信息的指针，是经过类型擦除的指针。
	//这里使用variant令返回值和报错信息共享同一内存空间。
	std::coroutine_handle<> mPrevious{};
	std::exception_ptr mException{};
	//通过结构体，联合体等包装一层之后，mResult不会在类实例化的时候被初始化。
	union 
	{
		T mResult;
	};
};

template<>
struct Promise<void>
{
	//当协程启动时，决定是否立即挂起
	auto initial_suspend()
	{
		return std::suspend_always();
	}
	//当协程结束时，决定是否挂起。
	auto final_suspend() noexcept
	{
		//返回一个自定义的挂起器，来处理协程结束时的特定逻辑
		return PreviousAwaiter(mPrevious);
	}

	void unhandled_exception()
	{
		mResultException = std::current_exception();
	}

	auto yield_void(int)
	{
		mResultException = std::current_exception();
		return std::suspend_always();
	}

	void return_void() noexcept
	{

	}

	void result()
	{
		//当指针不为空时，抛出异常。这里告诉编译器该分支不常发生
		if (mResultException) [[unlikely]]
			{
				std::rethrow_exception(mResultException);
			}
	}
	std::coroutine_handle<Promise> get_return_object()
	{
		return std::coroutine_handle<Promise>::from_promise(*this);
	}

	//对void进行偏特化，此时直接使用记录异常信息的指针即可。
	std::exception_ptr mResultException{};
	std::coroutine_handle<> mPrevious{};

	Promise() = default;
	Promise(Promise&&) = delete;
	~Promise() = default;
};

template<class T = void>
struct Task
{
	using promise_type = Promise<T>;
	std::coroutine_handle<promise_type> mCoroutine;

	Task(std::coroutine_handle<promise_type> coroutine) noexcept : mCoroutine(coroutine){}
	Task(Task&&) = delete;

	~Task()
	{
		mCoroutine.destroy();
		//debug(), "Task.mCoroutine destroy";
	}

	struct Awaiter
	{
		std::coroutine_handle<promise_type> mCoroutine;
		// 用于检查协程是否需要挂起
		// 如果函数返回true, 则代表继续执行，不需要挂起。
		// 如果返回false,则表示挂起
		// co_await首先调用await_ready函数，如果返回值为false,则调用await_suspend
		bool await_ready() const noexcept
		{
			return false;
		}
		//该函数接受一个协程句柄作为参数，为当前协程的句柄
	//该函数返回值为协程句柄，表示恢复时应继续执行的位置。
		std::coroutine_handle<promise_type> await_suspend(std::coroutine_handle<> coroutine) const noexcept
		{
			/*
			 * std::coroutine_handel<>具有默认参数，相当于std::coroutine_handle<void> coroutine
			 * std::coroutine_handel<>是一个特化类型，对不同类型进行了类型擦除。
			 * 所以coroutine相当于一个原始指针void*，该void*中的数据块记录了协程判断和调度的一系列函数。
			 * 该方法的好处在于可以为不同类型的coroutine_handle提供相同接口。
			 */
			/*
			 *此处参数coroutine是表示调用者，在执行co_await调用协程时，
			 *会将当前上下文句柄传入WorldTask中的await_suspend中，即这里的coroutine
			 *所以此处将coroutine记录给mCoroutine
			 */
			mCoroutine.promise().mPrevious = coroutine;
			return mCoroutine;
		}
		//当协程聪挂起状态恢复的时候，会调用该函数
		//该函数负责协程恢复时的一系列操作。
		//await_resume函数在co_yield或co_return之后被调用。
		T await_resume() const
		{
			//debug(), "await_resume()";
			return mCoroutine.promise().result();
		}
	};

	auto operator co_await() const noexcept
	{
		return Awaiter(mCoroutine);
	}

	operator std::coroutine_handle<> () const noexcept
	{
		return mCoroutine;
	}
};

struct SleepAwaiter
{
	std::chrono::system_clock::time_point mExpireTime;

	bool await_ready() const
	{
		//如果当前时间大于等于设定时间，则开始继续执行，否则挂起。
		return std::chrono::system_clock::now() >= mExpireTime;
	}
	std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const
	{
		std::this_thread::sleep_until(mExpireTime);
		return coroutine;
	}

	void await_resume() const noexcept
	{
		
	}
};

Task<void> sleep_until(std::chrono::system_clock::time_point expireTime)
{
	co_await SleepAwaiter(expireTime);
	co_return;
}

Task<void> sleep_for(std::chrono::system_clock::duration duration)
{
	co_await SleepAwaiter(std::chrono::system_clock::now() + duration);
	co_return;
}

Task<int> hello()
{
	debug(), "hello开始睡觉了";
	co_await sleep_for(std::chrono::seconds(1));
	debug(), "hello睡醒了";
	co_return 42;
}

int main()
{
	Task t = hello();
	while (!t.mCoroutine.done())
	{
		t.mCoroutine.resume();
		debug(), "main得到hello结果为",
			t.mCoroutine.promise().result();
	}
	return 0;
}
