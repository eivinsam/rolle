#include "server.h"

#include "range.h"

#include <ctime>
#include <thread>
#include <iostream>
#include <asio.hpp>
#include <fstream>
#include <filesystem>

using asio::ip::tcp;

std::string_view pop(std::string_view& text, char delim)
{
	auto result = text.substr(0, text.find_first_of(delim));
	text.remove_prefix(std::min(text.size(), result.size() + 1));
	return result;
}


constexpr char read_digit(char ch)
{
	constexpr char NV = 36; // Not Valid
	constexpr char digit_code[256] =
	{
		//  x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xA  xB  xC  xD  xE  xF
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, // 0x
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, // 1x
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, // 2x
		0,  1,  2,  3,  4,  5,  6,  7,  8,  9, NV, NV, NV, NV, NV, NV, // 3x
		NV, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, // 4x
		25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, NV, NV, NV, NV, NV, // 5x
		NV, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, // 6x
		25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, NV, NV, NV, NV, NV, // 7x
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, // 8x
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, // 9x
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, // Ax
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, // Bx
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, // Cx
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, // Dx
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, // Ex
		NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, NV, // Fx
	};
	return digit_code[static_cast<unsigned char>(ch)];
}

template <size_t N>
class Bitset
{
	static constexpr size_t log2(size_t n)
	{
		size_t result = 0;
		if (n > 0xffffffff) { result |= 0x20; n >>= 0x20; }
		if (n >     0xffff) { result |= 0x10; n >>= 0x10; }
		if (n >       0xff) { result |= 0x08; n >>= 0x08; }
		if (n >        0xf) { result |= 0x04; n >>= 0x04; }
		if (n >        0x3) { result |= 0x02; n >>= 0x02; }
		if (n >        0x1) { result |= 0x01; n >>= 0x01; }
		return result;
	}
	static constexpr size_t bits = sizeof(unsigned) * 8;
	static constexpr size_t p = log2(bits);
	static constexpr size_t mask = (1 << p) - 1;
	static constexpr size_t M = (N + bits - 1) >> p;
	unsigned _data[M] = { 0 };
public:
	constexpr Bitset() = default;
	template <class T>
	constexpr Bitset(std::initializer_list<T> data)
	{
		auto it = data.begin();
		const auto end = data.end();
		for (size_t k = 0; k < M; ++k)
			for (size_t j = 0; j < bits && it != end; ++j, ++it)
				_data[k] |= bool(*it) << j;
	}
	template <class P>
	static constexpr Bitset fromPredicate(P&& pred)
	{
		Bitset result;
		size_t i = 0;
		for (size_t k = 0; k < M; ++k)
			for (size_t j = 0; j < bits && i < N; ++j, ++i)
				result._data[k] |= pred(i) << j;
		return result;
	}

	constexpr bool operator[](size_t i) const { return (_data[i >> p] >> (i&mask)) & 1; }
};


std::string unescape_http(std::string_view escaped)
{
	struct http
	{
		static constexpr bool is_valid(size_t ch)
		{
			return
				(ch >= '0' && ch <= '9') ||
				(ch >= 'A' && ch <= 'Z') ||
				(ch >= 'a' && ch <= 'z') ||
				ch == '-' || ch == '.' || ch == '~' || ch == '_' || ch == ',';
		}
	};
	static constexpr auto valid_byte = Bitset<256>::fromPredicate(http::is_valid);
	auto pop_hex_digit = [](auto& it, auto& end)
	{
		if (++it == end) throw InvalidRequest("Missing hex digit after %");
		char result = read_digit(*it);
		if (result >= 16) throw InvalidRequest("Invalid digit character after %");
		return result;
	};
	std::string result;
	const auto end = escaped.end();
	for (auto it = escaped.begin(); it != end; ++it)
	{
		if (*it == '%')
			result.push_back((pop_hex_digit(it, end) << 4) | pop_hex_digit(it, end));
		else if (valid_byte[static_cast<unsigned char>(*it)])
			result.push_back(*it);
		else
			throw InvalidRequest("Encountered reserved character");
	}
	return result;
}

UriQuery parseQueryText(std::string_view query)
{
	UriQuery result;
	while (!query.empty())
	{
		auto value = pop(query, '&');
		auto key = pop(value, '=');
		for (auto ch : key) 
			if (!isalnum(ch)) throw InvalidRequest("Query key was not purely alphanueric");
		for (auto ch : value)
		{
			if (ch == '/') throw InvalidRequest("Slash in query value");
			if (ch == '&') throw InvalidRequest("Ampersand in query value");
		}
			
		result.emplace_back(key, unescape_http(value));
	}
	return result;
}
auto parseLocationText(std::string_view text)
{
	UriPath result;
	while (!text.empty())
	{
		while (!text.empty() && text.front() == '/')
			text.remove_prefix(1);
		if (text.empty())
			break;
		result.emplace_back(unescape_http(pop(text, '/')));
		if (result.back() == ".")
			result.pop_back();
		else if (result.back() == ".." && result.size() > 1)
		{
			result.pop_back();
			result.pop_back();
		}
	}
	return result;
}

