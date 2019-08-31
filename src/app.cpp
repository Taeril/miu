#include "app.hpp"

#include <string>
#include <tuple>

#include <fmt/core.h>
#include <fmt/time.h>

#include "filesystem.hpp"

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
}

App::~App() {
}

int App::run() {
	process_static(config.static_dir, config.destination_dir);

	return 0;
}

using mtime_t = decltype(fs::last_write_time(""));

std::tuple<mtime_t, std::string> get_mtime(fs::path const& path) {
	auto ftime = fs::last_write_time(path);
	std::time_t cftime = decltype(ftime)::clock::to_time_t(ftime);
	std::string datetime = fmt::format("{:%Y-%m-%dT%H:%M:%SZ}", *gmtime(&cftime));

	return make_tuple(ftime, datetime);
}

void update_file(std::string const& info, fs::path const& src, fs::path const& dst) {

	auto [src_mtime, src_datetime] = get_mtime(src);

	if(fs::exists(dst)) {
		auto [dst_mtime, dst_datetime] = get_mtime(dst);

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

} // namespace miu

