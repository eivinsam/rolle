#pragma once

#include <type_traits>
#include <iterator>
#include <limits>

namespace ranged
{
	namespace details
	{
		template <typename Category, class IT>
		class As : public IT
		{
		public:
			using iterator_category = Category;
			using difference_type = std::ptrdiff_t;
			using reference = decltype(std::declval<IT>().operator*());
			using value_type = std::remove_reference_t<reference>;
			using pointer = value_type*;

			using IT::IT;

			As& operator++() { IT::operator++(); return *this; }

			template <class T>
			bool operator==(const T& other) { return !(reinterpret_cast<IT*>(this) != other); }
		};
	}

	template <class C>
	using iterator_of = decltype(std::begin(std::declval<C>()));
	template <class C>
	using sentinel_of = decltype(std::end(std::declval<C>()));

	template <class IT>
	using category_of = typename std::iterator_traits<IT>::iterator_category;
	template <class IT>
	using value_type_of = typename std::iterator_traits<IT>::value_type;

	template <class IT>
	decltype(auto) value_declval() { return std::declval<value_type_of<IT>>(); }

	template <class T>
	struct RangeOperator
	{
		template <class C>
		auto operator()(const C& c) const { return static_cast<const T&>(*this)(c); }
	};

	template <class IT, class S = IT>
	class Range
	{
		IT _first;
		S  _last;
	public:
		Range(IT first, S last) : _first(std::move(first)), _last(std::move(last)) { }

		IT begin() const { return _first; }
		S  end()   const { return _last; }
	};

	template <class IT, class S>
	Range<IT, S> range(IT first, S last) { return { first, last }; }

	struct End {};

	namespace impl
	{
		template <class T>
		class Increaser
		{
			T _value;
		public:
			Increaser(T value) : _value(value) { }

			Increaser& operator++() { ++_value; return *this; }

			const T& operator*()  const { return  _value; }
			const T* operator->() const { return &_value; }

			bool operator!=(const Increaser& other) const { return _value != other._value; }
		};
	}
	template <class T>
	using Increaser = details::As<std::forward_iterator_tag, impl::Increaser<T>>;

	struct Indices : public RangeOperator<Indices>
	{
		template <class C>
		Range<Increaser<size_t>> operator()(const C& c) const { return { 0, c.size() }; }
	};
	static constexpr Indices indices;

	template <class EA, class EB>
	struct PairEnd
	{
		EA a;
		EB b;
	};
	template <class CA, class CB>
	PairEnd<sentinel_of<const CA>, sentinel_of<const CB>> 
		pair_end(const CA& ca, const CB& cb) 
	{ return { std::end(ca), std::end(cb) }; }

	namespace impl
	{
		template <class ITA, class ITB>
		class PairIterator
		{
			using refA = typename std::iterator_traits<ITA>::reference;
			using refB = typename std::iterator_traits<ITB>::reference;

			ITA _ita;
			ITB _itb;
		public:
			PairIterator(ITA ita, ITB itb) : _ita(std::forward<ITA>(ita)), _itb(std::forward<ITB>(itb)) { }

			PairIterator& operator++() { ++_ita; ++_itb; return *this; }

			std::pair<refA, refB> operator*() const { return { *_ita, *_itb }; }

			template <class SA, class SB>
			bool operator!=(const PairEnd<SA, SB>& end) { return _ita != end.a && _itb != end.b; }
		};
	}
	template <class ITA, class ITB>
	using PairIterator = details::As<std::input_iterator_tag, impl::PairIterator<ITA, ITB>>;

	template <class CA, class CB>
	PairIterator<iterator_of<const CA>, iterator_of<const CB>> 
		pair_begin(const CA& ca, const CB& cb) 
	{ return { std::begin(ca), std::begin(cb) }; }

	template <class CB>
	struct Zipper : public RangeOperator<Zipper<CB>>
	{
		const CB& second;
		Zipper(const CB& second) : second(second) { }

		template <class CA> 
		auto operator()(const CA& first) const { return range(pair_begin(first, second), pair_end(first, second)); }
	};
	template <class IT, class S>
	struct Zipper<Range<IT, S>> : public RangeOperator<Zipper<Range<IT, S>>>
	{
		Range<IT, S> second;
		Zipper(Range<IT, S> second) : second(std::move(second)) { }

		template <class CA>
		auto operator()(const CA& first) const { return range(pair_begin(first, second), pair_end(first, second)); }
	};
	template <class CB>
	Zipper<CB> zip(const CB& second) { return { second }; }
	template <class CA, class CB>
	auto zip(const CA& first, const CB& second) { return Zipper<CB>(second)(first); }

	struct Enumerator : public RangeOperator<Enumerator>
	{
		template <class C>
		auto operator()(const C& c) const
		{ 
			return zip(range(
				Increaser<size_t>(0), 
				Increaser<size_t>(std::numeric_limits<size_t>::max())), c); }
	};
	static constexpr Enumerator enumerate;

	namespace impl
	{
		class Delimiterator
		{
			static constexpr std::string_view _empty{};
			std::string _delim;
			bool _first;
		public:
			Delimiterator(std::string delim) : _delim(std::move(delim)) { }

			Delimiterator& operator++() { _first = false; return *this; }

			std::string_view operator*() const { return _first ? _empty : _delim; }

			bool operator!=(End) const { return true; }
		};
	}
	using Delimiterator = details::As<std::forward_iterator_tag, impl::Delimiterator>;

	inline auto delimit(std::string delim) { return zip(Range<Delimiterator, End>{ std::move(delim), {} }); }

	template <class Map>
	struct Mapper : public RangeOperator<Mapper<Map>>
	{
		Map _map;
		template <class IT>
		class IteratorImpl
		{
			IT _it;
			Map _map;
		public:
			IteratorImpl(Map map, IT it) : _map(std::move(map)), _it(std::move(it)) { }

			IteratorImpl& operator++() { ++_it; return *this; }

			decltype(auto) operator*() const { return _map(*_it); }

			template <class S>
			bool operator!=(const S& other) { return _it != other; }
		};
	public:
		template <class IT>
		using Iterator = details::As<category_of<IT>, IteratorImpl<IT>>;

		Mapper() = default;
		Mapper(Map map) : _map(std::move(map)) { }

		template <class C>
		Range<Iterator<iterator_of<const C>>, sentinel_of<const C>> operator()(const C& c) const { return { { _map, std::begin(c) }, std::end(c) }; }
	};

	namespace details
	{
		struct KeyGetter
		{
			template <class T>
			decltype(auto) operator()(T&& value) const { return value.first; }
		};
		struct ValueGetter
		{
			template <class T>
			decltype(auto) operator()(T&& value) const { return value.second; }
		};
	}

	static constexpr Mapper<details::KeyGetter> keys;
	static constexpr Mapper<details::ValueGetter> values;

	template <class F>
	Mapper<F> map(F f) { return { std::move(f) }; }
	template <class F>
	auto mapPair(F f) { return map([f](auto&& pair) { return f(pair.first, pair.second); }); }

	template <class T>
	auto equals(T value) { return map([value](auto&& arg) { return arg == value; }); }

	template <class C>
	auto flatten(const C& c)
	{
		std::vector<std::decay_t<decltype(*std::begin(c))>> result; 
		for (auto&& e : c)
			result.push_back(e);
		return result;
	}

	template <class C>
	bool all(const C& c) { for (auto&& e : c) if (!e) return false; return true; }
	template <class C>
	bool any(const C& c) { for (auto&& e : c) if (e) return true; return false; }

	template <class C, class OP>
	auto operator|(const C& c, const RangeOperator<OP>& op) { return op(c); }
}
