#pragma once

#include <iostream>
#include <tuple>
#include <stdexcept>
#include <memory>
#include <vector>
#include <string>
#include <variant>
#include <sqlite3.h>

#include "pointers.h"

#include "range.h"

namespace db
{

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

	enum class Comparator : char { Equal };
	inline std::string_view orth(Comparator cmp)
	{
		switch (cmp)
		{
		case Comparator::Equal: return " = ";
		default: throw std::logic_error("Invalid comparator value");
		}
	}

	using Value = std::variant<nullptr_t, int, sqlite_int64, double, std::string>;

	inline std::ostream& operator<<(std::ostream& out, nullptr_t) { return out << "null"; }

	struct Criterium
	{
		std::string key;
		Value value;
		Comparator cmp;
	};

	inline Criterium equal(std::string key, Value value) { return { std::move(key), std::move(value), Comparator::Equal }; }


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

				std::string_view name() const { return { _do(sqlite3_column_name) }; }

				Value value() const
				{
					switch (_do(sqlite3_column_type))
					{
					case SQLITE_NULL: return nullptr;
					case SQLITE_INTEGER: return _do(sqlite3_column_int64);
					case SQLITE_FLOAT: return _do(sqlite3_column_double);
					case SQLITE_TEXT: return std::string(
						reinterpret_cast<const char*>(_do(sqlite3_column_text)),
						size_t(_do(sqlite3_column_bytes)));
					default:
						throw std::logic_error("Unknown column type encountered");
					}
				}
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
	public:
		class Iterator : protected Row
		{
			friend class Query;
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

			Iterator& operator++() { _rc = _do(sqlite3_step); _col.reset(); return *this; }

			reference operator*() { return *this; }
			pointer operator->() { return this; }

			bool operator!=(End) const { return _rc != SQLITE_DONE; }
		};
	private:
		mutable Iterator _row;
	public:
		Query() = default;
		Query(STMT q) : _row(std::move(q)) { }

		std::string sql() const { return _row.sql(); }

		void bind(int pos, nullptr_t)              { _row.reset(); _row._do(sqlite3_bind_null, pos); }
		void bind(int pos, double value)           { _row.reset(); _row._do(sqlite3_bind_double, pos, value); }
		void bind(int pos, int    value)           { _row.reset(); _row._do(sqlite3_bind_int, pos, value); }
		void bind(int pos, sqlite_int64 value)     { _row.reset(); _row._do(sqlite3_bind_int64, pos, value); }
		void bind(int pos, std::string_view value) { _row.reset(); _row._do(sqlite3_bind_text, pos, value.data(), value.size(), SQLITE_STATIC); }

		Query operator()(sqlite_int64 value) { bind(1, value); return *this; }

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

		using string = std::string;
		using string_view = std::string_view;
		struct Builder
		{
			Database& db;
			string so_far;
			std::vector<Value> binds;

			Builder(Database& db) : db(db) { }
			~Builder()
			{
				std::cout << "Builder end of life: '" << so_far << "'\n";
				std::cout << "  Bound to ";
				for (auto&& b : binds)
					std::visit([](auto&& v) { std::cout << v << " "; }, b);
				std::cout << "\n";
			}

			Builder& operator<<(string_view piece) { so_far += piece; return *this; }
		};
		class BuildStep
		{
		protected:
			std::unique_ptr<Builder> _build;
		public:
			BuildStep(Database& db) : _build(std::make_unique<Builder>(db)) { }
			BuildStep(BuildStep&& other) = default;
		};
		class ReadyStep : public BuildStep
		{
			Query _prepare();
		public:
			using BuildStep::BuildStep;
			ReadyStep(BuildStep&& step) : BuildStep(std::move(step)) { }

			Query::Iterator begin() { return _prepare().begin(); }
			End end() { return {}; }

			void exec() { _prepare().exec(); }
		};

		class Where : public ReadyStep
		{
		public:
			template <class C>
			Where(BuildStep&& step, const C& criteria) : ReadyStep(std::move(step))
			{
				static const std::string_view AND = " AND ";
				std::string_view keyword = " WHERE ";
				for (const Criterium& c : criteria)
				{
					*_build << keyword << c.key << orth(c.cmp) << "?";
					_build->binds.push_back(c.value);
					keyword = AND;
				}
			}
		};

		class From : public ReadyStep
		{
		public:
			From(BuildStep&& build, string_view table) : ReadyStep(std::move(build)) { *_build << " FROM " << table; }

			template <class C>
			Where where(const C& criteria) && { return { std::move(*this), criteria }; }
			Where where(const std::initializer_list<Criterium>& criteria) && { return { std::move(*this), criteria }; }
		};
		class Select : public BuildStep
		{
		public:
			template <class C>
			Select(Database& db, const C& columns) : BuildStep(db)
			{
				using namespace ranged;
				*_build << "SELECT ";
				for (auto&& cd : columns | delimit(", "))
					*_build << cd.second << cd.first;
			}

			From from(string_view table) && { return { std::move(*this), table }; }
		};

		class Set : public BuildStep
		{
		public:
			template <class C>
			Set(BuildStep&& step, const C& criteria) : BuildStep(std::move(step))
			{
				static const string_view COMMA = ", ";
				string_view keyword = " SET ";
				for (const Criterium& c : criteria)
				{
					if (c.cmp != Comparator::Equal)
						throw std::logic_error("Invalid assignment operator");
					*_build << keyword << c.key << " = ?";
					_build->binds.push_back(c.value);
					keyword = COMMA;
				}
			}

			template <class C>
			Where where(const C& criteria) && { return { std::move(*this), criteria }; }
			Where where(const std::initializer_list<Criterium>& criteria) && { return { std::move(*this), criteria }; }
		};
		class Update : public BuildStep
		{
		public:
			Update(Database& db, string_view table) : BuildStep(db) { *_build << "UPDATE " << table; }

			template <class C>
			Set set(const C& criteria) && { return { std::move(*this), criteria }; }
			Set set(const std::initializer_list<Criterium>& criteria) && { return { std::move(*this), criteria }; }
		};
	public:

		Database(const std::string& filename) : _handle(_open(filename.c_str())) { }

		template <class C>
		Select select(const C& columns) { return { *this, columns }; }
		template <class T>
		Select select(const std::initializer_list<T>& columns) { return { *this, columns }; }
		Select selectAll() { return select({ "*" }); }

		Update update(string_view table) { return { *this, table }; }

		Query query(const string& query);

		sqlite_int64 lastInsert() { return sqlite3_last_insert_rowid(_handle.get()); }
	};
}
