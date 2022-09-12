#ifndef TACHOMETER_HARDWARE_H_
#define TACHOMETER_HARDWARE_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

void tachometer_hardware_init();
double tachometer_hardware_get_rpm();

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus
#endif  /* TACHOMETER_HARDWARE_H_ */
