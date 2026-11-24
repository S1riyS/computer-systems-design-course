# Откат USART6 с `usart_drv` на HAL

Прошивка на HAL — коммит `ddaa8dd` («Dump lab 1-3 changes + theory»). В нём есть `usart.c` и `usart.h`, `uart.c` ходит в `HAL_UART_*`, `main` вызывает `MX_USART6_UART_Init`, USART6 включён в `lab2.ioc`. Свой драйвер в этот коммит не входил.

После коммита со `usart_drv` хеш `ddaa8dd` не меняется. Откатываться нужно к нему, не к `HEAD`. `git reset --hard` не использовать: он снимет и остальные незакоммиченные правки.

`gpio_drv`, светодиоды и кнопка здесь ни при чём. Их откат этим коммитом не делается.

## Команды

Из корня репозитория:

```sh
git checkout ddaa8dd -- \
  lab2/source/Core/Inc/stm32f4xx_hal_conf.h \
  lab2/source/Core/Inc/uart.h \
  lab2/source/Core/Inc/usart.h \
  lab2/source/Core/Src/gpio.c \
  lab2/source/Core/Src/main.c \
  lab2/source/Core/Src/stm32f4xx_it.c \
  lab2/source/Core/Src/uart.c \
  lab2/source/Core/Src/usart.c \
  lab2/source/lab2.ioc \
  lab2/source/.mxproject

rm -f lab2/source/Core/Src/usart_drv.c lab2/source/Core/Inc/usart_drv.h
```

`checkout` не удаляет неотслеживаемые файлы. `usart_drv.c` тоже определяет `USART6_IRQHandler`, и вместе с восстановленным `stm32f4xx_it.c` линкер получит два символа. Оба файла драйвера нужно стереть.

Дальше чистая пересборка в CubeIDE, чтобы в прошивке не остался `usart_drv.o`.

Отчёт и `lab2/theory/lr2.md` описывают уже свой драйвер. Чтобы вернуть и их текст из того же коммита:

```sh
git checkout ddaa8dd -- lab2/report/main.typ lab2/report/main.pdf lab2/theory/lr2.md
```

Этот файл (`rollback-hal-uart.md`) в `ddaa8dd` нет, `checkout` его не трогает.
