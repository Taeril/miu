#ifndef HEADER_CONFIG_HPP
#define HEADER_CONFIG_HPP

#include <string>
#include <vector>

#include <kvc/kvc.hpp>

namespace miu {

struct Config {
	Config(int argc, char** argv);
	~Config();

	kvc::Config cfg;

	std::string root_dir;
	std::string cache_db;
	std::string source_dir;
	std::string destination_dir;
	std::string static_dir;
	std::string template_dir;
	std::vector<std::string> files;

	int verbose = 0;
	bool rebuild = false;
};

} // namespace miu

#endif /* HEADER_CONFIG_HPP */

