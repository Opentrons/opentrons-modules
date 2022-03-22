#include <memory>
#include <string>

#include "simulator/sim_driver.hpp"

namespace cli_parser {
using RT = std::pair<std::shared_ptr<sim_driver::SimDriver>, bool>;
RT get_sim_driver(int, char**);
}  // namespace cli_parser