std::string getToken(tcp::iostream& stream, char delim)
{
	std::string result;
	std::getline(stream, result, delim);
	return result;
}
std::string getWord(tcp::iostream& stream) { return getToken(stream, SP); }
std::string getLine(tcp::iostream& stream)
{
	auto result = getToken(stream, CRLF[0]);
	if (stream.get() != CRLF[1])
		throw InvalidRequest("Missing LF after CR in header");
	return result;
}
std::pair<std::string, std::string> getKeyValue(tcp::iostream& stream)
{
	std::pair<std::string, std::string> kv = { {}, getLine(stream) };
	if (kv.second.empty())
		return kv;
	auto found = kv.second.find_first_of(':');
	if (found == std::string::npos)
		throw InvalidRequest("Missing colon in header field");
	if (kv.second[found + 1] != ' ')
		throw InvalidRequest("Missing space after colon in header field");
	kv.first.assign(kv.second.begin(), kv.second.begin() + found);
	kv.second.assign(kv.second.begin() + found + 2, kv.second.end());
	return kv;
}

Method getMethod(tcp::iostream& stream)
{
	auto token = getWord(stream);
	std::cout << token << "\n";
	if (token.size() >= 3) switch (token[0])
	{
	case 'C': if (token == "CONNECT") return Method::Connect; break;
	case 'D': if (token == "DELETE")  return Method::Delete;  break;
	case 'G': if (token == "GET")     return Method::Get;     break;
	case 'H': if (token == "HEAD")    return Method::Head;    break;
	case 'O': if (token == "OPTIONS") return Method::Options; break;
	case 'T': if (token == "TRACE")   return Method::Trace;   break;
	case 'P':
		if (token == "POST") return Method::Post;
		if (token == "PUT")  return Method::Put;
		break;
	default: break;
	}
	throw InvalidRequest("Invalid request method: '" + token + "'");
}

template <char C>
bool is(char ch) { return ch == C; }

Request::Request(tcp::iostream & stream)
{
	method = getMethod(stream);
	
	{
		auto loc_text = getWord(stream);
		auto que = std::string_view(loc_text);
		location = parseLocationText(pop(que, '?'));
		query = parseQueryText(que);
	}

	auto version = getLine(stream);

	if (version != "HTTP/1.1")
		std::cout << "warning: deviant HTTP version: " << version << "\n";

	std::cout << name(method) << ": " << location << "\n";
	
	for (auto kv = getKeyValue(stream); !kv.first.empty(); kv = getKeyValue(stream))
	{
		auto it = fields.emplace(std::move(kv)).first;
	}
	auto content_length = fields.find("Content-Length");
	if (content_length == fields.end())
	{
		auto transfer_encoding = fields.find("Transfer-Encoding");
		if (transfer_encoding != fields.end() && transfer_encoding->second == "Chuncked")
			throw InvalidRequest("Chencked transfer not supported");
		return;
	}
	body.resize(std::stoi(content_length->second));
	stream.read(body.data(), body.size());
}

std::string current_time()
{
	std::string result;
	result.resize(128);
	time_t now = time(0);
	tm now_tm;
	gmtime_s(&now_tm, &now);
	result.resize(strftime(result.data(), result.size(), "%a, %d %b %Y %H:%M:%S GMT", &now_tm));
	return result;
}

void Response::send(tcp::iostream & stream)
{
	_buf.seekg(0, std::ios::end);
	auto content_length = size_t(_buf.tellg());
	_buf.seekg(0, std::ios::beg);

	stream << "HTTP/1.1" << SP << code(status) << SP << name(status) << CRLF;
	stream << "Date: " << current_time() << CRLF;
	stream << "Connection: close" << CRLF;
	stream << "Server: rested/0.0" << CRLF;
	for (auto& fv : _fields)
		stream << fv.first << ": " << fv.second << CRLF;
	if (content_length > 0)
	{
		stream << "Content-Language: en" << CRLF;
		stream << "Content-Type: " << name(contentType) << "; charset=" << name(charset) << CRLF;
		stream << "Content-Length: " << content_length << CRLF;
	}
	stream << CRLF;
	stream << _buf.rdbuf();
}

