#pragma once

#include <string>
#include <algorithm>

#include "range.h"

class Splitter
{
	std::string_view _current;
	std::string_view _rest;
	const char* _delim;
	bool _done = false;

	void _update_current() 
	{ 
		_current = _rest.substr(0, _rest.find_first_of(_delim)); 
	}
public:
	using iterator_category = std::forward_iterator_tag;
	using difference_type = std::ptrdiff_t;
	using value_type = const std::string_view;
	using reference = value_type&;
	using pointer = value_type*;

	Splitter(std::string_view src, const char* delim) : _rest(src), _delim(delim) 
	{ 
		_update_current();
	}

	Splitter& operator++()
	{
		if (_rest.size() == _current.size())
		{
			_done = true;
		}
		else
		{
			_rest.remove_prefix(_current.size() + 1);
			_update_current();
		}
		return *this;
	}

	reference operator*() const { return _current; }
	pointer operator->() const { return &_current; }

	bool operator!=(ranged::End) { return !_done; }
};

struct split
{
	const char* delim;

	split(const char* delim) : delim(delim) { }

	ranged::Range<Splitter, ranged::End> operator()(std::string_view src) { return { { src, delim }, {} }; }
};

auto operator|(std::string_view src, split s) { return s(src); }
