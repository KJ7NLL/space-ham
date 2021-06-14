
int systick_update();
int systick_init(int tps);
void systick_set(uint64_t newticks);
uint64_t systick_get();
void systick_delay(uint64_t delay);

// Use to get pointer to flash to save the time
volatile extern uint64_t systick_ticks;
