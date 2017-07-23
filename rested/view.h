#pragma once

template <class T>
class View
{
	const T& _ref;
public:
	View(const T& ref) : _ref(ref) { }

	operator const T&() const { return _ref; }
};

template <class T>
using ViewList = std::initializer_list<View<T>>;
