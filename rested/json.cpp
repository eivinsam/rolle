#include "json.h"

namespace json
{
	std::string stringify(std::string_view text)
	{
		std::string escaped;
		escaped += '"';
		for (auto ch : text) switch (ch)
		{
		case '\\': escaped += "\\\\"; continue;
		case '"': escaped += "\\\""; continue;
		default: escaped += ch; continue;
		}
		escaped += '"';
		return escaped;
	}

	std::string stringify(double value)
	{
		auto result = std::to_string(value);
		while (result.back() == '0')
			result.pop_back();
		if (result.back() == '.')
			result.pop_back();
		return result;
	}

	std::string stringify(const details::ValueVariant & value)
	{
		return std::visit([](const auto& value) { return stringify(value); }, value);
	}

	template <class IT, class S>
	class Parser
	{
#pragma push_macro("WHITESPACE")
#pragma push_macro("TOKEN_STOP")
#pragma push_macro("DIGIT")
#define WHITESPACE ' ': case '\t': case '\n': case '\r'
#define TOKEN_STOP WHITESPACE: case ',': case ']': case '}'
#define DIGIT '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9'
		IT it;
		const S end;
	public:
		class Error : public std::runtime_error
		{
		public:
			using std::runtime_error::runtime_error;
		};

		Parser(IT it, S end) : it(it), end(end) { }

		Value operator()()
		{
			for (;;) switch (safe_next())
			{
			case WHITESPACE: ++it; continue;
			case 'n': assert_rest("null"); return nullptr;
			case 't': assert_rest("true"); return true;
			case 'f': assert_rest("false"); return false;
			case '-': case DIGIT:
				return parse_number();
			case '"': return parse_string();
			case '[': return parse_array();
			case '{': return parse_object();
			default:
				throw Error("Not a valid value");
			}
		}
	private:
		void assert_rest(const char* s)
		{
			for (; *s != 0; ++s, ++it)
				if (it == end || *s != *it)
					throw Error("Invalid value");
			if (it != end) switch (*it)
			{
			case TOKEN_STOP: return;
			default: throw Error("Invalid value");
			}
		}
		char safe_next() { return it == end ? ',' : *it; }

		struct ToDigit
		{
			char mask;
			char shift;

			ToDigit(bool negate) : mask(negate ? ~0 : 0), shift('0' + negate) { }

