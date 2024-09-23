#include <iostream>
#include <chrono>
#include <coroutine>
#include "debug.hpp"
#include <stack>
#include <thread>
/*
 * 规定名称格式：
 * 形如promise_type这样的使用匈牙利命名法的函数名称对应标准库
 * 使用小驼峰命名法的为自定义函数
 * 使用大驼峰命名法的为自定义类
 */

struct RepeatAwaiter // awaiter(原始指针) / awaitable(operator->)
{
	bool await_ready() const noexcept
	{
        return false;
	}

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const noexcept
	{
		if (coroutine.done())
		{
			return std::noop_coroutine();
		}
		else
		{
			return coroutine;
		}
	}

	void await_resume() const noexcept {}
};

struct RepeatAwaitable
{
	RepeatAwaiter operator co_await()
	{
		return RepeatAwaiter();
	}
};

struct PreviousAwaiter
{
	bool await_ready() const noexcept
	{
		return false;
	}

	std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const noexcept
	{
		if (mPrevious)
		{
			return mPrevious;
		}
		else
		{
			return std::noop_coroutine();
		}
	}

	void await_resume() const noexcept {}

	std::coroutine_handle<> mPrevious;
};

struct Promise
{
	auto initial_suspend()
	{
		return std::suspend_always();
	}

	auto final_suspend() noexcept
	{
		return std::suspend_always();
	}

	void unhandled_exception()
	{
		throw;
	}

	auto yield_value(int ret)
	{
		mRetValue = ret;
		return std::suspend_always();
	}

	void return_void()
	{
		mRetValue = 0;
	}

	std::coroutine_handle<Promise> get_return_object()
	{
		return std::coroutine_handle<Promise>::from_promise(*this);
	}

	int mRetValue;
};

struct WorldPromise
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
		throw;
	}

	auto yield_value(int ret)
	{
		mRetValue = ret;
		return std::suspend_always();
	}

	/*void return_void()
	{
		mRetValue = 0;
	}*/

	void return_value(int value)
	{
		mRetValue = value;
	}

	std::coroutine_handle<WorldPromise> get_return_object()
	{
		return std::coroutine_handle<WorldPromise>::from_promise(*this);
	}

	int mRetValue;
	std::coroutine_handle<> mPrevious = nullptr;
};

struct Task
{
	using promise_type = WorldPromise;
	Task(std::coroutine_handle<promise_type> coroutine): mCoroutine(coroutine)
	{
		
	}

	std::coroutine_handle<promise_type> mCoroutine;
};



struct WorldTask
{
	using promise_type = WorldPromise;
	std::coroutine_handle<promise_type> mCoroutine;

	WorldTask(std::coroutine_handle<promise_type> coroutine) : mCoroutine(coroutine){}
	WorldTask(WorldTask&&) = delete;

	~WorldTask()
	{
		mCoroutine.destroy();
		//debug(), "WorldTask.mCoroutine destroy";
	}

	struct WorldAwaiter
	{
		std::coroutine_handle<promise_type> mCoroutine;

		bool await_ready() const noexcept
		{
			return false;
		}
		std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const noexcept
		{
			/*
			 *此处参数coroutine是表示调用者，在执行co_await调用协程时，
			 *会将当前上下文句柄传入WorldTask中的await_suspend中，即这里的coroutine
			 *所以此处将coroutine记录给mCoroutine
			 */
			mCoroutine.promise().mPrevious = coroutine;
			return mCoroutine;
		}

		void await_resume() const noexcept {}
	};

	auto operator co_await()
	{
		return WorldAwaiter(mCoroutine);
	}
};

WorldTask world()
{
	debug(), "world";
	co_yield 420;
	co_yield 440;
	co_return 0;
}

Task hello()
{
	debug(), "hello 正在构建worldTask";
	WorldTask worldTask = world();
	debug(), "hello 构建完了worldTask, 开始等待world";
	co_await worldTask;
	debug(), "等待world返回值为：", worldTask.mCoroutine.promise().mRetValue;
	co_await worldTask;
	debug(), "等待world返回值为：", worldTask.mCoroutine.promise().mRetValue;
	co_await worldTask;
	debug(), "等待world完了";
	debug(), "hello() 6";
	co_yield 6;
	debug(), "hello() 12";
	co_yield 12;
	debug(), "hello(), 36";
	co_return 36;
}

int main()
{
	debug(), "main即将调用hello";
	Task t = hello();
	debug(), "main调用完了hello";
	while (!t.mCoroutine.done())
	{
		t.mCoroutine.resume();
		debug(), "main得到hello结果为",
			t.mCoroutine.promise().mRetValue;
	}

	return 0;
}
