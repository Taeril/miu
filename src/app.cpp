#include "app.hpp"

#include <string>
#include <tuple>
#include <fstream>
#include <algorithm>

#include <fmt/core.h>
#if __has_include(<fmt/time.h>) && FMT_VERSION < 60000
#include <fmt/time.h>
#endif
#if __has_include(<fmt/chrono.h>)
#include <fmt/chrono.h>
#endif

#include <kvc/utils.hpp>
#include <mkd/utils.hpp>
#include <kvc/kvc.hpp>
#include <mkd/mkd.hpp>

#include "entry_tmpl.h"
#include "feed_tmpl.h"
#include "footer_tmpl.h"
#include "header_tmpl.h"
#include "index_tmpl.h"
#include "list_tmpl.h"
#include "page_tmpl.h"

#define LOG_INFO(...) do { if(config_.verbose > 0) fmt::print(__VA_ARGS__); } while(0)
#define LOG_TRACE(...) do { if(config_.verbose > 1) fmt::print(__VA_ARGS__); } while(0)
#define LOG_ERROR(...) do { fmt::print(stderr, __VA_ARGS__); } while(0)

// formatter for fs::path
// because using fmt/ostream.h would surround path with quotes
namespace fmt {
template <>
struct formatter<fs::path> {
	template <typename ParseContext>
	constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

	template <typename FormatContext>
	auto format(fs::path const& p, FormatContext &ctx) {
		return format_to(ctx.out(), "{}", p.string());
	}
};
}

namespace {
	std::string const& cond_rm(std::string const& file, bool rebuild) {
		if(rebuild) {
			fs::remove(file);
		}
		return file;
	}

	void write_file(fs::path const& path, std::string const& data) {
		std::ofstream out(path);
		out.write(data.c_str(), data.size());
	}

	miu::mtime_t get_mtime(fs::path const& path) {
		return fs::last_write_time(path);
	}

	std::string format_mtime(miu::mtime_t mtime) {
		std::time_t cftime = miu::mtime_t::clock::to_time_t(mtime);
		return fmt::format("{:%Y-%m-%dT%H:%M:%SZ}", *std::gmtime(&cftime));
	}
}

