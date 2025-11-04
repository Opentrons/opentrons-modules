#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdbool.h>
#include <stdint.h>

void pump_hardware_init(void);
void hw_start_pump_motor(bool start);
void hw_set_pump_duty_cycle(int16_t duty);
uint16_t hw_get_pump_duty_cycle(void);
float hw_get_pump_rpm(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
