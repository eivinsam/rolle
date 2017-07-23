#include "database.h"

#include <iostream>
#include <string>

namespace db
{
	Database::Handle Database::_open(const char * filename)
	{
		sqlite3* handle;
		if (sqlite3_open_v2(filename, &handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr))
			throw std::runtime_error("Could not open database");
		return Handle(handle);
	}

	Query Database::query(const std::string & query)
	{
		//std::cout << query << "\n";

		sqlite3_stmt* stmt;
		switch (sqlite3_prepare(_handle.get(), query.c_str(), query.size(), &stmt, nullptr))
		{
		case SQLITE_OK: return { STMT(stmt) };
		case SQLITE_ERROR: throw std::runtime_error("Error preparing query: " + _error());
		default:
			throw std::runtime_error("Unecpected error code");
		}
	}
	Query Database::ReadyStep::_prepare()
	{
		std::cout << "  == Builder prepare ==\n" << _build->so_far << "\n";
		if (!_build->binds.empty())
		{
			std::cout << "  Bound to ";
			for (auto&& b : _build->binds)
				b.visit([](auto&& v) { std::cout << v << " "; });
			std::cout << "\n";
		}

		Query q = _build->db.query(_build->so_far);
		for (auto&& iv : _build->binds | ranged::enumerate)
			iv.second.visit([&](auto&& v) { q.bind(iv.first + 1, v); });
		return q;
	}
}
