#include "app.hpp"

#include <string>
#include <tuple>
#include <fstream>
#include <algorithm>

#include <fmt/core.h>
#include <fmt/time.h>

#include <kvc/utils.hpp>
#include <mkd/utils.hpp>
#include <kvc/kvc.hpp>
#include <mkd/mkd.hpp>


// formatter for fs::path
// because using fmt/ostream.h would surround path with quotes
namespace fmt {
template <>
struct formatter<fs::path> {
	template <typename ParseContext>
	constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

	template <typename FormatContext>
	auto format(fs::path const& p, FormatContext &ctx) {
		return format_to(ctx.begin(), "{}", p.string());
	}
};
}


namespace miu {

App::App(int argc, char** argv) : config(argc, argv) {
	std::string header = read_file(config.template_dir + "/header.tmpl");
	std::string footer = read_file(config.template_dir + "/footer.tmpl");

	index_tmpl.parse(header + read_file(config.template_dir + "/index.tmpl") + footer);
	list_tmpl.parse(header + read_file(config.template_dir + "/list.tmpl") + footer);
	page_tmpl.parse(header + read_file(config.template_dir + "/page.tmpl") + footer);
	entry_tmpl.parse(header + read_file(config.template_dir + "/entry.tmpl") + footer);
	feed_tmpl.parse(read_file(config.template_dir + "/feed.tmpl"));
}

App::~App() {
}

int App::run() {
	process_static(config.static_dir, config.destination_dir);
	process_mkd(config.source_dir, config.destination_dir);

	return 0;
}

void write_file(fs::path const& path, std::string const& data) {
	std::ofstream out(path);
	out.write(data.c_str(), data.size());
}

using mtime_t = decltype(fs::last_write_time(""));

mtime_t get_mtime(fs::path const& path) {
	return fs::last_write_time(path);
}

std::string format_mtime(mtime_t mtime) {
	std::time_t cftime = mtime_t::clock::to_time_t(mtime);
	return fmt::format("{:%Y-%m-%dT%H:%M:%SZ}", *gmtime(&cftime));
}

void App::update_file(std::string const& info,
	fs::path const& src, fs::path const& dst) {

	auto src_mtime = get_mtime(src);

	if(!config.rebuild && fs::exists(dst)) {
		auto dst_mtime = get_mtime(dst);

		if(src_mtime > dst_mtime) {
			fmt::print("UPDATE: {}\n", info);
		} else {
			return;
		}
	} else {
		fmt::print("COPY: {}\n", info);
	}

	fs::create_directories(dst.parent_path());
	fs::copy_file(src, dst, fs::copy_options::update_existing);
}

void App::create_file(std::string const& info, std::string const& data,
	fs::path const& src, fs::path const& dst) {

	auto src_mtime = get_mtime(src);

	if(!config.rebuild && fs::exists(dst)) {
		auto dst_mtime = get_mtime(dst);

		if(src_mtime > dst_mtime) {
			fmt::print("UPDATE: {}\n", info);
		} else {
			return;
		}
	} else {
		fmt::print("CREATE: {}\n", info);
	}

	fs::create_directories(dst.parent_path());
	write_file(dst, data);
}

void App::process_static(std::string const& src, std::string const& dst) {
	auto destination = fs::path(dst);

	for(auto const& p : fs::recursive_directory_iterator(src)) {
		if(!p.is_regular_file()) {
			continue;
		}

		auto path = p.path().lexically_relative(src);
		auto file = destination / path;

		update_file(path, p.path(), file);
	}
}

void App::process_mkd(std::string const& src, std::string const& dst) {
	auto destination = fs::path(dst);

	for(auto const& p : fs::recursive_directory_iterator(src)) {
		if(!p.is_regular_file()) {
			continue;
		}
		if(p.path().extension() != ".md") {
			continue;
		}

		auto author = config.cfg.get_value("author", "Unknown");
		auto base_url = config.cfg.get_value("base_url", "/");
		if(base_url.back() != '/') {
			base_url += '/';
		}

		std::string md = read_file(p.path().string());
		
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

		auto type = meta.get_value("type", "entry");
		bool is_page = type == "page";
		tmpl::Template& tmpl = is_page ? page_tmpl : entry_tmpl;

		auto root = tmpl.data();
		root->clear();

		auto path = p.path().lexically_relative(src);

		std::string title = meta.get_value("title", parser.title());
		std::string slug = meta.get_value("slug", parser.slug());
		if(title.empty()) {
			title = path.stem();
		}
		if(slug.empty()) {
			slug = slugify(title);
		}
		meta.set("title", title);
		root->set("title", title);

		if(auto meta_author = meta.get("author"); meta_author) {
			author = meta_author->value;
		}
		
		auto base = path.parent_path() / slug;
		auto info = base / "index.html";
		auto dst = destination / info;
		
		auto src_mtime = get_mtime(p.path());
		auto src_datetime = format_mtime(src_mtime);
		if(auto created = meta.get("created"); !created) {
			meta.set("created", src_datetime);
		} else if(created->value != src_datetime) {
			meta.set("updated", src_datetime);
		}
		root->set("datetime", src_datetime);
		root->set("date", src_datetime.substr(0, 10));
		root->set("url", base_url + info.string());

		if(is_page) {
			meta.set("type", "page");
		}

		auto tags = meta.get("tags");
		if(tags && tags->is_array) {
			auto block = root->block("tags");
			for(auto const& tag : tags->values) {
				auto& t = block->add();
				t.set("url", base_url + "tag/" + tag);
				t.set("name", tag);
			}
		}

		root->set("content", html);

		auto meta_files = meta.get("files");

		if(parser.files().size()) {
			if(!meta_files || (meta_files && !meta_files->is_array)) {
				// TODO: make less hacky way
				meta.add("files", "");
				meta_files = meta.get("files");
				meta_files->is_array = true;
			}
			for(auto const& file : parser.files()) {
				meta_files->values.push_back(file);
			}

			auto& vs = meta_files->values;
			//for(auto& v : vs) {
			//	// TODO: normalize values, make path relative to .md file
			//}
			std::sort(vs.begin(), vs.end());
			auto last = std::unique(vs.begin(), vs.end());
			vs.erase(last, vs.end());
		}


		write_file(p.path(), separator + meta.to_string() + separator + md);
		fs::last_write_time(p.path(), src_mtime);

		create_file(info, tmpl.make(), p.path(), dst);

		for(auto const& code : parser.codes()) {
			auto [file, data] = code;
			auto finfo = base / file;
			auto fpath = destination / finfo;
			create_file(finfo, data, p.path(), fpath);
		}


		if(meta_files && meta_files->values.size()) {
			auto src_dir = p.path().parent_path();
			for(auto const& file : meta_files->values) {
				auto src_file = src_dir / file;
				auto finfo = base / file;
				auto dst_file = destination / finfo;
				update_file(finfo, src_file, dst_file);
			}
		}
	}
}

} // namespace miu

