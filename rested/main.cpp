#include "server.h"
#include "database.h"

#include "range.h"

#include <algorithm>
#include <cstdint>
#include <array>

class TableLocation : public Location
{
	std::shared_ptr<Database> _db;
	std::string _table;
	Query _select_all;
	Query _by_id;

	void _json_result(Response& res, Query&& q) { _json_result(res, q); }
	void _json_result(Response& res, Query& q)
	{
		using namespace ranged;
		res.status = Status::OK;
		res.contentType = ContentType::AppJson;
		res << "[";
		std::cout << q.sql() << "\n";
		for (auto&& rd : q | delimit(","))
		{
			res << rd.second << "\n  { ";
			for (auto&& cd : rd.first | delimit(", "))
			{
				res << cd.second << '"' << cd.first.name() << "\": ";
				switch (cd.first.type())
				{
				case SQLITE_NULL: res << "null"; break;
				case SQLITE_INTEGER: res << cd.first.integer(); break;
				default:
					res << '"' << cd.first.text() << '"'; 
					break;
				}
			}
			res << " }";
		}
		res << "\n]";
		std::cout << "sent: \n" << res.rdbuf() << "\n";
	}
	void _post(Response& res, const UriQuery& query)
	{
		using namespace ranged;
		std::string query_text = "INSERT INTO " + _table + "(";

		for (auto&& kd : query | keys | delimit(", "))
			(query_text += kd.second) += kd.first;
		query_text += ") VALUES (";
		for (auto&& kvd : query | delimit(", "))
			(query_text += kvd.second) += "?";
		query_text += ")";

		std::cout << query_text << "\n";
		auto q = _db->query(query_text);
		for (auto&& iv : query | values | enumerate)
			q.bind(iv.first + 1, iv.second);
		q.exec();
		_json_result(res, _by_id(_db->lastInsert()));
	}

public:
	TableLocation(shared<Database> db, std::string table) : 
		_db(std::move(db)), _table(std::move(table))
	{
		_select_all = _db->query("SELECT * FROM " + _table);
		_by_id = _db->query("SELECT * FROM " + _table + " WHERE id = ?");
	}

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
				_json_result(res, id == 0 ? _select_all : _by_id(id));
			else
			{
				std::string query_text = "SELECT " + std::string(columns.empty() ? "*" : columns) + " FROM " + _table;
				if (id != 0)
					((query_text += " WHERE id = '") += std::to_string(id)) += "'";
				for (auto& kv : request.query)
				{
					query_text += (id != 0 ? " AND " : " WHERE "); id = 0;
					query_text += kv.first;
					if (kv.second == "null")
						query_text += " IS null";
					else
						((query_text += " = '") += kv.second) += "'";
				}

				std::cout << query_text << "\n";
				_json_result(res, _db->query(query_text));
			}
			return;
		default:
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
