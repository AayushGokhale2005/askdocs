#include "askdocs/app.hpp"
#include "askdocs/file_io.hpp"

#include <argparse/argparse.hpp>

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    argparse::ArgumentParser program("askdocs", "0.2.0");
    program.add_description("Terminal code editor with an online-docs AI tutor");
    program.add_argument("file")
        .help("Initial file to open")
        .nargs(argparse::nargs_pattern::optional);
    program.add_argument("-C", "--directory")
        .help("Working directory for the file explorer and @ mentions")
        .default_value(std::string{});
    program.add_argument("--offline").help("Disable online doc / web search").flag();

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << '\n' << program;
        return 1;
    }

    if (program.get<bool>("--offline")) {
        setenv("ASKDOCS_ONLINE", "0", 1);
    }

    askdocs::AppOptions options;
    options.working_directory = askdocs::get_working_directory();

    const std::string directory = program.get<std::string>("--directory");
    if (!directory.empty()) {
        options.working_directory = directory;
    }

    if (program.is_used("file")) {
        options.initial_file = program.get<std::string>("file");
    }

    askdocs::App app(std::move(options));
    return app.run();
}
