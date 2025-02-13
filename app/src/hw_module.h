#ifndef hw_module_h
#define hw_module_h

void send_usb_cdc(const char *buf, size_t len);

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