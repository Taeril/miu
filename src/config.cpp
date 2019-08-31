#include "config.hpp"
#include "version.hpp"

#include <cstdlib>
#include <optional>

#include "filesystem.hpp"

#include <fmt/core.h>
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

#if 0
	fmt::print("conf: [{}] {}\n", bool(conf), conf.str());
	fmt::print("root: [{}] {}\n", bool(root), root.str());
	fmt::print("cache: [{}] {}\n", bool(cache), cache.str());
	fmt::print("src: [{}] {}\n", bool(src), src.str());
	fmt::print("dest: [{}] {}\n", bool(dest), dest.str());
	fmt::print("static: [{}] {}\n", bool(static_files), static_files.str());
	fmt::print("tmpl: [{}] {}\n", bool(tmpl), tmpl.str());
#endif

#if 0
	fmt::print("Positional args:\n");
	size_t n = 0;
	for(auto& pos_arg : args.pos_args())
		fmt::print("{}. {}\n", n++, pos_arg);

	fmt::print("\nFlags:\n");
	for(auto& flag : args.flags())
		fmt::print(" -{}\n", flag);

	fmt::print("\nParameters:\n");
	for(auto& param : args.params())
		fmt::print("  {}: {}\n", param.first, param.second);
#endif


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

#if 0
	fmt::print("verbose: {}\n", verbose);
	fmt::print("rebuild: {}\n", rebuild);
	fmt::print("conf: [{}] {}\n", bool(miu_conf), miu_conf ? *miu_conf : "");
	fmt::print("root: {}\n", root_dir);
	fmt::print("cache: {}\n", cache_db);
	fmt::print("source: {}\n", source_dir);
	fmt::print("destination: {}\n", destination_dir);
	fmt::print("static: {}\n", static_dir);
	fmt::print("template: {}\n", template_dir);

	fmt::print("files({}):\n", files.size());
	for(auto const& file : files) {
		fmt::print(" - {}\n", file);
	}

	fmt::print("cfg:\n{}\n", cfg.to_string());
#endif
}

Config::~Config() {
}

} // namespace miu

