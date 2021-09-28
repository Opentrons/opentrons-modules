#include <iostream>

#include "heater-shaker/tasks.hpp"
#include "simulator/comm_thread.hpp"
#include "simulator/heater_thread.hpp"
#include "simulator/motor_thread.hpp"
#include "simulator/simulator_queue.hpp"
#include "simulator/system_thread.hpp"
#include <boost/program_options.hpp>

using namespace std;

int main(int argc, char *argv[]) {
    bool use_stdin = false;
    bool use_socket = false;

    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "produce help message")
            ("stdin", po::bool_switch(&use_stdin), "Use stdin to provide G-Codes")
            ("socket", po::bool_switch(&use_socket), "Use socket to provide G-Codes")
            ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    bool options_provided = argc > 1;
    bool both_stdin_and_socket_specified = use_stdin && use_socket;

    if (!options_provided) {
        cout << endl << "ERROR: You must provide either the --stdin OR the --socket option" << endl << endl;
        cout << desc << endl;
        exit(1);
    }

    if (both_stdin_and_socket_specified) {
        cout << endl << "ERROR: You may only provide either the --stdin OR the --socket option, not both" << endl << endl;
        cout << desc << endl;
        exit(1);
    }

    if (use_stdin) {
        cout << "Using stdin" << endl;
    }

    if (use_socket) {
        cout << "Using socket" << endl;
    }
}
