#pragma once

#include <iostream>
#include <tuple>
#include <stdexcept>
#include <memory>
#include <sqlite3.h>

#include "pointers.h"

class Database;

class STMT
{
	struct Deleter { void operator()(sqlite3_stmt* ptr) { sqlite3_finalize(ptr); } };
	using Handle = shared<sqlite3_stmt>;

	Handle _handle;
public:
	STMT() = default;
	STMT(sqlite3_stmt* stmt) : _handle(stmt, Deleter{}) { }

	sqlite3_stmt* get() const { return _handle.get(); }
};

struct End { };


class Query
{
	class Row
	{
		class Column
		{
			template <class F>
			auto _do(F&& f) const { return f(_q.get(), _index); }
		protected:
			STMT _q;
			int _index = 0;
		public:
			Column() = default;
			Column(STMT q) : _q(std::move(q)) { }

			bool isInteger() const { return _do(sqlite3_column_type) == SQLITE_INTEGER; }

			int integer() const { return _do(sqlite3_column_int); }

			std::string_view text() const
			{
				return
				{
					reinterpret_cast<const char*>(_do(sqlite3_column_text)),
					size_t(_do(sqlite3_column_bytes))
				};
			}

			std::string_view name() const { return { _do(sqlite3_column_name) }; }

			auto type() const { return _do(sqlite3_column_type); }
		};
		class Iterator : protected Column
		{
			friend class Row;
		public:
			using iterator_category = std::input_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using value_type = Column;
			using reference = value_type&;
			using pointer = value_type*;

			Iterator() = default;
			Iterator(STMT q) : Column(std::move(q)) { }

			void reset() { _index = 0; }

			Iterator& operator++() { ++_index;  return *this; }

			reference operator*() { return *this; }
			pointer operator->() { return this; }

			bool operator!=(int end) { return _index < end; }
		};
	protected:
		template <class F, class... Args>
		auto _do(F&& f, Args&&... args) const { return f(_col._q.get(), std::forward<Args>(args)...); }
		mutable Iterator _col;
		int _rc = SQLITE_ROW;
	public:
		Row() = default;
		Row(STMT q) : _col(std::move(q)) { }

		int size() const { return _do(sqlite3_column_count); }

		Iterator& begin() const { return _col; }
		int       end()   const { return size(); }
	};
	struct End { };
	class Iterator : protected Row
	{
	public:
		using iterator_category = std::input_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = Row;
		using reference = value_type&;
		using pointer = value_type*;

		Iterator() = default;
		Iterator(STMT q) : Row(std::move(q)) { }

		std::string sql() const { return _do(sqlite3_sql); }

		void reset() { _do(sqlite3_reset); }
		void bind(int pos, int value) { reset(); _do(sqlite3_bind_int, pos, value); }
		void bind(int pos, std::string_view value) { reset(); _do(sqlite3_bind_text, pos, value.data(), value.size(), SQLITE_STATIC); }

		Iterator& operator++() { _rc = _do(sqlite3_step); _col.reset(); return *this; }

		reference operator*() { return *this; }
		pointer operator->() { return this; }

		bool operator!=(End) const { return _rc != SQLITE_DONE; }
	};

	mutable Iterator _row;
public:
	Query() = default;
	Query(STMT q) : _row(std::move(q)) { }

	std::string sql() const { return _row.sql(); }

	Query bind(int binding, int value) { _row.bind(binding, value); return *this; }
	Query bind(int binding, std::string_view value) { _row.bind(binding, value); return *this; }

	Query operator()(int value) { _row.bind(1, value); return *this; }

	Iterator& begin() const { _row.reset(); ++_row; return _row; }
	End       end()   const { return {}; }

	void exec()
	{
		++_row;
		if (_row != end())
			throw std::runtime_error("Unexpected multi-row result");
	}
};




class Database
{
	struct Deleter { void operator()(sqlite3* handle) { sqlite3_close(handle); } };
	using Handle = std::unique_ptr<sqlite3, Deleter>;
	static Handle _open(const char* filename);
	Handle _handle;
	std::string _error()
	{
		return sqlite3_errmsg(_handle.get());
	}

public:
	Database(const std::string& filename) : _handle(_open(filename.c_str())) { }

	Query query(const std::string& query);

	int lastInsert() { return sqlite3_last_insert_rowid(_handle.get()); }
};
