#include "json.h"

#include "server.h"
#include "database.h"

#include "range.h"
#include "string.h"

#include <iostream>
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
				c.value().visit([&](auto v) { data.back().emplace_back(std::string(c.name()), intsToString(std::move(v))); });
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

		auto pop = [](auto&& it) { std::string_view r = *it; ++it; return r; };

		if (seg != request.location.end())
		{
			if (isdigit(seg->front()))
			{
				for (auto d : pop(seg) | map(read_digit))
					if (d < 10)
						id = id * 10 + d;
					else
						return;
			}
		}

		const auto columns = seg == request.location.end() ? 
			std::vector<std::string_view>{} : 
			flatten(pop(seg) | split(","));

		static const auto allalpha = [](const std::string_view& s) { return all(s | map(isalpha)); };
		if (seg != request.location.end() || !all(columns | map(allalpha)))
			return;

		switch (request.method)
		{
		case Method::Get: 
		{
			auto query = request.query;
			if (id != 0) query.emplace_back("id", std::to_string(id));
			_json_result(res,
				(columns.empty() ? _db->selectAll() : _db->select(columns))
				.from(_table).where(query | mapPair(equal)));
			return;
		}
		case Method::Put:
			if (id == 0 || columns.empty() || !request.query.empty())
				res.status = Status::MethodNotAllowed;
			else
			{
				auto body = json::parse(request.body);
				std::cout << "want to put '" << request.body << "' into columns " << columns << "\n";

				if (columns.size() != 1)
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
						.set({ db::equal(std::string(columns[0]), _store_json(body)) })
						.where({ equal("id", std::to_string(id)) })
						.exec();
					res.status = Status::OK;
				}
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
		const auto id = integer("id").primaryKey();
		const auto name = text("name").notNull();
		const auto desc = text("desc").notNull("");
		Query(db->create("places", { id, name, desc }, {}));
		Query(db->create("groups", { id, name, desc }, {}));
		Query(db->create("charactes", 
		{ 
			id, name, desc, integer("group"), integer("place"), 
			integer("str").notNull(5),
			integer("dex").notNull(5),
			integer("nte").notNull(5),
			integer("emp").notNull(5),
			integer("ntu").notNull(5)
		},
		{
			foreignKey("group").references("groups", "id"),
			foreignKey("place").references("places", "id")
		}));

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
