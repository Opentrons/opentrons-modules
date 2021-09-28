#include <string>



namespace cli_parser {
    enum SimType {STDIN, SOCKET};
    SimType get_sim_type(int, char**);
}