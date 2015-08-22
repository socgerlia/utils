#pragma once

#include <utility>
#include <boost/asio/coroutine.hpp>
#include <boost/make_unique.hpp>
#include <boost/make_shared.hpp>

// namespace detail{

// template<class Func, class... Args>
// struct wrapper{
// 	Func f;
// 	wrapper(const Func& f) : f(f){}
// 	wrapper(Func&& f) : f(std::move(f)){}

// 	void operator()(Args... args){
// 		f(args...);
// 	}
// };

// } // end namespace detail

// struct unused
// {
// 	template<class T> unused(T&& v){}
// };

// template<class T>
// struct coroutine_ex : boost::asio::coroutine{
// 	template<class... Args>
// 	void resume(Args... args){
// 		(*this)(args...);
// 	}

// 	void operator()(){
// 		static_cast<T*>(this)->invoke();
// 	}

// 	template<class... Args>
// 	void operator()(Args&&... args){
// 		void* ar[] = { const_cast<void*>(reinterpret_cast<const void*>(&args))... };
// 		ap = ar;
// 		static_cast<T*>(this)->invoke();
// 	}

// 	template<class U>
// 	U& get(int index){
// 		return *reinterpret_cast<U*>(ap[index]);
// 	}

// 	template<class... Args>
// 	::detail::wrapper<T, Args...> wrap(){
// 		return { std::move(*static_cast<T*>(this)) };
// 	}

// 	void** ap;
// };

// template<class... Args, class Func>
// auto make_resume(coroutine_ex co, Func&& f){
// 	return [co, f{ std::forward<Func>(f) }](Args... args){
// 		void* ar[] = { const_cast<void*>(reinterpret_cast<const void*>(&args))... };
// 		co.ap = ar;
// 		f(co);
// 	};
// }
template<class T> struct coroutine;

namespace detail{

template<class... Args> inline void noop(Args&&... args){}

struct null_lvalue_type{
	template<class T> null_lvalue_type& operator=(T&& t){ return *this; }
};

}

namespace placeholders{ namespace{
detail::null_lvalue_type _;
}}

template<class Coroutine> struct is_coroutine_type : std::false_type{};
template<class T> struct is_coroutine_type<coroutine<T>> : std::true_type{};

template<class Coroutine>
auto get_context(Coroutine& co)
	-> typename std::enable_if<
		is_coroutine_type<Coroutine>::value,
		Coroutine&
	>::type
{
	return co;
}
template<class Coroutine>
auto get_context(Coroutine& co)
	-> typename std::enable_if<
		is_coroutine_type<typename std::remove_reference<decltype(*co)>::type>::value,
		decltype(*co)
	>::type
{
	return *co;
}

template<class Coroutine>
auto get_body(Coroutine&& co) -> decltype(get_context(co).f)&
{
	return get_context(co).f;
}

template<class Coroutine>
auto resume(Coroutine&& co, void** values = nullptr) -> decltype(get_body(co)(co))
{
	return get_body(co)(co);
}

template<class Coroutine>
auto resume(Coroutine&& co, void** values = nullptr) -> decltype(get_body(co)(co, values))
{
	return get_body(co)(co, values);
}

// template<class Coroutine>
// auto resume(Coroutine&& co, void** values = nullptr)
// 	-> decltype(co->f(co))
// {
// 	return co->f(co);
// }

// template<class Coroutine>
// auto resume(Coroutine&& co, void** values = nullptr)
// 	-> decltype(co.f(co, values))
// {
// 	return co.f(co, values);
// }

// template<class Coroutine>
// auto resume(Coroutine&& co, void** values = nullptr)
// 	-> decltype(co->f(co, values))
// {
// 	return co->f(co, values);
// }

template<class Coroutine, class... Locals>
auto make_resume(Coroutine&& co, Locals&... locals){
	using coroutine_type = typename std::remove_reference<Coroutine>::type;
	return [co = coroutine_type{std::forward<Coroutine>(co)}, &locals...](auto&&... args) -> void{
		static_assert(sizeof...(locals) == sizeof...(args), "Locals' count does not match");
		detail::noop((locals = std::forward<decltype(args)>(args))...);
		resume(co, nullptr);
	};
}

