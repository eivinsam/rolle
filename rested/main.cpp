#include "json.h"

#include "server.h"
#include "database.h"

#include "range.h"
#include "string.h"

#include <algorithm>
#include <cstdint>
#include <array>

using namespace db;

template <class T>
static std::ostream& operator<<(std::ostream& out, const std::vector<T>& v)
{
	static const char* comma = ", ";
	const char* delim = "";
	out << "[";
	for (auto&& e : v)
	{
		out << delim << e;
		delim = comma;
	}
	return out << "]";
}

class TableLocation : public Location
{
	std::shared_ptr<Database> _db;
	std::string _table;

	template <class T>
	static std::enable_if_t<std::is_integral_v<T>, std::string> 
		intsToString(T value) { return std::to_string(value); }
	template <class T>
	static std::enable_if_t<!std::is_integral_v<T>, T>
		intsToString(T value) { return value; }

	static constexpr struct
	{
		using R = db::Value;
		R operator()(nullptr_t) const { return nullptr; }
		R operator()(bool v) const { return int(v); }
		R operator()(double v) const { return v; }
		R operator()(const std::string& v) const { return v; }
		R operator()(const json::Array& v) const { return json::stringify(v); }
		R operator()(const json::Object& v) const { return json::stringify(v); }
	} storeJson{};

	db::Value _store_json(const json::Value& value)
	{
		using std::get_if;
		if (get_if<nullptr_t>(&value))
			return nullptr;
		if (auto v = get_if<bool>(&value))
			return int(*v);
		if (auto v = get_if<double>(&value))
			return *v;
		if (auto v = get_if<std::string>(&value))
			return *v;
		throw std::runtime_error("Cannot store json arrays or objects");
	}

	void _json_result(Response& res, Query&& q) { _json_result(res, q); }
	void _json_result(Response& res, Query& q)
	{
		using namespace ranged;

		std::vector<json::Object> data;

		for (auto&& row : q)
		{
			data.emplace_back();
			for (auto&& c : row)
				std::visit([&](auto v) { data.back().emplace_back(std::string(c.name()), intsToString(std::move(v))); }, c.value());
		}

		res.status = Status::OK;
		res.contentType = ContentType::AppJson;
		res << json::stringify(data);
		std::cout << "sent: \n" << res.rdbuf() << "\n";
	}
public:
	TableLocation(shared<Database> db, std::string table) : 
		_db(std::move(db)), _table(std::move(table))  { }

	void handle(const Request& request, SegmentIterator seg, Response& res) override
	{
		using namespace ranged;
		std::cout << name(request.method) << " table " << _table << ": ";
		for (auto& s : range(seg, request.location.end()))
			std::cout << s << '/';
		std::cout << "\n";

		res.status = Status::NotFound;

		int id = 0;

		if (seg != request.location.end())
		{
			if (isdigit(seg->front()))
			{
				for (auto d : *seg | map(read_digit))
					if (d < 10)
						id = id * 10 + d;
					else
						return;
				++seg;
			}
		}

		std::string_view columns;
		if (seg != request.location.end())
		{
			columns = *seg;
			++seg;
		}

		if (seg != request.location.end() || !all(columns | map([](char c) { return isalpha(c) || c == ','; })))
			return;

		switch (request.method)
		{
		case Method::Get: 
			if (columns.empty() && request.query.empty())
			{
				auto select_all = _db->selectAll().from(_table);
				_json_result(res, id == 0 ? Query(select_all) : Query(std::move(select_all).where({ equal("id", id) })) );
			}
			else
			{
				auto select = (columns.empty() ? _db->selectAll() : _db->select(columns | split(","))).from(_table);
				_json_result(res, columns.empty() ? 
					Query(select) : 
					Query(std::move(select).where(request.query | map([](auto&& kv) { return equal(kv.first, kv.second); }))));
			}
			return;
		case Method::Put:
			if (id == 0 || columns.empty() || !request.query.empty())
				res.status = Status::MethodNotAllowed;
			else
			{

				auto body = json::parse(request.body);
				auto split_columns = ranged::flatten(columns | split(","));
				std::cout << "want to put '" << request.body << "' into columns " << split_columns << "\n";

				if (split_columns.size() != 1)
				{
					res.status = Status::NotImplemented;
				}
				else if (auto values = std::get_if<json::Array>(&body))
				{
					res.status = Status::NotImplemented;
				}
				else 
				{
					_db->update(_table)
						.set({ db::equal(std::string(split_columns[0]), _store_json(body)) })
						.where({ equal("id", std::to_string(id)) })
						.exec();
					res.status = Status::OK;
				}
				//std::string query_text = "UPDATE " + _table + " SET ";
				//for (auto&& kd : query | keys | delimit(", "))
				//	((query_text += kd.second) += kd.first) += " = ?";
				//(query_text += " WHERE id = ") += std::to_string(id);
				//std::cout << query_text << "\n";
				//auto q = _db->query(query_text);
				//for (auto&& iv : query | values | enumerate)
				//	q.bind(iv.first+1, iv.second);
				//q.exec();
			}
			break;
		default:
			res.status = Status::MethodNotAllowed;
			return;
		}
	}
};

//class LoginLocation : public Location
//{
//
//public:
//	void handle(const Request& request, std::string_view location, Response& res) override
//	{
//		auto sub = pop(location, '?');
//		if (!sub.empty() && request.method != Method::Get)
//		{
//			res.status = Status::MethodNotAllowed;
//			return;
//		}
//		switch (request.method)
//		{
//		//case Method::Get: 
//		case Method::Post:
//
//		default:
//			res.status = Status::MethodNotAllowed;
//			return;
//		}
//	}
//};

int main(int argc, char* argv[])
{
	using std::make_shared;

	auto db = std::make_shared<Database>("rested.db");

	try
	{
		serverRoot.addLocation("places", make_shared<TableLocation>(db, "places"));
		serverRoot.addLocation("characters", make_shared<TableLocation>(db, "characters"));
		serverRoot.addLocation("interface", make_shared<Folder>("interface"));
	}
	catch (std::exception& e)
	{
		std::cout << "Database error: " << e.what() << "\n";
	}

	runServer(4);

	return 0;
}
