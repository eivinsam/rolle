#pragma once

#include <iostream>
#include <map>

struct ConstructorProbe
{
	static const std::string& _name(const ConstructorProbe* object)
	{
		static std::string next_name = "A";
		static std::map<const ConstructorProbe*, std::string> objects;

		auto found = objects.find(object);
		if (found != objects.end())
			return found->second;

		auto in_map = objects.emplace(object, "");
		if (in_map.second)
		{
			in_map.first->second = next_name;
			next_name.back() += 1;
		}
		return in_map.first->second;
	}

	ConstructorProbe() { std::cout << "make " << _name(this) << "\n"; }
	ConstructorProbe(ConstructorProbe&& that) { std::cout << "move " << _name(&that) << " -> " << _name(this) << "\n"; }
	ConstructorProbe(const ConstructorProbe& that) { std::cout << "copy " << _name(&that) << " -> " << _name(this) << "\n"; }

	~ConstructorProbe() { std::cout << "destroy " << _name(this) << "\n"; }

	ConstructorProbe& operator=(ConstructorProbe&& that) { std::cout << "move " << _name(&that) << " -> " << _name(this) << "\n"; return *this; }
	ConstructorProbe& operator=(const ConstructorProbe& that) { std::cout << "copy " << _name(&that) << " -> " << _name(this) << "\n"; return *this; }
};
