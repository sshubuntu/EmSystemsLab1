#include "tm1637.h"

// Задержка (эмпирическая, ~24 такта на микросекунду при 48 МГц)
void delay_us(uint32_t us) {
    volatile uint32_t cycles = us * 24;
    while (cycles-- > 0) {
        __asm__("nop");
    }
}

// Настройка PA6 (CLK) и PA7 (DIO) как выходов с открытым стоком
void tm1637_init(void) {
    GPIOA->MODER = (GPIOA->MODER & ~(0xF << (TM1637_CLK_PIN * 2)))
                 | (0x5 << (TM1637_CLK_PIN * 2));
    GPIOA->OTYPER |= (1 << TM1637_CLK_PIN) | (1 << TM1637_DIO_PIN);
    GPIOA->OSPEEDR |= (0x3 << (TM1637_CLK_PIN * 2))
                     | (0x3 << (TM1637_DIO_PIN * 2));
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(0xFU << 12)) | (0x5U << 12);
    GPIOA->BSRR = (1 << TM1637_CLK_PIN) | (1 << TM1637_DIO_PIN);

    tm1637_start();
    tm1637_write_byte(TM1637_CMD1);
    tm1637_stop();

    tm1637_start();
    tm1637_write_byte(TM1637_CMD3);
    tm1637_stop();

    tm1637_clear();
    tm1637_display_number(0);
}

void tm1637_start(void) {
    GPIOA->BSRR = (1 << TM1637_DIO_PIN) | (1 << TM1637_CLK_PIN);
    delay_us(2);
    GPIOA->BRR = (1 << TM1637_DIO_PIN);
    delay_us(2);
    GPIOA->BRR = (1 << TM1637_CLK_PIN);
    delay_us(2);
}

void tm1637_stop(void) {
    GPIOA->BRR = (1 << TM1637_CLK_PIN);
    delay_us(2);
    GPIOA->BRR = (1 << TM1637_DIO_PIN);
    delay_us(2);
    GPIOA->BSRR = (1 << TM1637_CLK_PIN);
    delay_us(2);
    GPIOA->BSRR = (1 << TM1637_DIO_PIN);
    delay_us(2);
}

void tm1637_write_byte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        GPIOA->BRR = (1 << TM1637_CLK_PIN);
        delay_us(2);

        if (byte & 0x01) {
            GPIOA->BSRR = (1 << TM1637_DIO_PIN);
        } else {
            GPIOA->BRR = (1 << TM1637_DIO_PIN);
        }
        delay_us(2);

        GPIOA->BSRR = (1 << TM1637_CLK_PIN);
        delay_us(2);

        byte >>= 1;
    }

    GPIOA->BRR = (1 << TM1637_CLK_PIN);
    GPIOA->BSRR = (1 << TM1637_DIO_PIN);
    delay_us(2);
    GPIOA->BSRR = (1 << TM1637_CLK_PIN);
    delay_us(2);

    GPIOA->BRR = (1 << TM1637_CLK_PIN);
    delay_us(2);
}

void tm1637_display_digit(uint8_t digit, uint8_t data) {
    tm1637_start();
    tm1637_write_byte(TM1637_CMD2 | digit);
    tm1637_write_byte(data);
    tm1637_stop();

    tm1637_start();
    tm1637_write_byte(TM1637_CMD3);
    tm1637_stop();
}

const uint8_t digit_codes[] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F, // 9
    0x77, // A
    0x7C, // b
    0x39, // C
    0x5E, // d
    0x79, // E
    0x71  // F
};

void tm1637_display_number(int number) {
    uint8_t digits[4];

    digits[0] = digit_codes[number % 10];
    digits[1] = (number >= 10) ? digit_codes[(number / 10) % 10] : 0;
    digits[2] = (number >= 100) ? digit_codes[(number / 100) % 10] : 0;
    digits[3] = (number >= 1000) ? digit_codes[(number / 1000) % 10] : 0;

    tm1637_start();
    tm1637_write_byte(0x40);
    tm1637_stop();

    tm1637_start();
    tm1637_write_byte(0xC0);

    for (int i = 3; i >= 0; i--) {
        tm1637_write_byte(digits[i]);
    }

    tm1637_stop();

    tm1637_start();
    tm1637_write_byte(0x8F);
    tm1637_stop();
}

void tm1637_clear(void) {
    for (uint8_t i = 0; i < 4; i++) {
        tm1637_display_digit(i, 0x00);
    }
}

void tm1637_text(const char text[4]) {
    tm1637_start();
    tm1637_write_byte(0x40);
    tm1637_stop();

    tm1637_start();
    tm1637_write_byte(0xC0);

    for (unsigned i = 0; i < 4; i++) {
        char c = text[i];
        uint8_t segments = 0;

        if (c >= '0' && c <= '9')
            segments = digit_codes[c - '0'];
        else if (c == '-')
            segments = 0x40;
        else if (c == 'O')
            segments = 0x3F;
        else if (c == 'P')
            segments = 0x73;
        else if (c == 'E')
            segments = 0x79;
        else if (c == 'N')
            segments = 0x54;
        else if (c == 'r')
            segments = 0x50;

        tm1637_write_byte(segments);
    }

    tm1637_stop();

    tm1637_start();
    tm1637_write_byte(0x8F);
    tm1637_stop();
}
