#include <memory>
#include <string>

#include "simulator/sim_driver.hpp"

namespace cli_parser {
/**
 * First value is the sim input driver, second input is a boolean
 * set to true if the sim should run in realtime and false if it
 * should run in simulated time
 */
using RT = std::pair<std::shared_ptr<sim_driver::SimDriver>, bool>;

/**
 * Parse the inputs and determine 1) what kind of input should be
 * used 2) whether the simulation should be realtime or accelerated
 */
RT get_sim_driver(int, char**);
}  // namespace cli_parser
