#ifndef HEADER_APP_HPP
#define HEADER_APP_HPP

#include <string>

#include <tmpl/tmpl.hpp>

#include "config.hpp"

#include "filesystem.hpp"

namespace miu {

class App {
	public:
		App(int argc, char** argv);
		~App();
		
		int run();
	private:
		Config config;
		tmpl::Template index_tmpl;
		tmpl::Template list_tmpl;
		tmpl::Template page_tmpl;
		tmpl::Template entry_tmpl;
		tmpl::Template feed_tmpl;

		void update_file(std::string const& info,
			fs::path const& src, fs::path const& dst);
		void create_file(std::string const& info, std::string const& data,
			fs::path const& src, fs::path const& dst);

		void process_static(std::string const& src, std::string const& dst);
		void process_mkd(std::string const& src, std::string const& dst);
};

} // namespace miu

#endif /* HEADER_APP_HPP */

