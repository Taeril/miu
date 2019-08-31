#ifndef HEADER_APP_HPP
#define HEADER_APP_HPP

#include "config.hpp"

namespace miu {

class App {
	public:
		App(int argc, char** argv);
		~App();
		
		int run();
	private:
		Config config;

		void process_static(std::string const& src, std::string const& dst);
};

} // namespace miu

#endif /* HEADER_APP_HPP */

