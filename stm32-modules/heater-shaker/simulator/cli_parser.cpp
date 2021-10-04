#include "simulator/cli_parser.hpp"

#include <boost/program_options.hpp>
#include <iostream>

#include "simulator/sim_driver.hpp"
#include "simulator/socket_sim_driver.hpp"
#include "simulator/stdin_sim_driver.hpp"

using namespace cli_parser;

[[noreturn]] void no_options_specified_error(
    boost::program_options::options_description desc) {
    std::cerr
        << std::endl
        << "ERROR: You must provide either the --stdin OR the --socket option."
        << std::endl
        << std::endl;
    std::cerr << desc << std::endl;
    exit(1);
}

[[noreturn]] void both_drivers_specified_error(
    boost::program_options::options_description desc) {
    std::cerr << std::endl
              << "ERROR: You may only provide either the --stdin OR the "
                 "--socket option, not both."
              << std::endl
              << std::endl;
    std::cerr << desc << std::endl;
    exit(1);
}

[[noreturn]] void neither_driver_error(
    boost::program_options::options_description desc) {
    std::cerr << std::endl
              << "ERROR: Neither --socket or --stdin was specified";
    std::cerr << desc << std::endl;
    exit(1);
}

sim_driver::SimDriver* cli_parser::get_sim_driver(int num_args, char* args[]) {
    bool use_stdin = false;
    bool use_socket = false;
    bool options_specified = num_args > 1;

    boost::program_options::options_description desc("Allowed options");
    desc.add_options()("help", "Show this help message")(
        "stdin", boost::program_options::bool_switch(&use_stdin),
        "Use stdin to provide G-Codes")("socket",
                                        boost::program_options::value<
                                            std::string>(),
                                        "Use socket to provide G-Codes");

    boost::program_options::variables_map vm;
    /*
     * Have to do this long crazy parser creation to make sure that partial
     * options are not accepted. For instance, `--std` was being accepted as
     * --stdin, and that's super confusing
     */
    auto parser =
        boost::program_options::command_line_parser(num_args, args)
            .options(desc)
            .style(boost::program_options::command_line_style::default_style &
                   ~boost::program_options::command_line_style::allow_guessing)
            .run();
    boost::program_options::store(parser, vm);
    boost::program_options::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
    }
    if (vm.count("socket")) {
        use_socket = true;
    }
    if (num_args <= 1) {
        no_options_specified_error(desc);
    }
    if (use_stdin && use_socket) {
        both_drivers_specified_error(desc);
    }

    sim_driver::SimDriver* sim_driver;

    if (use_stdin) {
        sim_driver = new stdin_sim_driver::StdinSimDriver();
    } else if (use_socket) {
        sim_driver = new socket_sim_driver::SocketSimDriver(
            vm["socket"].as<std::string>());
    } else {
        neither_driver_error(desc);
    }

    return sim_driver;
}