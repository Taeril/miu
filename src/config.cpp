#include "config.hpp"
#include "version.hpp"

#include <cstdlib>
#include <optional>

#include "filesystem.hpp"

#include <fmt/core.h>
#if __has_include(<fmt/time.h>) && FMT_VERSION < 60000
#include <fmt/time.h>
#endif
#if __has_include(<fmt/chrono.h>)
#include <fmt/chrono.h>
#endif
//#include <fmt/ostream.h>

// https://github.com/adishavit/argh
#include "argh.h"


namespace miu {

static const char* help_str = R"~(miu v{}

usage:
  {} [options] [FILES...]
Available options:
  -c, --conf, --config       <file>   - use this configuration file
                                        (disables searching for miu.conf)
  -r, --root                 <path>   - root directory (default: ./)
  -C, --cache                <file>   - cache file (default: ./cache,db)
  -s, --src, --source        <path>   - source directory (default: ./content)
  -d, --dest, --destination  <path>   - destination directory (default: ./public)
  -f, --files, --static      <path>   - static source directory (default: ./static)
  -t, --tmpl, --template     <path>   - directory with templates (default: ./template)
  -R, --rebuild                       - ignore cache and recreate everything
  -v, --verbose                       - verbose output (levels: 0-2)
                                        (use multiple times to increase level)
  -V, --version                       - display version
  -h, -?, --help                      - show this help and exit
)~";

Config::Config(int argc, char** argv) {
	argh::parser args({
		"c", "conf", "config",
		"r", "root",
		"C", "cache",
		"s", "src", "source",
		"d", "dest", "destination",
		"f", "files", "static",
		"t", "tmpl", "template",
	});

	args.parse(argc, argv, 0
		//| argh::parser::PREFER_PARAM_FOR_UNREG_OPTION
		| argh::parser::SINGLE_DASH_IS_MULTIFLAG
	);

	auto prog = args(0).str();

	auto flags = args.flags();
	verbose = static_cast<int>(flags.count("verbose") + flags.count("v"));

	auto conf = args({"config", "conf", "c"});
	auto root = args({"root", "r"});
	auto cache = args({"cache", "C"});
	auto src = args({"source", "src", "s"});
	auto dest = args({"destination", "dest", "d"});
	auto static_files = args({"static", "files", "f"});
	auto tmpl = args({"template", "tmpl", "t"});

	if(args[{"help", "h", "?"}]) {
		fmt::print(help_str, VERSION, prog);
		std::exit(0);
	}

	if(args[{"version", "V"}]) {
		fmt::print("miu v{}\n", VERSION);
		std::exit(0);
	}

	rebuild = args[{"rebuild", "R"}];

	for(size_t i=1; i<args.size(); ++i) {
		files.push_back(args(i).str());
	}


	auto cwd = fs::current_path();
	auto root_path = cwd.root_path();

	std::optional<std::string> miu_conf;
	if(bool(conf)) {
		miu_conf = conf.str();
	}

	if(!miu_conf) {
		auto p = cwd;
		do {
			auto conf_path = p / "miu.conf";
			if(fs::exists(conf_path)) {
				miu_conf = conf_path.string();
			} else {
				p = p.parent_path();
			}
		} while(!miu_conf && p != root_path);
	}

	if(miu_conf) {
		if(fs::exists(*miu_conf)) {
			cfg.parse_file(*miu_conf);
		} else {
			fmt::print(stderr, "Configuration file '{}' does not exists.\n", *miu_conf);
			std::exit(1);
		}
	}

	auto my_path = miu_conf ? fs::path(*miu_conf).remove_filename() : cwd;
	root_dir = cfg.get_value("root", my_path.string());
	root_path = fs::path(root_dir);
	if(root_path.is_relative()) {
		root_path = my_path / root_path;
	}
	root_path = root_path.lexically_normal();
	root_dir = root_path.string();

	cache_db = cfg.get_value("cache", (root_path / "cache.db").string());
	source_dir = cfg.get_value("source", (root_path / "content").string());
	destination_dir = cfg.get_value("destination", (root_path / "public").string());
	static_dir = cfg.get_value("static", (root_path / "static").string());
	template_dir = cfg.get_value("template", (root_path / "template").string());

	cfg.add("", "", "");
	cfg.add("", "", "autogenerated:");
	cfg.add("cache", cache_db);
	cfg.add("source", source_dir);
	cfg.add("destination", destination_dir);
	cfg.add("static", static_dir);
	cfg.add("template", template_dir);


	auto base_url = cfg.get_value("base_url", "/");
	if(base_url.back() != '/') {
		base_url += '/';
	}
	cfg.set("base_url", base_url);

	cfg.add_new("author", "Unknown");
	cfg.add_new("home_name", "Home");
	cfg.add_new("tags_name", "Tags");

	cfg.set("home_url", base_url);
	cfg.set("tags_url", base_url + "tags/");

	std::time_t ctime = std::time(nullptr);
	cfg.set("now", fmt::format("{:%Y-%m-%dT%H:%M:%SZ}", *std::gmtime(&ctime)));
}

Config::~Config() {
}

} // namespace miu

