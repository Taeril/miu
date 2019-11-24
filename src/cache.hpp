#ifndef HEADER_CACHE_HPP
#define HEADER_CACHE_HPP

#include <string>
#include <vector>
#include <optional>
#include <functional>

#include <sqlite3.h>

enum class Type {
	Static,
	Page,
	Entry,
	Source,
	File,
	List,
	Index,
	Feed,
};

struct Entry {
	Type type;
	std::string source;

	sqlite3_int64 path;
	std::optional<std::string> slug;
	std::string file;

	std::optional<std::string> title;

	std::string created;
	std::string updated;
	bool update;
};

using QueryResult = std::vector<std::string> const&;
using QueryCallback = std::function<void(QueryResult)>;

class Cache {
	public:
		Cache(std::string path);
		~Cache();

		bool open(std::string path);
		bool created() { return created_; }
		void close();

		sqlite3_int64 path_id(std::string const& path);
		sqlite3_int64 tag_id(std::string const& tag);

		sqlite3_int64 add_entry(Entry const& entry);
		void add_tag(sqlite3_int64 entry, std::string const& tag);

		void last_entries(int count, QueryCallback cb);
		void list_subpaths(sqlite3_int64 path, QueryCallback cb);
		void list_entries_path(sqlite3_int64 path, QueryCallback cb);
		void list_entries_tag(sqlite3_int64 tag, QueryCallback cb);
		void list_tags(QueryCallback cb);
	private:
		std::string path_;
		sqlite3* db_ = nullptr;
		bool created_ = false;

		void err_exit(std::string msg, int rc);

		bool create();

		void prepare_or_exit(const char* sql, int max_len,
			sqlite3_stmt** stmt, const char** tail, const char* errmsg);

		void bind_or_exit(sqlite3_stmt* stmt, int idx,
			const char* value, int size, const char* errmsg);
		void bind_or_exit(sqlite3_stmt* stmt, int idx,
			std::string const& value, const char* errmsg);
		void bind_or_exit(sqlite3_stmt* stmt, int idx,
			int value, const char* errmsg);
		void bind_or_exit(sqlite3_stmt* stmt, int idx,
			sqlite3_int64 value, const char* errmsg);

		sqlite3_int64 get_id(
			std::string const& name,
			const char* sql_insert, int sql_insert_len,
			const char* sql_select, int sql_select_len
		);

		void list_things(sqlite3_stmt* stmt, int count, QueryCallback cb);
};

#endif /* HEADER_CACHE_HPP */

