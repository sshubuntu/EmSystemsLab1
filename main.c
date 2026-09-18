#include "main.h"
#include "tm1637.h"
#include "keyboard.h"

volatile uint32_t tickCount;

// Сохранённый пароль
static char accessCode[5] = "";

// 0 - пароль ещё не установлен
// 1 - пароль установлен
static int codeSet = 0;

// Текущий ввод
static char entered[5];
static unsigned count = 0;

// Результат OPEN / Err
static uint32_t resultTime;
static int showingResult = 0;

// 1 - сейчас подтверждаем сброс пароля
static int resetMode = 0;

void osSystickHandler(void) {
    tickCount++;
}

void initGPIO(void) {
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN;

    // PA9 и PA15 - светодиоды
    for (unsigned pin = 9; pin <= 15; pin += 6) {
        GPIOA->MODER = (GPIOA->MODER & ~(3U << (2 * pin))) |(1U << (2 * pin));
        GPIOA->OTYPER &= ~(1U << pin);
    }

    // PA5 - индикатор готовности
    GPIOA->MODER = (GPIOA->MODER & ~(3U << 10)) | (1U << 10);
    GPIOA->OTYPER &= ~(1U << 5);
    GPIOA->OSPEEDR |= (1U << 10);
}


void initUSART2(void) {
    RCC->APBENR1 |= RCC_APBENR1_USART2EN;
    GPIOA->MODER = (GPIOA->MODER & ~(0xFU << 4)) | (0xAU << 4);
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFFU << 8)) | (1U << 8) | (1U << 12);
    USART2->BRR = 417;
    USART2->CR1 =
        USART_CR1_TE |
        USART_CR1_UE;
}


void initSysTick(void) {
    SysTick->LOAD = 47999;
    SysTick->VAL = 0;
    SysTick->CTRL =
        (1U << 2) |
        (1U << 1) |
        (1U << 0);
}


int _write(int file, uint8_t *ptr, int len) {
    for (int i = 0; i < len; i++) {
        while (!(USART2->ISR & USART_ISR_TXE));
        USART2->TDR = ptr[i];
    }
    
    return len;
}

// Очистка текущего ввода
static void clearEntry(void) {
    count = 0;
    memset(entered, 0, sizeof(entered));
    showingResult = 0;
    // Выключаем оба светодиода
    GPIOA->BSRR =
        (1U << (9 + 16)) |
        (1U << (15 + 16));

    tm1637_text("----");
}


// Вход в режим сброса пароля
static void startResetMode(void) {

    // Если пароль ещё не установлен, сбрасывать нечего
    if (!codeSet) {
        printf("NO CODE TO RESET\n");
        return;
    }
    clearEntry();
    resetMode = 1;
    // Показываем, что вошли в режим Reset
    tm1637_text("r---");
    printf("RESET MODE\n");
    printf("ENTER CURRENT PASSWORD AND PRESS #\n");
}

static void resetAccessCode(void) {

    memset(accessCode, 0, sizeof(accessCode));
    codeSet = 0;
    resetMode = 0;
    clearEntry();
    printf("PASSWORD RESET SUCCESSFULLY\n");
    printf("ENTER NEW PASSWORD AND PRESS #\n");
}

// Установка нового пароля
static void saveNewCode(void) {

    strcpy(accessCode, entered);

    codeSet = 1;

    printf("NEW PASSWORD SAVED\n");

    clearEntry();
}

// Обработка клавиатуры
static void handleKey(char key) {

    // ДОЛГОЕ НАЖАТИЕ *
    if (key == 'R') {
        startResetMode();
        return;
    }
    // КОРОТКОЕ НАЖАТИЕ *
    if (key == '*') {
        // Если находимся в режиме сброса, отменяем сброс
        if (resetMode) {
            resetMode = 0;
            clearEntry();
            printf("RESET CANCELLED\n");
            return;
        }
        // В обычном режиме просто очищаем текущий ввод
        clearEntry();
        printf("INPUT CLEARED\n");
        return;
    }
    // Пока показывается OPEN или Err
    if (showingResult) {
        return;
    }
    // ВВОД ЦИФР
    if (key >= '0' && key <= '9') {
        if (count >= 4) {
            printf("FULL: PRESS # OR *\n");
            return;
        }
        entered[count++] = key;
        entered[count] = '\0';
        char text[5] = "----";
        memcpy(text, entered, count);
        tm1637_text(text);
        printf("INPUT: %s\n", text);
        return;
    }

    if (key == '#') {
        if (count != 4) {
            printf("ENTER 4 DIGITS\n");
            return;
        }
        // РЕЖИМ СБРОСА
        if (resetMode) {
            if (strcmp(entered, accessCode) == 0) {
                printf("PASSWORD CORRECT\n");
                resetAccessCode();

            } else {
                printf("WRONG PASSWORD\n");

                GPIOA->BSRR =
                    (1U << 15) |
                    (1U << (9 + 16));

                tm1637_text("Err ");

                showingResult = 1;
                resultTime = tickCount;
            }
            return;
        }

        if (!codeSet) {
            saveNewCode();
            printf("PASSWORD SET\n");
            return;
        }

        int ok = strcmp(entered, accessCode) == 0;
        if (ok) {
            GPIOA->BSRR = (1U << 9) | (1U << (15 + 16));
            tm1637_text("OPEN");
            printf("ACCESS GRANTED\n");
        } else {
            // Красный светодиод
            GPIOA->BSRR = (1U << 15) | (1U << (9 + 16));
            tm1637_text("Err ");
            printf("ACCESS DENIED\n");
        }
        showingResult = 1;
        resultTime = tickCount;
    }
}

int main(void) {
    initGPIO();
    initUSART2();
    initSysTick();
    initKeyboard();
    tm1637_init();
    GPIOA->BSRR = 1U << 5;
    clearEntry();
    printf("ENTER NEW 4-DIGIT PASSWORD\n");

    while (1) {
        char key = scanKeyboard();

        // Через 3 секунды убираем OPEN / Err
        if (showingResult &&
            (uint32_t)(tickCount - resultTime) >= 3000U) {

            clearEntry();
            // Если ошибка была при попытке reset, остаёмся в режиме reset
            if (resetMode) {
                tm1637_text("r---");
                printf("ENTER CURRENT PASSWORD AGAIN\n");
            }
        }
        if (key) {
            handleKey(key);
        }
    }
}