VirtualFolder serverRoot;

class ClientHandler
{
	std::shared_ptr<tcp::iostream> _stream;
public:
	ClientHandler(std::shared_ptr<tcp::iostream> stream) : _stream(std::move(stream)) { }

	void operator()()
	{
		using namespace ranged;
		Response response;
		try
		{
			Request request(*_stream);

			std::cout << name(request.method) << SP << request.location << "\n";
			if (!request.query.empty())
			{
				std::cout << '?';
				for (auto&& kvd : request.query | delimit("&"))
					std::cout << kvd.second << kvd.first.first << '=' << kvd.first.second;
			}

			serverRoot.handle(request, request.location.begin(), response);
		}
		catch (InvalidRequest& invalid)
		{
			response.status = Status::BadRequest;
			response  << "Invalid request: " << invalid.what() << "\n";
			std::cout << "Invalid request: " << invalid.what() << "\n";
		}
		catch (std::exception& e)
		{
			response.status = Status::InternalError;
			std::cout << "Exception while handling request: " << e.what() << "\n";
		}
		response.send(*_stream);
	}
};
class Listener
{
	tcp::acceptor _acceptor;

	void _accept()
	{
		auto stream = std::make_shared<tcp::iostream>();

		_acceptor.async_accept(*stream->rdbuf(), [this, stream](const auto& error)
		{
			if (!error)
				_acceptor.get_io_service().post(ClientHandler(stream));
			_accept();
		});

	}
public:
	Listener(asio::io_service& io) : _acceptor(io, tcp::endpoint(tcp::v4(), 8888))
	{
		_accept();
	}
};

std::unique_ptr<asio::io_service> g_io;

static void handle_termination(int)
{
	std::cout << "trying to terminate gracefully\n";
	g_io->stop();
}

static void handle_exception(std::exception& e)
{
	std::cout << "noes: " << e.what() << "\n";
	g_io->stop();
}

void runServer(size_t thread_count)
{
	signal(SIGINT, handle_termination);

	std::vector<std::thread> threads;
	try
	{
		g_io = std::make_unique<asio::io_service>();
		Listener listener(*g_io);

		for (size_t i = 0; i < thread_count - 1; ++i)
			threads.emplace_back([&]
		{
			try { g_io->run(); }
			catch (std::exception& e) { handle_exception(e); }
		});
		g_io->run();
	}
	catch (std::exception& e)
	{
		handle_exception(e);
	}
	for (auto&& thread : threads)
		thread.join();
}



void VirtualFolder::handle(const Request & request, SegmentIterator seg, Response& res)
{
	if (seg != request.location.end())
	{
		auto found = _dir.find(*seg);
		if (found == _dir.end())
		{
			res << "404 / file not found";		}
		else
			found->second->handle(request, seg + 1, res);
	}
	else
	{
		if (request.method == Method::Get)
		{
			res.status = Status::OK;
			res.contentType = ContentType::TextHtml;
			res << "<html><head><title>Directory " << request.location << "</title></head><body>";
			for (auto& kv : _dir)
				res << "<p><a href='" << kv.first << "'>" << kv.first << "</a></p>";
			res << "</body></html>";
		}
		else
		{
			res.status = Status::MethodNotAllowed;
		}
	}
}

namespace std
{
	namespace fs
	{
		using namespace std::experimental::filesystem;
	}
}

void fileResponse(Response& res, const std::fs::path& path, ContentType content_type)
{
	std::ifstream file(path.string(), std::ios::binary);

	res.status = Status::OK;
	res.contentType = content_type;

	res << file.rdbuf();
}
void fileResponse(Response& res, const std::fs::path& path)
{
	static const std::map<std::fs::path, ContentType> by_extension =
	{
		{ ".html", ContentType::TextHtml },
		{ ".css", ContentType::TextCss },
		{ ".js" , ContentType::AppJson }
	};

	auto found = by_extension.find(path.extension());
	
	fileResponse(res, path, found != by_extension.end() ? found->second : ContentType::TextPlain);
}


void Folder::handle(const Request& request, SegmentIterator seg, Response& res)
{
	std::fs::path path = _dir;

	while (seg != request.location.end())
	{
		path /= *seg;
		++seg;
	}

	if (std::fs::is_directory(path))
	{
		if (std::fs::is_regular_file(path / "index.html"))
		{
			path /= "index.html";
			res.status = Status::Found;
			res.set("Location", path.string());
		}
	}
	else if (std::fs::is_regular_file(path))
	{
		fileResponse(res, path);
	}
	else
	{
		std::cout << "file not found: " << path;
	}
}
