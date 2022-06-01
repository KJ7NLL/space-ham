
int rtcc_update();
void rtcc_set_bypass(int b);
int rtcc_init(int tps);
void rtcc_set(uint64_t newticks);
uint64_t rtcc_get();
uint64_t rtcc_get_sec();
void rtcc_set_sec(uint64_t t);
void rtcc_delay_sec(float sec);
void rtcc_delay_ticks(uint64_t delay);
float rtcc_elapsed_sec(uint64_t start);
