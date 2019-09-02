#ifndef HEADER_CACHE_HPP
#define HEADER_CACHE_HPP

#include <string>
#include <optional>

#include <sqlite3.h>

enum class Type {
	Static,
	Page,
	Entry,
	Source,
	File,
};

struct Entry {
	Type type;
	std::string source;

	sqlite3_int64 path;
	std::optional<std::string> slug;
	std::string file;

	std::optional<std::string> title;

	std::string datetime;
	bool update;
};

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

	private:
		std::string path_;
		sqlite3* db_ = nullptr;
		bool created_ = false;

		void err_exit(std::string msg, int rc);

		bool create();

		void prepare_or_exit(const char *sql, int max_len,
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
};

#endif /* HEADER_CACHE_HPP */

