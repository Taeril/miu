#ifndef HEADER_APP_HPP
#define HEADER_APP_HPP

#include <string>

#include <tmpl/tmpl.hpp>

#include "config.hpp"

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

		void process_static(std::string const& src, std::string const& dst);
		void process_mkd(std::string const& src, std::string const& dst);
};

} // namespace miu

#endif /* HEADER_APP_HPP */

