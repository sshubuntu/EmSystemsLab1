#include "keyboard.h"

void initKeyboard() {
  // Настройка строк (R1-R4) как выходы с открытым стоком
  // PB7, PB6, PB3, PA10
  GPIOB->MODER = (GPIOB->MODER & ~(3U << (7 * 2))) | (1U << (7 * 2));
  GPIOB->MODER = (GPIOB->MODER & ~(3U << (6 * 2))) | (1U << (6 * 2));
  GPIOB->MODER = (GPIOB->MODER & ~(3U << (3 * 2))) | (1U << (3 * 2));
  GPIOA->MODER = (GPIOA->MODER & ~(3U << (10 * 2))) | (1U << (10 * 2));

  GPIOB->OTYPER |= (1 << 7) | (1 << 6) | (1 << 3);
  GPIOA->OTYPER |= (1 << 10);

  // Настройка столбцов (C1-C3) как входы с подтяжкой к питанию
  // PB10, PB4, PB5
  GPIOB->MODER &= ~(3U << (10 * 2) | 3U << (4 * 2) | 3U << (5 * 2));
  GPIOB->PUPDR = (GPIOB->PUPDR & ~(3U << (10 * 2))) | (1U << (10 * 2));
  GPIOB->PUPDR = (GPIOB->PUPDR & ~(3U << (4 * 2))) | (1U << (4 * 2));
  GPIOB->PUPDR = (GPIOB->PUPDR & ~(3U << (5 * 2))) | (1U << (5 * 2));

  GPIOB->BSRR = (1U << 7) | (1U << 6) | (1U << 3);
  GPIOA->BSRR = 1U << 10;
}

char readKey() {
  const uint8_t rows[] = {7, 6, 10, 3};    // Пины строк
  const uint8_t cols[] = {10, 4, 5};       // Пины столбцов
  const char keymap[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
  };

  for (uint8_t i = 0; i < 4; i++) {
    // Активируем текущую строку
    if (rows[i] != 10) {
      GPIOB->BSRR = (1 << (rows[i] + 16));
    } else {
      GPIOA->BSRR = (1 << (rows[i] + 16));
    }

    // Небольшая задержка для стабилизации
    for (volatile int d = 0; d < 100; d++);

    // Проверяем столбцы
    for (uint8_t j = 0; j < 3; j++) {
      if ((GPIOB->IDR & (1 << cols[j])) == 0) {
        // Деактивируем строку перед возвратом
        if (rows[i] != 10) {
          GPIOB->BSRR = (1 << rows[i]);
        } else {
          GPIOA->BSRR = (1 << rows[i]);
        }
        return keymap[i][j];
      }
    }

    // Деактивируем строку
    if (rows[i] != 10) {
      GPIOB->BSRR = (1 << rows[i]);
    } else {
      GPIOA->BSRR = (1 << rows[i]);
    }
  }

  return '\0';
}

// Событие только после 30 мс стабильного состояния; удержание не повторяется.
char scanKeyboard(void) {
    static uint32_t lastScan = 0;
    static uint32_t changed = 0;
    static uint32_t pressStart = 0;

    static char candidate = 0;
    static char stable = 0;

    static int longPressSent = 0;

    uint32_t now = tickCount;

    // Сканируем клавиатуру каждые 5 мс
    if ((uint32_t)(now - lastScan) < 5U)
        return 0;

    lastScan = now;

    char key = readKey();

    // Кнопка изменилась
    if (key != candidate) {
        candidate = key;
        changed = now;
    }

    // Антидребезг 30 мс
    if ((uint32_t)(now - changed) >= 30U && stable != candidate) {

        char oldKey = stable;
        stable = candidate;

        // Кнопку нажали
        if (stable) {
            pressStart = now;
            longPressSent = 0;

            // Все кнопки кроме * возвращаем сразу
            if (stable != '*') {
                return stable;
            }
        }

        // Кнопку отпустили
        else {
            // Короткое нажатие *
            if (oldKey == '*' && !longPressSent) {
                return '*';
            }
        }
    }

    // Удерживаем * 2 секунды
    if (stable == '*' &&
        !longPressSent &&
        (uint32_t)(now - pressStart) >= 2000U) {

        longPressSent = 1;
        return 'R';
    }

    return 0;
}