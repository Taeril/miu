#include "app.hpp"

namespace miu {

App::App(int argc, char** argv) : config(argc, argv) {
}

App::~App() {
}

int App::run() {
	return 0;
}

} // namespace miu

