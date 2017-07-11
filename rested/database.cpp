#include "database.h"

#include <iostream>
#include <string>

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
