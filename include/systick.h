
int systick_update();
int systick_init(int tps);
void systick_set(uint64_t newticks);
uint64_t systick_get();
void systick_delay_sec(float sec);
void systick_delay_ticks(uint64_t delay);
float systick_elapsed_sec(uint64_t start);

// Use to get pointer to flash to save the time
extern volatile uint64_t systick_ticks;
