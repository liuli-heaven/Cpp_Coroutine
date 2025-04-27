#pragma once
#include <std.hpp>
#include "non_void_helper.hpp"

namespace mini_async
{
	//将裸露的值封装为一个未初始化空间，将复制和取值的操作封装起来。
	template<class T>
	struct Uninitialized
	{
		union
		{
			T mValue;
		};

		Uninitialized() noexcept {}
		Uninitialized(Uninitialized&&) = delete;
		~Uninitialized() noexcept {}

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

//	template<class T>
//	struct Uninitialized
//	{
//		union
//		{
//			T mValue;
//		};
//	#if CO_ASYNC_DEBUG
//		bool mHasValue = false;
//	#endif
//		Uninitialized() noexcept {};
//		Uninitialized(Uninitialized&&) = delete;
//		~Uninitialized() noexcept
//		{
//		#if CO_ASYNC_DEBUG					
//			if (!mHasValue) [[unlikely]] {
//				throw std::logic_error("Uninitialized::ref called in an unvalued slot");
//				}
//		#endif	
//		};
//
//		T const& ref() const noexcept
//		{
//		#if CO_ASYNC_DEBUG
//			if (!mHasValue) [[unlikely]] {
//				throw std::logic_error(
//					"Uninitialized::ref called in an unvalued slot");
//				}
//		#endif
//			return mValue;
//		}
//
//		T& ref() noexcept
//		{
//		#if CO_ASYNC_DEBUG
//			if (!mHasValue) [[unlikely]] {
//				throw std::logic_error(
//					"Uninitialized::ref called in an unvalued slot");
//				}
//		#endif
//			return mValue;
//		}
//
//		T move()
//		{
//		#if CO_ASYNC_DEBUG
//			if (!mHasValue) [[unlikely]] {
//				throw std::logic_error(
//					"Uninitialized::move called in an unvalued slot");
//				}
//		#endif
//			T ret(std::move(mValue));
//			mValue.~T();
//		#if CO_ASYNC_DEBUG
//			mHasValue = false;
//		#endif
//			return ret;
//		}
//
//		template<class ...Ts>
//			requires std::constructible_from<T, Ts...> // ��ΪT���Ա�Ts...�βΰ����������
//		void emplace(Ts&& ...args)
//		{
//#if CO_ASYNC_DEBUG
//			if (mHasValue) [[unlikely]]
//				{
//					throw std::logic_error("Uninitialized::ref called in an unvalued slot");
//				}
//#endif
//				std::construct_at(std::addressof(mValue), std::forward<Ts>(args)...);
//#if CO_ASYNC_DEBUG
//		mHasValue = true;
//#endif
//
//		}
//	};
//
//	template<>
//	struct Uninitialized<void>
//	{
//		void ref()const noexcept{}
//
//		void destory(){}
//
//		Void move()
//		{
//			return Void();
//		}
//
//		void emplace(){}
//
//		void emplace(Void){}
//	};
//
//	template<>
//	struct Uninitialized<Void> : Uninitialized<void>{};
//
//	template<class T>
//	struct Uninitialized<const T> : Uninitialized<T>
//	{
//
//	};
//
//	template<class T>
//	struct Uninitialized<T&> : Uninitialized<std::reference_wrapper<T>>
//	{
//	private:
//		using Base = Uninitialized<std::reference_wrapper<T>>;
//
//	public:
//		const T& ref()const noexcept
//		{
//			return Base::ref().get();
//		}
//		T& ref()noexcept
//		{
//			return Base::ref().get();
//		}
//		T& move()
//		{
//			return Base::move().get();
//		}
//	};
//
//	template<class T>
//	struct Uninitialized<T&&> : Uninitialized<T>
//	{
//	private:
//		using Base = Uninitialized<T&>;
//	public:
//		T&& move()
//		{
//			return std::move(Base::move().get());
//		}
//	};
}
