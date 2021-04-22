void initUsart1(void);
void USART1_RX_IRQHandler(void);
void USART1_TX_IRQHandler(void);
void serial_write(void *s, int len);
void serial_read(void *s, int len);
void print(char *s);
void print_back(char *s);

int esc_key(char **keys);
int input(char *buf, int len, struct linklist *history);
