#include <iostream>
#include <chrono>
#include <coroutine>
#include "debug.hpp"
#include <stack>
#include <thread>
#include <variant>
#include <deque>
#include <queue>
#include <concepts>

using namespace std::chrono_literals;
/*
 * 规定名称格式：
 * 形如promise_type这样的使用匈牙利命名法的函数名称对应标准库
 * 使用小驼峰命名法的为自定义函数
 * 使用大驼峰命名法的为自定义类
 */


//将裸露的值封装为一个未初始化空间，将复制和取值的操作封装起来。
template<class T = void>
struct NonVoidHelper
{
	using Type = T;
};

template<>
struct NonVoidHelper<void>
{
	using Type = NonVoidHelper;
	explicit NonVoidHelper() = default;
};

template<class T>
struct Uninitialized
{
	union 
	{
		T mValue;
	};

	Uninitialized() noexcept {};
	Uninitialized(Uninitialized&&) = delete;
	~Uninitialized() noexcept {};

	T moveValue()
	{
		T ret(std::move(mValue));
		mValue.~T();
		return ret;
	}

	template<class ...Ts>
	void putValue(Ts&& ...args)
	{
		new (std::addressof(mValue)) T(std::forward<Ts>(args)...);
	}
};

template<>
struct Uninitialized<void>
{
	auto moveValue()
	{
		return NonVoidHelper<>{};
	}
};

template<class T>
struct Uninitialized<const T> : Uninitialized<T>
{
	
};

template<class T>
struct Uninitialized<T&> : Uninitialized<std::reference_wrapper<T>>
{
	
};

template<class T>
struct Uninitialized<T&&> : Uninitialized<T>
{
	
};

//定义概念Awaiter，要求类型A包含这三个成员函数
template<class A>
concept	Awaiter = requires(A a, std::coroutine_handle<> coroutine)
{
	{ a.await_ready() };
	{ a.await_suspend(coroutine) };
	{ a.await_resume() };
};



 // 即协程调度器(Scheduler)， 使用名称Loop来指代。
 // 由于传统协程通常存在一个循环来拉取事件，因此称为Loop
struct Loop
{
	std::deque<std::coroutine_handle<>> mReadyQueue;
	//std::deque<std::coroutine_handle<>> mWaitingQueue;

	struct TimeEntry
	{
		std::chrono::system_clock::time_point expireTime;
		std::coroutine_handle<> coroutine;

		bool operator<(const TimeEntry& other) const noexcept
		{
			return expireTime > other.expireTime;
		}
	};
	//priority_queue：底层采用堆实现的优先队列。默认采用大顶堆。
	std::priority_queue<TimeEntry> mTimerHeap;

	void addTask(std::coroutine_handle<> coroutine)
	{
		mReadyQueue.push_front(coroutine);
	}
	void addTimer(std::chrono::system_clock::time_point expireTime,
		std::coroutine_handle<> coroutine)
	{
		mTimerHeap.push({ expireTime, coroutine });
	}
	void runAll()
	{
		while (!mTimerHeap.empty() || !mReadyQueue.empty())
		{
			//在当前的逻辑下，首先处理就绪队列中的任务，全部执行完成后查询等待任务堆。
			while (!mReadyQueue.empty())
			{
				auto coroutine = mReadyQueue.front();
				debug(), "pop";
				mReadyQueue.pop_front();
				coroutine.resume();
			}
			//任务堆为小顶堆，时间点最近的事件在堆顶。
			//如果有任务到达时间，则将其加入就绪队列，再次循环。
			//如果没有任务到达时间，由于此时已经处理完所有就绪任务，
			//则开始睡眠，直到第一个任务就绪。
			if (!mTimerHeap.empty())
			{
				auto nowTime = std::chrono::system_clock::now();
				auto timer = std::move(mTimerHeap.top());
				if (timer.expireTime < nowTime)
				{
					mTimerHeap.pop();
					timer.coroutine.resume();
				}
				else
				{
					std::this_thread::sleep_until(timer.expireTime);
				}
			}
		}
	}
	Loop& operator=(Loop&&) = delete;
};

