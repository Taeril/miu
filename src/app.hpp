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
};

} // namespace miu

#endif /* HEADER_APP_HPP */