namespace miu {

App::App(int argc, char** argv) : config_(argc, argv),
	cache_(cond_rm(config_.cache_db, config_.rebuild)) {

	std::string header = init_tmpl("header.tmpl", header_tmpl);
	std::string footer = init_tmpl("footer.tmpl", footer_tmpl);

	index_tmpl_.parse(header + init_tmpl("index.tmpl", index_tmpl) + footer);
	list_tmpl_.parse(header + init_tmpl("list.tmpl", list_tmpl) + footer);
	page_tmpl_.parse(header + init_tmpl("page.tmpl", page_tmpl) + footer);
	entry_tmpl_.parse(header + init_tmpl("entry.tmpl", entry_tmpl) + footer);
	feed_tmpl_.parse(init_tmpl("feed.tmpl", feed_tmpl));
}

App::~App() {
}

std::string App::init_tmpl(std::string const& path, const char* default_) {
	auto p = fs::path(config_.template_dir) / path;

	if(fs::exists(p) || fs::is_regular_file(p)) {
		return read_file(p);
	}

	LOG_TRACE("TEMPLATE: create {}\n", path);
	fs::create_directories(config_.template_dir);
	write_file(p, default_);
	return default_;
}

int App::run() {
	process_static();

	if(config_.rebuild || config_.files.empty()) {
		process_source();
	}
	if(!config_.files.empty()) {
		for(auto const& file : config_.files) {
			auto path = fs::path(file);
			if(!fs::exists(path) || !fs::is_regular_file(path)) {
				continue;
			}
			if(path.is_relative()) {
				path = fs::absolute(path);
			}
			//path = fs::canonical(path);
			path = path.lexically_normal();

			LOG_TRACE("FILE: {}\n", path);

			auto src_path = fs::path(config_.source_dir);
			auto rel_path = path.lexically_relative(src_path);

			if(path != src_path / rel_path) {
				LOG_ERROR("ERROR: file '{}' is outside source directory '{}'\n",
					path, src_path);
				std::exit(1);
			}

			process_mkd(path);
		}
	}

	process_paths();
	process_tags();
	process_index();

	return 0;
}


mtime_t App::update_file(std::string const& info,
	fs::path const& src, fs::path const& dst) {

	if(!fs::exists(src) || !fs::is_regular_file(src)) {
		LOG_INFO("FILE NOT FOUND: {}\n", src);
		return mtime_t::min();
	}

	auto src_mtime = get_mtime(src);

	if(!config_.rebuild && fs::exists(dst)) {
		auto dst_mtime = get_mtime(dst);

		if(src_mtime > dst_mtime) {
			LOG_INFO("UPDATE: {}\n", info);
		} else {
			return mtime_t::min();
		}
	} else {
		LOG_INFO("COPY: {}\n", info);
	}

	fs::create_directories(dst.parent_path());
	fs::copy_file(src, dst, fs::copy_options::update_existing);

	return src_mtime;
}

mtime_t App::create_file(std::string const& info, std::string const& data,
	fs::path const& src, fs::path const& dst) {

	auto src_mtime = get_mtime(src);

	if(!config_.rebuild && fs::exists(dst)) {
		auto dst_mtime = get_mtime(dst);

		if(src_mtime > dst_mtime) {
			LOG_INFO("UPDATE: {}\n", info);
		} else {
			return mtime_t::min();
		}
	} else {
		LOG_INFO("CREATE: {}\n", info);
	}

	fs::create_directories(dst.parent_path());
	write_file(dst, data);

	return src_mtime;
}

void App::process_static() {
	auto destination = fs::path(config_.destination_dir);

	for(auto const& p : fs::recursive_directory_iterator(config_.static_dir)) {
		if(!p.is_regular_file()) {
			continue;
		}

		auto path = p.path().lexically_relative(config_.static_dir);
		auto file = destination / path;

		auto mtime = update_file(path, p.path(), file);
		if(mtime != mtime_t::min()) {
			auto sql_path = cache_.path_id(path.parent_path());
			Entry entry;
			entry.type = Type::Static;
			entry.source = path;
			entry.path = sql_path;
			entry.slug = {};
			entry.file = path.filename();
			entry.title = {};
			entry.datetime = format_mtime(mtime);
			entry.update = false;

			cache_.add_entry(entry);
		}
	}
}

void App::process_source() {
	for(auto const& p : fs::recursive_directory_iterator(config_.source_dir)) {
		if(!p.is_regular_file()) {
			continue;
		}

		auto path = p.path();
		if(path.extension() != ".md") {
			continue;
		}

		process_mkd(path);
	}
}

void App::process_mkd(fs::path const& src_path) {
	auto destination = fs::path(config_.destination_dir);

	auto base_url = config_.cfg.get_value("base_url", "/");

	std::string md = read_file(src_path.string());

	std::string const separator("---\n");
	kvc::Config meta;
	if(md.rfind(separator, 0) == 0) {
		auto pos = md.find(separator, separator.size());
		auto const len = separator.size();
		if(pos != std::string::npos) {
			meta.parse(md.substr(len, pos-len));
			md = md.substr(pos+len);
		}
	}

	auto path = src_path.lexically_relative(config_.source_dir);

	auto pages_dirs = config_.cfg.get("pages_dirs");
	bool auto_page = false;
	if(pages_dirs && pages_dirs->is_array) {
		// use of fs::path("/") makes same in config:
		// "" and "/"
		// "foo" and "/foo"
		auto const p = fs::path("/") / path.parent_path();
		for(auto const& d : pages_dirs->values) {
			auto const v = fs::path("/") / d;
			if(v == p) {
				auto_page = true;
				break;
			}
		}
	}

	mkd::Parser parser;
	std::string html = parser.parse(md);

	auto type = meta.get_value("type", "entry");
	bool is_page = auto_page || type == "page";
	tmpl::Template& tmpl = is_page ? page_tmpl_ : entry_tmpl_;

	auto root = tmpl.data();
	root->clear();

	std::string title = meta.get_value("title", parser.title());
	std::string slug = meta.get_value("slug", parser.slug());
	if(title.empty()) {
		title = path.stem();
	}
	if(slug.empty()) {
		slug = slugify(title);
	}
	meta.set("title", title);

	auto base = path.parent_path() / slug;
	auto info = base / "index.html";
	auto dst = destination / info;

	auto src_mtime = get_mtime(src_path);
	auto src_datetime = format_mtime(src_mtime);
	if(auto created = meta.get("created"); !created) {
		meta.set("created", src_datetime);
	} else if(created->value != src_datetime) {
		meta.set("updated", src_datetime);
	}

	if(is_page) {
		meta.set("type", "page");
	}

	auto meta_files = meta.get("files");

	if(parser.files().size()) {
		if(!meta_files || (meta_files && !meta_files->is_array)) {
			// TODO: make less hacky way
			meta.add("files", "");
			meta_files = meta.get("files");
			meta_files->is_array = true;
		}
		for(auto const& file : parser.files()) {
			// skip directories
			// TODO: rethink this hack
			if(file[file.size()-1] != '/') {
				meta_files->values.push_back(file);
			}
		}

		auto& vs = meta_files->values;
		//for(auto& v : vs) {
		//	// TODO: normalize values, make path relative to .md file
		//}
		std::sort(vs.begin(), vs.end());
		auto last = std::unique(vs.begin(), vs.end());
		vs.erase(last, vs.end());
	}

	auto tags = meta.get("tags");
	if(tags && tags->is_array) {
		auto block = root->block("tags");
		for(auto const& tag : tags->values) {
			auto& t = block->add();
			t.set("url", base_url + "tags/" + tag + "/");
			t.set("name", tag);
		}
	}

	config2tmpl(config_.cfg, root);
	config2tmpl(meta, root);
	root->set("datetime", src_datetime);
	root->set("date", src_datetime.substr(0, 10));
	root->set("url", base_url + base.string() + "/");
	root->set("content", html);


	// update .md file
	write_file(src_path, separator + meta.to_string() + separator + md);
	fs::last_write_time(src_path, src_mtime);


	// create index.html from .md
	auto md_mtime = create_file(info, tmpl.make(), src_path, dst);
	if(md_mtime != mtime_t::min()) {
		auto sql_path = cache_.path_id(base.parent_path());
		Entry entry;
		entry.type = is_page ? Type::Page : Type::Entry;
		entry.source = path;
		entry.path = sql_path;
		entry.slug = slug;
		entry.file = "index.html";
		entry.title = title;
		entry.datetime = format_mtime(md_mtime);
		entry.update = false;

		auto entry_id = cache_.add_entry(entry);

		if(!is_page) {
			auto path = base.parent_path();
			while(!path.empty()) {
				paths_.insert(path);
				path = path.parent_path();
			}
			if(tags && tags->is_array) {
				for(auto const& tag : tags->values) {
					cache_.add_tag(entry_id, tag);
					tags_.insert(tag);
				}
			}
		}
	}

	for(auto const& code : parser.codes()) {
		auto [file, data] = code;
		auto finfo = base / file;
		auto fpath = destination / finfo;

		auto code_mtime = create_file(finfo, data, src_path, fpath);
		if(code_mtime != mtime_t::min()) {
			auto sql_path = cache_.path_id(base.parent_path());
			Entry entry;
			entry.type = Type::Source;
			entry.source = path;
			entry.path = sql_path;
			entry.slug = slug;
			entry.file = file;
			entry.title = {};
			entry.datetime = format_mtime(code_mtime);
			entry.update = false;

			cache_.add_entry(entry);
		}
	}


	if(meta_files && meta_files->values.size()) {
		auto src_dir = src_path.parent_path();
		for(auto const& file : meta_files->values) {
			auto src_file = src_dir / file;
			auto finfo = base / file;
			auto dst_file = destination / finfo;

			auto file_mtime = update_file(finfo, src_file, dst_file);
			if(file_mtime != mtime_t::min()) {
				auto sql_path = cache_.path_id(base.parent_path());
				Entry entry;
				entry.type = Type::File;
				entry.source = src_file.lexically_relative(config_.source_dir);
				entry.path = sql_path;
				entry.slug = slug;
				entry.file = file;
				entry.title = {};
				entry.datetime = format_mtime(file_mtime);
				entry.update = false;

				cache_.add_entry(entry);
			}
		}
	}
}

void App::config2tmpl(kvc::Config& conf, tmpl::Data::Value* root) {
	conf.each([root](kvc::KVC const& cfg) {
		if(!cfg.is_array) {
			root->set(cfg.key, cfg.value);
		}
	});
}

void App::process_paths() {
	enum { PATH, NAME };
	enum { PATH_, SLUG, FILE_, TITLE, DATETIME };

	if(paths_.empty()) {
		return;
	}

	auto root = list_tmpl_.data();

	auto destination = fs::path(config_.destination_dir);
	auto base_url = config_.cfg.get_value("base_url", "/");

	for(auto const& path : paths_) {
		// skip / as there is index.html from process_index
		if(path.empty()) {
			continue;
		}

		root->clear();
		config2tmpl(config_.cfg, root);
		root->set("title", path);

		auto path_id = cache_.path_id(path);

		auto block_list = root->block("list");
		cache_.list_subpaths(path_id, [&](QueryResult paths) {
			auto& p = block_list->add();
			p.set("url", base_url + paths[PATH] + "/");
			p.set("name", paths[NAME]);
		});

		auto block_entries = root->block("entries");
		cache_.list_entries_path(path_id, [&](QueryResult entry) {
			auto& e = block_entries->add();
			e.set("datetime", entry[DATETIME]);
			e.set("date", entry[DATETIME].substr(0, 10));
			e.set("title", entry[TITLE]);
			std::string path_slash = entry[PATH].empty() ? "" : entry[PATH] + "/";
			e.set("url", base_url + path_slash + entry[SLUG] + "/");
		});

		LOG_INFO("CREATE: {}/index.html\n", path);
		auto dst = destination / path;
		fs::create_directories(dst);
		write_file(dst / "index.html", list_tmpl_.make());
		
		auto sql_path = cache_.path_id(path);
		Entry entry;
		entry.type = Type::List;
		entry.source = "";
		entry.path = sql_path;
		entry.slug = {};
		entry.file = "index.html";
		entry.title = {};
		entry.datetime = config_.cfg.get_value("now", "now");
		entry.update = false;

		cache_.add_entry(entry);
	}
}

void App::process_tags() {
	enum { NAME };
	enum { PATH, SLUG, FILE_, TITLE, DATETIME };

	if(tags_.empty()) {
		return;
	}

	auto root = list_tmpl_.data();

	auto destination = fs::path(config_.destination_dir);
	auto base_url = config_.cfg.get_value("base_url", "/");

	root->clear();
	config2tmpl(config_.cfg, root);
	root->set("title", config_.cfg.get("tags_name")->value);

	auto block_list = root->block("list");
	cache_.list_tags([&](QueryResult tag) {
		auto& p = block_list->add();
		p.set("url", base_url + "tags/" + tag[NAME] + "/");
		p.set("name", tag[NAME]);
	});

	LOG_INFO("CREATE: tags/index.html\n");
	auto dst = destination / "tags";
	fs::create_directories(dst);
	write_file(dst / "index.html", list_tmpl_.make());

	auto sql_path = cache_.path_id("tags");
	Entry entry;
	entry.type = Type::List;
	entry.source = "";
	entry.path = sql_path;
	entry.slug = {};
	entry.file = "index.html";
	entry.title = {};
	entry.datetime = config_.cfg.get_value("now", "now");
	entry.update = false;

	cache_.add_entry(entry);

	for(auto const& tag : tags_) {
		root->clear();
		config2tmpl(config_.cfg, root);
		root->set("title", config_.cfg.get("tags_name")->value + ": " + tag);

		auto tag_id = cache_.tag_id(tag);

		auto block_entries = root->block("entries");
		cache_.list_entries_tag(tag_id, [&](QueryResult entry) {
			auto& e = block_entries->add();
			e.set("datetime", entry[DATETIME]);
			e.set("date", entry[DATETIME].substr(0, 10));
			e.set("title", entry[TITLE]);
			std::string path_slash = entry[PATH].empty() ? "" : entry[PATH] + "/";
			e.set("url", base_url + path_slash + entry[SLUG] + "/");
		});

		LOG_INFO("CREATE: tags/{}/index.html\n", tag);
		auto dst = destination / "tags" / tag;
		fs::create_directories(dst);
		write_file(dst / "index.html", list_tmpl_.make());
		

		auto sql_path = cache_.path_id(fmt::format("tags/{}", tag));
		Entry entry;
		entry.type = Type::List;
		entry.source = "";
		entry.path = sql_path;
		entry.slug = {};
		entry.file = "index.html";
		entry.title = {};
		entry.datetime = config_.cfg.get_value("now", "now");
		entry.update = false;

		cache_.add_entry(entry);
	}
}

void App::process_index() {
	enum { PATH, SLUG, FILE_, TITLE, DATETIME, SOURCE };

	if(paths_.empty()) {
		return;
	}

	auto root = index_tmpl_.data();
	auto feed = feed_tmpl_.data();

	auto destination = fs::path(config_.destination_dir);
	auto base_url = config_.cfg.get_value("base_url", "/");
	auto feed_base_url = config_.cfg.get_value("feed_base_url", base_url);
	if(feed_base_url.back() != '/') {
		feed_base_url += '/';
	}
	auto title = config_.cfg.get_value("title",
		config_.cfg.get_value("home_name", "/")
	);

	root->clear();
	config2tmpl(config_.cfg, root);
	root->set("title", title);

	feed->clear();
	config2tmpl(config_.cfg, feed);
	feed->set("title", title);
	bool is_first = true;
	feed->set("feed_url", feed_base_url + "feed.xml");
	feed->set("index_url", feed_base_url);
	feed->set("id", feed_base_url);

	int num_entries = std::stoi(config_.cfg.get_value("num_entries", "5"));
	int short_size = std::stoi(config_.cfg.get_value("short_size", "200"));

	auto block_entries = root->block("entries");
	auto feed_entries = feed->block("entries");
	cache_.last_entries(num_entries, [&](QueryResult entry) {
		if(is_first) {
			feed->set("updated", entry[DATETIME]);
			is_first = false;
		}

		auto src_path = fs::path(config_.source_dir) / entry[SOURCE];
		std::string md = read_file(src_path.string());

		std::string const separator("---\n");
		kvc::Config meta;
		if(md.rfind(separator, 0) == 0) {
			auto pos = md.find(separator, separator.size());
			auto const len = separator.size();
			if(pos != std::string::npos) {
				meta.parse(md.substr(len, pos-len));
				md = md.substr(pos+len);
			}
		}

		mkd::Parser parser;
		std::string html = parser.parse(md);

		auto pos = md.find("<!-- cut -->");
		if(pos != std::string::npos) {
			md = md.substr(0, pos);
		} else {
			pos = md.find('\n', short_size);
			while(pos != std::string::npos) {
				if(md[pos+1] == '\r' || md[pos+1] == '\n') {
					md = md.substr(0, pos);
					break;
				}
				pos = md.find('\n', pos+1);
			}
			pos = md.find("\n```");
			if(pos != std::string::npos) {
				md = md.substr(0, pos);
			}
			pos = md.find("\n    ");
			if(pos != std::string::npos) {
				md = md.substr(0, pos);
			}
		}
		std::string short_html = parser.parse(md);

		auto& e = block_entries->add();
		config2tmpl(meta, &e);
		e.set("datetime", entry[DATETIME]);
		e.set("date", entry[DATETIME].substr(0, 10));
		e.set("title", entry[TITLE]);
		std::string path_slash = entry[PATH].empty() ? "" : entry[PATH] + "/";
		e.set("url", base_url + path_slash + entry[SLUG] + "/");
		e.set("content", short_html);

		auto tags = meta.get("tags");
		if(tags && tags->is_array) {
			auto block = e.block("tags");
			for(auto const& tag : tags->values) {
				auto& t = block->add();
				t.set("url", base_url + "tags/" + tag + "/");
				t.set("name", tag);
			}
		}

		auto& fe = feed_entries->add();
		fe.set("title", entry[TITLE]);
		fe.set("url", feed_base_url + path_slash + entry[SLUG] + "/");
		fe.set("datetime", entry[DATETIME]);
		fe.set("content", short_html);
		fe.set("id", feed_base_url + path_slash + entry[SLUG] + "/");
	});

	{
		LOG_INFO("CREATE: index.html\n");
		auto dst = destination;
		fs::create_directories(dst);
		write_file(dst / "index.html", index_tmpl_.make());

		auto sql_path = cache_.path_id("");
		Entry entry;
		entry.type = Type::Index;
		entry.source = "";
		entry.path = sql_path;
		entry.slug = {};
		entry.file = "index.html";
		entry.title = {};
		entry.datetime = config_.cfg.get_value("now", "now");
		entry.update = false;

		cache_.add_entry(entry);
	}

	{
		LOG_INFO("CREATE: feed.xml\n");
		auto dst = destination;
		fs::create_directories(dst);
		write_file(dst / "feed.xml", feed_tmpl_.make());

		auto sql_path = cache_.path_id("");
		Entry entry;
		entry.type = Type::Feed;
		entry.source = "";
		entry.path = sql_path;
		entry.slug = {};
		entry.file = "feed.xml";
		entry.title = {};
		entry.datetime = config_.cfg.get_value("now", "now");
		entry.update = false;

		cache_.add_entry(entry);
	}
}


} // namespace miu

