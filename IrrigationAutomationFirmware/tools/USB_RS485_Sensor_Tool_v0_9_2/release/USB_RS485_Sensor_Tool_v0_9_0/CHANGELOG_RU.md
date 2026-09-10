# CHANGELOG

## v0.9.0

- Добавлен профиль `TUF-2000M + TS-2 Ultrasonic Flow Meter` для Modbus RTU
  `9600 8N1`.
- Одна операция `Read Device` последовательно читает REG0001-0006, REG0072,
  REG0092-0094, REG0097-0098, REG0221-0222 и REG1442.
- Для каждого запроса постоянно видны полные TX/RX кадры, CRC-статус и время.
- Добавлен разбор расхода, скорости, M08 error bits, M90 quality/raw strengths,
  M91 time ratio, внутреннего диаметра и адреса M46.
- REAL4 выводится в вариантах HIGH_WORD_FIRST и LOW_WORD_FIRST; программа не
  выбирает порядок без сравнения с реальным прибором.
- Добавлены готовые TUF-запросы во вкладку Raw Modbus.
- Профиль TUF read-only: смена M46 оставлена штатной клавиатуре прибора.

## v0.7.0

- Добавлена смена адреса EPEVER через найденную custom service-команду `0x45`:
  - чтение/поиск ID: `F8 45 00 01 01 F8 + CRC`;
  - запись ID: `F8 45 00 01 01 NN + CRC`;
  - пример для `0x60`: `F8 45 00 01 01 60 88 14`.
- EPEVER address change добавлен в обычную вкладку **Change Address** для профиля `Epever Solar Controller / Tracer-AN G3`.
- Вкладка **EPEVER Config** получила кнопки:
  - `Find/Read EPEVER ID`;
  - `CHANGE EPEVER ID`.
- После смены ID программа проверяет новый и старый адрес обычным FC04-запросом battery voltage.
- Raw Modbus examples дополнены:
  - `EPEVER custom find ID`;
  - `EPEVER custom set ID 0x60`.
- Важно: команда `0x45` является proprietary/custom, не обычным Modbus FC06/FC10. Использовать только когда на линии подключён один EPEVER controller.

## v0.6.0

- Добавлена вкладка **EPEVER Config** для Tracer-AN G3 / XTRA-N G3 / Tracer-CPN G3.
- Добавлено чтение EPEVER настроек:
  - `0x9000..0x9002` battery type / capacity / temperature compensation;
  - `0x9003..0x900E` voltage block;
  - `0x9067` rated voltage level;
  - `0x90BF` maximum charging current.
- Добавлена запись EPEVER настроек через Modbus FC10:
  - Battery type = User (`0x9000 = 0`);
  - Capacity Ah из `config.h`;
  - Voltage block `0x9003..0x900E` одной группой;
  - Rated voltage level `0x9067`;
  - Max charge current `0x90BF`.
- Добавлены пресеты из `config.h`:
  - `PKCELL 2x 12V9Ah parallel`, capacity 18Ah, max charge current 3.60A;
  - `StarOne STR12120 12V12Ah`, capacity 12Ah, max charge current 3.60A.
- Добавлены Raw Modbus examples для EPEVER.

## v0.5.0

- Added Honde SHT45 / Air T-H RS485 address change from uploaded Honde datasheet.

## v0.8.0

- Исправлена EPEVER вкладка: TX/RX лог теперь всегда виден справа, а настройки слева прокручиваются.
- Исправлена ложная блокировка записи voltage block: 0x900A Low Voltage Recovery и 0x900B Undervoltage Alarm Recovery больше не сравниваются как одна строгая цепочка. Значения из config.h теперь проходят проверку.
- Ошибки подготовки записи теперь также пишутся в EPEVER TX/RX окно, а не только показываются всплывающим окном.
- Добавлены подсказки по EPEVER voltage sanity rules.