			char operator()(char ch) { return (ch - shift) ^ mask; }
		};
		double parse_number()
		{
			auto to_digit = [this]
			{
				const bool neg = *it == '-';
				if (neg) ++it;
				return ToDigit(neg);
			}();
			if (safe_next() == '0')
			{
				++it;
				switch (safe_next())
				{
				case TOKEN_STOP: return 0;
				case '.':           ++it; return parse_decimals(to_digit, 0);
				case 'E': case 'e': ++it; return parse_exponent(0);
				default: throw Error("Unexpected character while parsing number with initial zero");
				}
			}
			double n = 0;
			for (;;) switch (safe_next())
			{
			case TOKEN_STOP: return n;
			case DIGIT:
				n = n * 10 + to_digit(*it);
				++it;
				continue;
			case '.':           ++it; return parse_decimals(to_digit, n);
			case 'E': case 'e': ++it; return parse_exponent(n);
			default: throw Error("Unexpected character while parsing integer");
			}
		};
		double parse_decimals(ToDigit to_digit, double n)
		{
			double q = 1;
			switch (safe_next())
			{
			case TOKEN_STOP: throw Error("Number ended with Period");
			default: break;
			}
			for (;;) switch (safe_next())
			{
			case TOKEN_STOP:
				return n / q;
			case DIGIT:
				q *= 10;
				n = n * 10 + to_digit(*it);
				++it;
				continue;
			case 'E': case 'e': ++it; return parse_exponent(n / q);
			default: throw Error("Unexpected character while parsing decimal number");
			}
		};
		double parse_exponent(double n)
		{
			auto to_digit = [this]() -> ToDigit
			{
				switch (safe_next())
				{
				case TOKEN_STOP: throw Error("Number ended with exponent symbol");
				case '+': ++it; return false;
				case '-': ++it; return true;
				default: return false;
				}
			}();
			switch (safe_next())
			{
			case TOKEN_STOP: throw Error("Number ended with exponent sign");
			default: break;
			}
			int p = 0;
			while (it != end) switch (*it)
			{
			case TOKEN_STOP: break;
			case DIGIT: p = p * 10 + to_digit(*it);
			default: throw Error("Unexpected character while parsing exponent");
			}
			return n * pow(10.0, p);
		};
		std::string parse_string()
		{
			[this] {
				for (;;) switch (safe_next())
				{
				case WHITESPACE: ++it; continue;
				case '"': ++it; return;
				default: throw Error("Invalid start of string");
				}
			}();
			std::string result;
			while (it != end) switch (*it)
			{
			case '"': ++it; return result;
			case '\'':
				++it;
				switch (safe_next())
				{
				case '"': case '\\': case '/': result.push_back(*it); break;
				case 'b': result.push_back('\b'); break;
				case 'f': result.push_back('\f'); break;
				case 'n': result.push_back('\n'); break;
				case 'r': result.push_back('\r'); break;
				case 't': result.push_back('\t'); break;
				case 'u':
					throw Error("Unicode codepoints in strings not implemented");
				default:
					throw Error("Invalid escape character");
				}
				++it;
				continue;
			default:
				result.push_back(*it);
				++it;
				continue;
			}
			throw Error("Unexpected end of data while parsing string");
		};
		Array parse_array()
		{
			[this] {
				for (;;) switch (safe_next())
				{
				case WHITESPACE: ++it; continue;
				case '[': ++it; return;
				default: throw Error("Invalid start of array");
				}
			}();
			auto parse_item = [this]
			{
				Value result = operator()();
				while (it != end) switch (*it)
				{
				case WHITESPACE: ++it; continue;
				case ']':       return result;
				case ',': ++it; return result;
				default: throw Error("Invalid character after array item");
				}
				throw Error("Unexpected end of data while parsing array");
			};
			Array a;
			for (;;) switch (safe_next())
			{
			case WHITESPACE: ++it; continue;
			case ']': ++it; return a;
			case '}': case ',':
				throw Error("Invalid termination of array");
			default:
				a.emplace_back(parse_item());
				continue;
			}
		};
		Object parse_object()
		{
			[this] {
				for (;;) switch (safe_next())
				{
				case WHITESPACE: ++it; continue;
				case '{': ++it; return;
				default: throw Error("Invalid start of object");
				}
			}();
			auto parse_item = [this]
			{
				auto parse_value = [this]
				{
					Value result = operator()();
					while (it != end) switch (*it)
					{
					case WHITESPACE: ++it; continue;
					case '}': return result;
					case ',': ++it; return result;
					default: throw Error("Invalid character after object value");
					}
					throw Error("Unexpected end of data while parsing object");
				};
				std::string key = parse_string();
				for (;;) switch (safe_next())
				{
				case WHITESPACE: ++it; continue;
				case ':': ++it; return std::make_pair(key, parse_value());
				default: throw Error("Unexpected character after property name");
				}
			};
			Object o;
			for (;;) switch (safe_next())
			{
			case WHITESPACE: ++it; continue;
			case '}': ++it; return o;
			case ']': case ',':
				throw Error("Invaid termination of object");
			default:
				o.emplace_back(parse_item());
				continue;
			}
		};

#pragma pop_macro("DIGIT")
#pragma pop_macro("TOKEN_STOP")
#pragma pop_macro("WHITESPACE")
	};

	template <class IT, class S>
	Parser<IT, S> makeParser(IT it, S end) { return { std::move(it), std::move(end) }; }

	Value parse(std::string_view stored) { return makeParser(stored.begin(), stored.end())(); }
}