Loop& getLoop()
{
	static Loop loop;
	return loop;
}

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

	std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) const noexcept
	{
		if (mPrevious)
		{
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
		//std::construct_at(&mResult, std::move(ret)) 等价于new (&mResult) T(std::move(ret));
		mResult.putValue(std::move(ret));
		return std::suspend_always();
	}

	void return_value(T ret)
	{
		mResult.putValue(std::move(ret));
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
		//std::destroy_at(&mResult) 等价于mResult.~T();

		return mResult.moveValue();
	}
	std::coroutine_handle<Promise> get_return_object()
	{
		return std::coroutine_handle<Promise>::from_promise(*this);
	}

	
	std::coroutine_handle<> mPrevious{};
	//可能会没有返回值,此时mException中将会记录报错信息。
	//exception_ptr是记录异常信息的指针，是经过类型擦除的指针。
	std::exception_ptr mException{};
	//通过结构体，联合体等包装一层之后，mResult不会在类实例化的时候被初始化。
	union 
	{
		Uninitialized<T> mResult;
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
		mException = std::current_exception();
	}

	auto yield_void(int)
	{
		mException = std::current_exception();
		return std::suspend_always();
	}

	void return_void() noexcept
	{

	}

	void result()
	{
		//当指针不为空时，抛出异常。这里告诉编译器该分支不常发生
		if (mException) [[unlikely]]
			{
				std::rethrow_exception(mException);
			}
	}
	std::coroutine_handle<Promise> get_return_object()
	{
		return std::coroutine_handle<Promise>::from_promise(*this);
	}

	//对void进行偏特化，此时直接使用记录异常信息的指针即可。
	std::exception_ptr mException{};
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
		return false;
	}
	void await_suspend(std::coroutine_handle<> coroutine) const
	{
		//调度器中记录设定时间和对应的协程，以便到达时间后返回。
		getLoop().addTimer(mExpireTime, coroutine);
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

template<typename T>
concept is_coroutine_type = requires(T t)
{
	t.await_ready();
	t.await_suspend();
	t.await_resume();
};

struct WhenAllCounterBlock
{
	std::size_t mCount{};
	std::coroutine_handle<> mPrevious{};
	std::exception_ptr mException{};
	WhenAllCounterBlock operator ++(int)
	{
		mCount++;
		return *this;
	}
	WhenAllCounterBlock operator --(int)
	{
		mCount--;
		return *this;
	}
};

struct ReturnPreviousPromise
{
	auto initial_suspend() noexcept
	{
		return std::suspend_always();
	}

	auto final_suspend() noexcept
	{
		return PreviousAwaiter(mPrevious);
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

struct WhenAllAwaiter
{
	WhenAllCounterBlock& counter;
	const ReturnPreviousTask& t1;
	const ReturnPreviousTask& t2;

	bool await_ready() noexcept
	{
		return false;
	}

	std::coroutine_handle<> await_suspend(std::coroutine_handle<> coroutine) noexcept
	{
		counter.mPrevious = coroutine;
		getLoop().addTask(t2.mCoroutine);
		return t1.mCoroutine;
	}

	void await_resume() noexcept {}
};

template<class T>
ReturnPreviousTask whenAllHelper(const Task<T>& t, WhenAllCounterBlock& counter, Uninitialized<T>& result)
{
	try
	{
		result.putValue(co_await t);
	}
	catch (...)
	{
		counter.mException = std::current_exception();
		co_return counter.mPrevious;
	}
	//在不进行co_await时，协程都是单线程在运行，所以不需要担心原子性的问题。
	//在这里，co_await t之前的不是原子性，在其之后的代码具有原子性。
	counter--;
	if (counter.mCount == 0)
	{
		co_return counter.mPrevious;
	}
	co_return nullptr;
}



template<class T1, class T2>
Task<std::tuple<T1, T2>> when_all(const Task<T1>& t1, const Task<T2>& t2)
{
	WhenAllCounterBlock counter;
	std::tuple<Uninitialized<T1>, Uninitialized<T2>> result;
	counter.mCount = 2;
	//使用辅助函数whenAllHelper来记录当前的协程句柄。
	//当counter.mCount为0的时候，结束当前的协程函数。
	co_await WhenAllAwaiter(counter,
		whenAllHelper(t1, counter, std::get<0>(result)),
		whenAllHelper(t2, counter, std::get<1>(result)));

	co_return std::tuple<T1, T2>(
		std::get<0>(result).moveValue(),
		std::get<1>(result).moveValue());
}

Task<int> hello1()
{
	debug(), "hello1开始睡觉了";
	co_await sleep_for(2s);
	debug(), "hello1睡醒了";
	co_return 1;
}
Task<int> hello2()
{
	debug(), "hello2开始睡觉了";
	co_await sleep_for(4s);
	debug(), "hello2睡醒了";
	co_return 2;
}

Task<int> hello()
{
	auto [i, j] = co_await when_all(hello1(), hello2());
	debug(), "hello1的返回值为：", i, " ", "hello2的返回值为：", j;
	co_return i + j;
}

int main()
{
	auto t = hello();
	getLoop().addTask(t);
	getLoop().runAll();
	return 0;
}
