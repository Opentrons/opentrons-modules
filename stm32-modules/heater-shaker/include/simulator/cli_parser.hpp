#include <string>
#include <memory>
#include "simulator/sim_driver.hpp"


namespace cli_parser {
    sim_driver::SimDriver* get_sim_driver(int, char**);
}