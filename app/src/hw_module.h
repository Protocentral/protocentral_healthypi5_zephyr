#ifndef hw_module_h
#define hw_module_h

uint32_t hw_keypad_get_key(void);


int16_t hpi_hw_read_temp(void);

void send_usb_cdc(const char *buf, size_t len);

int readADC(void);

enum gpio_keypad_key
{
    GPIO_KEYPAD_KEY_NONE = 0,
    GPIO_KEYPAD_KEY_OK,
    GPIO_KEYPAD_KEY_UP,
    GPIO_KEYPAD_KEY_DOWN,
    GPIO_KEYPAD_KEY_LEFT,
    GPIO_KEYPAD_KEY_RIGHT,
};

#endif