#ifndef HEADER_APP_HPP
#define HEADER_APP_HPP

#include <string>
#include <unordered_set>

#include <tmpl/tmpl.hpp>

#include "config.hpp"
#include "cache.hpp"

#include "filesystem.hpp"

namespace miu {

using mtime_t = decltype(fs::last_write_time(""));

class App {
	public:
		App(int argc, char** argv);
		~App();

		int run();
	private:
		Config config_;
		Cache cache_;
		tmpl::Template index_tmpl_;
		tmpl::Template list_tmpl_;
		tmpl::Template page_tmpl_;
		tmpl::Template entry_tmpl_;
		tmpl::Template feed_tmpl_;
		std::unordered_set<std::string> paths_;
		std::unordered_set<std::string> tags_;

		std::string init_tmpl(std::string const& path, const char* default_);

		mtime_t update_file(std::string const& info,
			fs::path const& src, fs::path const& dst);
		mtime_t create_file(std::string const& info, std::string const& data,
			fs::path const& src, fs::path const& dst);

		void process_static();
		void process_source();
		void process_mkd(fs::path const& src_path);
		void process_paths();
		void process_tags();
		void process_index();

		void config2tmpl(kvc::Config& conf, tmpl::Data::Value* root);
};

} // namespace miu

#endif /* HEADER_APP_HPP */

