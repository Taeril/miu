#include "cache.hpp"

#include <cstdlib>

#include <fmt/core.h>


Cache::Cache(std::string path) {
	open(path);
}

Cache::~Cache() {
	close();
}


bool Cache::open(std::string path) {
	path_ = path;

	int rc = sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE, nullptr);
	if(rc == SQLITE_OK) {
		created_ = false;
		sqlite3_extended_result_codes(db_, 1);
		return true;
	}

	if(rc != SQLITE_CANTOPEN) {
		err_exit("open", rc);
		return false;
	}

	rc = sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
	if(rc == SQLITE_OK) {
		sqlite3_extended_result_codes(db_, 1);
		return create();
	}

	err_exit("open(create)", rc);
	//close();
	return false;
}

void Cache::close() {
	if(db_) {
		sqlite3_close(db_);
	}

	path_ = "";
	db_ = nullptr;
	created_ = false;
}

void Cache::err_exit(std::string msg, int rc) {
	close();

	//fmt::print("SQLITE ERROR: {}: {}\n", msg, sqlite3_errmsg(db_));
	fmt::print("SQLITE ERROR({}): {}: {}\n", rc, msg, sqlite3_errstr(rc));
	std::exit(1);
}

bool Cache::create() {

#include "sql.h"

	const char* psql = sql;
	int psql_len = sql_len;
	const char* tail = nullptr;
	const char* end = sql + sql_len;

	while(psql < end) {
		sqlite3_stmt* stmt = nullptr;

		int rc = sqlite3_prepare_v2(db_, psql, psql_len, &stmt, &tail);
		if(rc != SQLITE_OK) {
			created_ = false;
			err_exit("create(prepare)", rc);
			return false;
		}

		int n = static_cast<int>(tail - psql);

		if(stmt) {
			rc = sqlite3_step(stmt);
			if(rc != SQLITE_DONE) {
				err_exit("create(step)", rc);
			}
		}

		sqlite3_finalize(stmt);
		// rc = ???

		psql_len -= n;
		psql = tail;
	}

	created_ = true;
	return true;
}


template<size_t N>
constexpr int length(char const (&)[N]) {
	return static_cast<int>(N - 1);
}

void Cache::prepare_or_exit(const char *sql, int max_len,
	sqlite3_stmt** stmt, const char** tail, const char* errmsg) {
	int rc = sqlite3_prepare_v2(db_, sql, max_len, stmt, tail);
	if(rc != SQLITE_OK) {
		err_exit(errmsg, rc);
	}
}

void Cache::bind_or_exit(sqlite3_stmt* stmt, int idx,
	const char* value, int size, const char* errmsg) {
	int rc = sqlite3_bind_text(stmt, idx, value, size, SQLITE_STATIC);
	if(rc != SQLITE_OK) {
		sqlite3_finalize(stmt);
		err_exit(errmsg, rc);
	}
}

void Cache::bind_or_exit(sqlite3_stmt* stmt, int idx,
	std::string const& value, const char* errmsg) {
	int rc = sqlite3_bind_text(stmt, idx,
		value.c_str(), static_cast<int>(value.size()), SQLITE_STATIC);
	if(rc != SQLITE_OK) {
		sqlite3_finalize(stmt);
		err_exit(errmsg, rc);
	}
}

void Cache::bind_or_exit(sqlite3_stmt* stmt, int idx,
	int value, const char* errmsg) {
	int rc = sqlite3_bind_int(stmt, idx, value);
	if(rc != SQLITE_OK) {
		sqlite3_finalize(stmt);
		err_exit(errmsg, rc);
	}
}

void Cache::bind_or_exit(sqlite3_stmt* stmt, int idx,
	sqlite3_int64 value, const char* errmsg) {
	int rc = sqlite3_bind_int64(stmt, idx, value);
	if(rc != SQLITE_OK) {
		sqlite3_finalize(stmt);
		err_exit(errmsg, rc);
	}
}


sqlite3_int64 Cache::get_id(
	std::string const& name,
	const char* sql_insert, int sql_insert_len,
	const char* sql_select, int sql_select_len
) {
	// try to insert

	sqlite3_stmt* stmt = nullptr;
	prepare_or_exit(sql_insert, sql_insert_len, &stmt, nullptr,
		"get_id(prepare insert)");

	bind_or_exit(stmt, 1, name, "get_id(bind insert)");

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);


	// select id

	prepare_or_exit(sql_select, sql_select_len, &stmt, nullptr,
		"get_id(prepare select)");

	bind_or_exit(stmt, 1, name, "get_id(bind select)");

	rc = sqlite3_step(stmt);
	if(rc == SQLITE_ROW) {
		auto ret = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
		return ret;
	}

	sqlite3_finalize(stmt);
	err_exit("get_id(step)", rc);

	return 0;
}

