#include <memory>
#include <string>

#include "simulator/sim_driver.hpp"

namespace cli_parser {
std::unique_ptr<sim_driver::SimDriver> get_sim_driver(int, char**);
}