#pragma once

#include <memory>
#include <map>
#include <string>
#include <sstream>
#include <string_view>

#include "pointers.h"

constexpr char read_digit(char ch);

void runServer(size_t thread_count);

#define ASIO_STANDALONE
#include <asio/ip/tcp.hpp>

static constexpr char CRLF[3] = { '\r', '\n', 0 };
static constexpr char SP = ' ';

template <class E, class U = std::underlying_type_t<E>>
U code(E e) { return static_cast<U>(e); }

enum class Method : char { Get, Head, Post, Put, Delete, Connect, Options, Trace };
inline std::string_view name(Method m)
{
	static const std::string_view names[] = { "GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE" };
	return names[code(m)];
}

enum class Charset : char { UTF8 };
inline std::string_view name(Charset cs)
{
	static const std::string_view names[] = { "utf-8" };
	return names[code(cs)];
}

enum class ContentType { TextPlain, TextHtml, TextCss, AppJson };
inline std::string_view name(ContentType ct)
{
	static const std::string_view names[] = { "text/plain", "text/html", "text/css", "application/json" };
	return names[code(ct)];
}

enum class Status : char
{
	// 100, 101
	OK, // 201...
	Found,	// 300...
		BadRequest, Unauthorized, Forbidden, NotFound, MethodNotAllowed, // 406...
		InternalError, NotImplemented, VersionNotSupported
};

inline auto& data(Status s)
{
	struct StatusData
	{
		short code;
		std::string_view name;
	};
	static const StatusData _data[] =
	{
		{ 200, "OK" },
		{ 302, "Found "},
		{ 400, "Bad Request" },
		{ 401, "Unauthorized" },
		{ 403, "Forbidden" },
		{ 404, "NotFound" },
		{ 405, "Method Not Allowed" },
		{ 500, "Internal Server Error" },
		{ 501, "Not Implemented" },
		{ 505, "HTTP Version Not Supported" }
	};
	return _data[static_cast<char>(s)];
}

inline short code(Status s) { return data(s).code; }
inline std::string_view name(Status s) { return data(s).name; }

class InvalidRequest : public std::runtime_error
{
public:
	InvalidRequest(const char* details) : std::runtime_error(details) { }
	InvalidRequest(std::string details) : std::runtime_error(details) { }
};

class UriPath : public std::vector<std::string>
{
	using std::vector<std::string>::vector;
};
inline std::ostream& operator<<(std::ostream& out, const UriPath& path)
{
	out << '/';
	for (auto it = path.begin(); it != path.end(); ++it)
	{
		if (it != path.begin())
			out << '/';
		out << *it;
	}
	return out;
}
class UriQuery : public std::vector<std::pair<std::string, std::string>>
{
	using std::vector<std::pair<std::string, std::string>>::vector;
};
inline std::ostream& operator<<(std::ostream& out, const UriQuery& query)
{
	for (auto it = query.begin(); it != query.end(); ++it)
		out << (it == query.begin() ? '?' : '&') << it->first << '=' << it->second;
	return out;
}
UriQuery parseQueryText(std::string_view query);


class Request
{
	using tcp = asio::ip::tcp;
public:
	Method method;
	UriPath location;
	UriQuery query;
	std::string body;
	std::map<std::string, std::string> fields;

	Request(tcp::iostream& stream);
};

class Response
{
	using tcp = asio::ip::tcp;
	std::stringstream _buf;
	std::vector<std::pair<std::string, std::string>> _fields;
public:

	Status      status      = Status::NotFound;
	ContentType contentType = ContentType::TextPlain;
	Charset     charset     = Charset::UTF8;

	auto rdbuf() const { return _buf.rdbuf(); }
	template <class Arg>
	Response& operator<<(Arg&& arg) { _buf << std::forward<Arg>(arg); return *this; }

	void set(std::string field, std::string value) { _fields.emplace_back(std::move(field), std::move(value)); }

	void send(tcp::iostream& stream);
};

using SegmentIterator = UriPath::const_iterator;
class Location
{
protected:
	using tcp = asio::ip::tcp;
public:


	virtual ~Location() = default;

	virtual void handle(const Request&, SegmentIterator, Response&) = 0;
};

class Folder : public Location
{
	std::string _dir;
public:
	Folder(std::string dir) : _dir(dir) { }

	void handle(const Request&, SegmentIterator, Response&) override;
};

class VirtualFolder : public Location
{
	std::map<std::string, shared<Location>> _dir;
public:

	void addLocation(std::string name, shared<Location> loc)
	{
		_dir.emplace(std::move(name), std::move(loc));
	}
	void handle(const Request&, SegmentIterator, Response&) override;
};

extern VirtualFolder serverRoot;
