#pragma once

#include <type_traits>
#include <iterator>
#include <limits>

namespace ranged
{
	template <class C>
	using iterator_of = decltype(std::begin(std::declval<C>()));
	template <class C>
	using sentinel_of = decltype(std::end(std::declval<C>()));

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

	template <class T>
	class IntegerIncreaser
	{
		T _value;
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using reference = value_type;
		using pointer = value_type*;

		IntegerIncreaser(T value) : _value(value) { }

		IntegerIncreaser& operator++() { ++_value; return *this; }

		reference operator*() const { return _value; }

		bool operator!=(const IntegerIncreaser& other) const { return _value != other._value; }
	};

	struct Indices : public RangeOperator<Indices>
	{
		template <class C>
		Range<IntegerIncreaser<size_t>> operator()(const C& c) const { return { 0, c.size() }; }
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

	template <class ITA, class ITB>
	class PairIterator
	{
		using refA = typename std::iterator_traits<std::remove_reference_t<ITA>>::reference;
		using refB = typename std::iterator_traits<std::remove_reference_t<ITB>>::reference;

		ITA _ita;
		ITB _itb;
	public:
		using iterator_category = std::input_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = std::pair<refA, refB>;
		using reference = value_type;
		using pointer = value_type*;

		PairIterator(ITA ita, ITB itb) : _ita(std::forward<ITA>(ita)), _itb(std::forward<ITB>(itb)) { }

		PairIterator& operator++() { ++_ita; ++_itb; return *this; }

		reference operator*() const { return { *_ita, *_itb }; }

		template <class SA, class SB>
		bool operator!=(const PairEnd<SA, SB>& end) { return _ita != end.a && _itb != end.b; }
	};
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
				IntegerIncreaser<size_t>(0), 
				IntegerIncreaser<size_t>(std::numeric_limits<size_t>::max())), c); }
	};
	static constexpr Enumerator enumerate;

	class Delimiterator
	{
		std::string _delim;
		bool _first;
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = std::string;
		using reference = std::string_view;
		using pointer = value_type*;

		Delimiterator(std::string delim) : _delim(std::move(delim)) { }

		Delimiterator& operator++() { _first = false; return *this; }

		reference operator*() const { return _first ? std::string_view{} : _delim; }

		bool operator!=(End) const { return true; }
	};

	inline auto delimit(std::string delim) { return zip(Range<Delimiterator, End>{ std::move(delim), {} }); }

	template <template<class> class MetaIT>
	struct Getter : public RangeOperator<Getter<MetaIT>>
	{
		template <class C>
		Range<MetaIT<iterator_of<const C>>, sentinel_of<const C>> operator()(const C& c) const { return { std::begin(c), std::end(c) }; }
	};

	template <class IT>
	class KeyIterator
	{
		IT _it;
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = const decltype(std::declval<IT>()->first);
		using reference = value_type&;
		using pointer = value_type*;

		KeyIterator(IT it) : _it(std::move(it)) { }

		KeyIterator& operator++() { ++_it; return *this; }

		reference operator*() const { return _it->first; }

		template <class S>
		bool operator!=(const S& other) { return _it != other; }
	};
	static constexpr Getter<KeyIterator> keys;

	template <class IT>
	class ValueIterator
	{
		IT _it;
	public:
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = const decltype(std::declval<IT>()->second);
		using reference = value_type&;
		using pointer = value_type*;

		ValueIterator(IT it) : _it(std::move(it)) { }

		ValueIterator& operator++() { ++_it; return *this; }

		reference operator*() const { return _it->second; }

		template <class S>
		bool operator!=(const S& other) { return _it != other; }
	};
	static constexpr Getter<ValueIterator> values;

	template <class F>
	class Mapper : public RangeOperator<Mapper<F>>
	{
		F _f;
	public:
		Mapper(F f) : _f(std::move(f)) { }

		template <class IT, class F>
		class Iterator
		{
			IT _it;
			F _f;
		public:
			using iterator_category = std::forward_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using value_type = decltype(_f(*_it));
			using reference = value_type;
			using pointer = value_type*;

			Iterator(IT it, F f) : _it(std::move(it)), _f(std::move(f)) { }

			Iterator& operator++() { ++_it; return *this; }

			reference operator*() const { return _f(*_it); }

			template <class S>
			bool operator!=(const S& other) { return _it != other; }
		};

		template <class C>
		Range<Iterator<iterator_of<C>, F>, sentinel_of<C>>  operator()(const C& c) const { return { { std::begin(c), _f }, std::end(c) }; }
	};
	template <class F>
	Mapper<F> map(F f) { return { std::move(f) }; }

	template <class T>
	auto equals(T value) { return map([value](auto&& arg) { return arg == value; }); }

	template <class C>
	bool all(const C& c) { for (auto&& e : c) if (!e) return false; return true; }
	template <class C>
	bool any(const C& c) { for (auto&& e : c) if (e) return true; return false; }

	template <class C, class OP>
	auto operator|(const C& c, const RangeOperator<OP>& op) { return op(c); }
}