sqlite3_int64 Cache::path_id(std::string const& path) {
	const char sql_insert[] = "INSERT OR IGNORE INTO paths(name) VALUES(?)";
	constexpr const int sql_insert_len = length(sql_insert);
	const char sql_select[] = "SELECT id FROM paths WHERE name = ?";
	constexpr const int sql_select_len = length(sql_select);

	return get_id(path, sql_insert, sql_insert_len, sql_select, sql_select_len);
}

sqlite3_int64 Cache::tag_id(std::string const& tag) {
	const char sql_insert[] = "INSERT OR IGNORE INTO tags(name) VALUES(?)";
	constexpr const int sql_insert_len = length(sql_insert);
	const char sql_select[] = "SELECT id FROM tags WHERE name = ?";
	constexpr const int sql_select_len = length(sql_select);

	return get_id(tag, sql_insert, sql_insert_len, sql_select, sql_select_len);
}


sqlite3_int64 Cache::add_entry(Entry const& entry) {
	const char sql_upsert[] = R"~(
		INSERT
			--           1     2       3     4     5     6      7
			INTO entries(type, source, path, slug, file, title, created)
			VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7)
		ON CONFLICT(path, slug, file) DO UPDATE
			SET type = ?1, source = ?2, title = ?6, updated = ?8
			WHERE path = ?3 AND slug = ?4 AND file = ?5
	)~";
	constexpr const int sql_upsert_len = length(sql_upsert);

	sqlite3_stmt* stmt = nullptr;
	prepare_or_exit(sql_upsert, sql_upsert_len, &stmt, nullptr,
		"add_entry(prepare)");

	bind_or_exit(stmt, 1, static_cast<int>(entry.type), "add_entry(bind type)");
	bind_or_exit(stmt, 2, entry.source, "add_entry(bind source)");

	bind_or_exit(stmt, 3, entry.path, "add_entry(bind path)");
	if(entry.slug) {
		bind_or_exit(stmt, 4, *entry.slug, "add_entry(bind slug)");
	} else {
		bind_or_exit(stmt, 4, "", "add_entry(bind slug='')");
	}
	bind_or_exit(stmt, 5, entry.file, "add_entry(bind file)");
	if(entry.title) {
		bind_or_exit(stmt, 6, *entry.title, "add_entry(bind title)");
	} else {
		bind_or_exit(stmt, 6, nullptr, 0, "add_entry(bind title=NULL)");
	}
	bind_or_exit(stmt, 7, entry.datetime, "add_entry(bind created)");
	if(entry.update) {
		bind_or_exit(stmt, 8, entry.datetime, "add_entry(bind updated)");
	} else {
		bind_or_exit(stmt, 8, nullptr, 0, "add_entry(bind updated=NULL)");
	}

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if(rc != SQLITE_DONE) {
		err_exit("add_entry(step)", rc);
	}


	// select id
	const char sql_select[] = R"~(
		SELECT id FROM entries
			WHERE path = ?1 AND slug = ?2 AND file = ?3
	)~";
	constexpr const int sql_select_len = length(sql_select);

	prepare_or_exit(sql_select, sql_select_len, &stmt, nullptr,
		"add_entry(prepare select)");

	bind_or_exit(stmt, 1, entry.path, "add_entry(bind path)");
	if(entry.slug) {
		bind_or_exit(stmt, 2, *entry.slug, "add_entry(bind slug)");
	} else {
		bind_or_exit(stmt, 2, "", "add_entry(bind slug='')");
	}
	bind_or_exit(stmt, 3, entry.file, "add_entry(bind file)");

	rc = sqlite3_step(stmt);
	if(rc == SQLITE_ROW) {
		auto ret = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
		return ret;
	}

	sqlite3_finalize(stmt);
	err_exit("add_entry(step select)", rc);

	return 0;
}

void Cache::add_tag(sqlite3_int64 entry, std::string const& tag) {
	const char sql_upsert[] = R"~(
		INSERT INTO tagged_entries(tag, entry) VALUES(?, ?)
	)~";
	constexpr const int sql_upsert_len = length(sql_upsert);

	sqlite3_stmt* stmt = nullptr;
	prepare_or_exit(sql_upsert, sql_upsert_len, &stmt, nullptr,
		"add_tag(prepare)");

	bind_or_exit(stmt, 1, tag_id(tag), "add_tag(bind tag)");
	bind_or_exit(stmt, 2, entry, "add_tag(bind entry)");

	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if(rc != SQLITE_DONE) {
		err_exit("add_tag(step)", rc);
	}
}