template<class... Args, class Coroutine>
auto make_resume_raw(Coroutine&& co){
	using coroutine_type = typename std::remove_reference<Coroutine>::type;
	return [co = coroutine_type{std::forward<Coroutine>(co)}](Args... args) -> void{
		void* values[] = { const_cast<void*>(reinterpret_cast<const void*>(&args))... };
		resume(co, values);
	};
}

template<class T>
struct coroutine : boost::asio::coroutine{
private:
	template<class Coroutine> friend auto get_body(Coroutine&& co) -> decltype(get_context(co).f)&;
	T f;

public:
	coroutine(coroutine&& v) = default;
	coroutine(const coroutine& v) = default;
	coroutine& operator=(coroutine&& v) = default;
	coroutine& operator=(const coroutine& v) = default;

	coroutine(T&& v) : f(std::move(v)){}
	coroutine(const T& v) : f(v){}
	template<class... Args> coroutine(Args&&... args) : f(std::forward<Args>(args)...){}

	void interrupt(){
		::boost::asio::detail::coroutine_ref coref { this };
	}
};


// make
template<class Func>
coroutine<typename std::decay<Func>::type> make_coroutine(Func&& f){
	return { std::forward<Func>(f) };
}

template<class Func>
auto make_unique_coroutine(Func&& f){
	using coroutine_type = coroutine<typename std::decay<Func>::type>;
	return boost::make_unique<coroutine_type>(std::forward<Func>(f));
}

template<class Func>
auto make_shared_coroutine(Func&& f){
	using coroutine_type = coroutine<typename std::decay<Func>::type>;
	return boost::make_shared<coroutine_type>(std::forward<Func>(f));
}

// emplace
template<class T, class... Args>
coroutine<T> emplace_coroutine(Args&&... args){
	return { std::forward<Args>(args)... };
}

template<class T, class... Args>
auto emplace_unique_coroutine(Args&&... args){
	using coroutine_type = coroutine<T>;
	return boost::make_unique<coroutine_type>(std::forward<Args>(args)...);
}

template<class T, class... Args>
auto emplace_shared_coroutine(Args&&... args){
	using coroutine_type = coroutine<T>;
	return boost::make_shared<coroutine_type>(std::forward<Args>(args)...);
}

template<class Ret>
struct coroutine_container{

	template<class Coroutine>
	coroutine_container& operator=(Coroutine&& co){
		return *this;
	}
};

// template<class Coroutine>
// struct generator_adapter
// {
// 	using coroutine_ptr = boost::shared_ptr<Coroutine>();
// 	using result_type = decltype(std::declval<Coroutine&>()());
// 	static_assert(!std::is_void<result_type>::value, "must not be void");

// 	struct iterator_type
// 	{
// 		generator_adapter& container_;

// 		result_type operator*(){
// 			return (*container_.co_)();
// 		}
// 		iterator_type operator++(int){
// 			return this;
// 		}

// 		friend bool operator==(const iterator_type& lhs, const iterator_type& rhs){
// 			return lhs.co_.is_complete();
// 		}
// 	};

// 	CoroutinePtr co_;

// 	generator_adapter(Coroutine&& co) : co_(std::move(co)){}

// 	iterator_type begin() { return { *this }; }
// 	iterator_type end() { return { *this }; }
// };

// template<class Coroutine>
// struct enumerator_adapter
// {
// 	using coroutine_ptr = boost::shared_ptr<Coroutine>();
// 	using result_type = decltype(std::declval<Coroutine&>()());

// 	enumerator_adapter(CoroutinePtr&& co) : co_(std::move(co)){}
// };


// void range(int min, int max){
// 	struct xxx{
// 		int min;
// 		int max;
// 		bool success;

// 		template<class Coroutine>
// 		int operator()(Coroutine* co){
// 			reenter(co){
// 				yield scenes.load(101, co.make_resume(success, null_lvalue));
// 				yield return min;
// 				for(; min <= max; ++min)
// 					yield return min;
// 			}
// 			return 0;
// 		}
// 	} ss;

// 	auto co = make_coroutine([a = 1](auto&& co, void** values){
// 		reenter(co){
// 			yield scenes.load(101, co.make_resume(success, null_lvalue));
// 			yield scenes.load(101, co.make_resume_raw<string, int>());
// 			get<string>(values[0]);

// 			yield return min;
// 			for(; min <= max; ++min)
// 				yield return min;
// 		}
// 		return 0;
// 	});
// }

