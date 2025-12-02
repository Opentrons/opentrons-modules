#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

void pump_hardware_init(void);
bool hw_start_pump_motor();
bool hw_stop_pump_motor();
void hw_set_pump_duty_cycle(uint8_t duty);
uint8_t hw_get_pump_duty_cycle(void);
bool hw_enable_pump_tach(bool enable);
float hw_get_pump_rpm(void);
void tach_period_overflow_callback(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
