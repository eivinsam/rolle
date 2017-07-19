#pragma once

#include <variant>
#include <vector>
#include <string>

namespace json
{
	class Value;

	using Array = std::vector<Value>;
	using Object = std::vector<std::pair<std::string, Value>>;

	namespace details
	{
		using ValueVariant = std::variant<nullptr_t, bool, double, std::string, Array, Object>;
	}

	class Value : public details::ValueVariant
	{
	public:
		using details::ValueVariant::variant;
	};


	Value parse(std::string_view stored);


	std::string stringify(std::string_view text);

	inline std::string stringify(nullptr_t) { return "null"; }
	inline std::string stringify(bool value) { return value ? "true" : "false"; }
	inline std::string stringify(const std::string& text) { return stringify(std::string_view{ text }); }

	std::string stringify(double value);
	std::string stringify(const details::ValueVariant& value);


	template <class T>
	std::string stringify(const std::vector<T>& array)
	{
		static const auto comma = ", ";
		auto delim = "[ ";
		std::string result;

		for (auto&& e : array)
		{
			result.append(delim).append(stringify(e));
			delim = comma;
		}
		result.append(" ]");
		return result;
	}
	template <class T>
	std::string stringify(const std::vector<std::pair<std::string, T>>& object)
	{
		static const auto comma = ", ";
		auto delim = "{ ";
		std::string result;

		for (auto&& e : object)
		{
			result.append(delim).append(stringify(e.first)).append(": ").append(stringify(e.second));
			delim = comma;
		}
		result.append(" }");
		return result;
	}

}
