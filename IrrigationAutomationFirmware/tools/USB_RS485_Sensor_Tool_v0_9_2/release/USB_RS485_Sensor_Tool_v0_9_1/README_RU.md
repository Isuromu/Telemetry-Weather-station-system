# USB-RS485 Sensor Tool v0.9.1

Standalone Windows/Python utility для работы с RS485/Modbus датчиками и EPEVER Tracer-AN G3 через USB-to-RS485 адаптер.

## Запуск

Готовый standalone-вариант: запустите `USB_RS485_Sensor_Tool.exe` или
`START_USB_RS485_SENSOR_TOOL.bat`.

Для запуска из исходников установите Python 3 с `tkinter`, выполните
`INSTALL_REQUIREMENTS.bat`, затем `START_USB_RS485_SENSOR_TOOL.bat`.

## Основные функции

- COM-port connect/disconnect.
- Scan sensor addresses.
- Read Device по профилям.
- Change Address для поддержанных сенсоров.
- Raw Modbus с optional auto CRC.
- Logs/export.
- Профиль `TUF-2000M + TS-2 Ultrasonic Flow Meter`:
  - единая многошаговая проверка расхода, скорости, M08, M90, M91,
    внутреннего диаметра и M46;
  - полный TX/RX лог каждого Modbus RTU запроса;
  - разбор кодов ошибок и качества сигнала;
  - подтверждённый на приборе порядок REAL4 `LOW_WORD_FIRST`.
- EPEVER Config для Tracer-AN G3 / XTRA-N G3:
  - чтение battery settings;
  - запись battery type = User;
  - запись capacity Ah;
  - запись voltage block `0x9003..0x900E` одной группой;
  - запись rated voltage level `0x9067`;
  - запись max charge current `0x90BF`;
  - смена EPEVER ID через custom service command `0x45`.

## EPEVER address change

В v0.8.0 добавлена смена адреса через найденную proprietary/custom service-команду EPEVER `0x45`. Это не обычный Modbus FC06/FC10.

Команды:

```text
Чтение/поиск ID: F8 45 00 01 01 F8 + CRC
Запись ID:       F8 45 00 01 01 NN + CRC
Пример 0x60:     F8 45 00 01 01 60 88 14
```

Использовать только когда к USB-RS485 подключён один EPEVER controller. После записи программа проверяет новый и старый адрес обычным FC04-запросом battery voltage. Старую догадку `0x9020` использовать нельзя: в G3 protocol это `Turn-Off Voltage`, а не ID/address.

## Настройки из config.h

Пресеты:

- PKCELL 2x 12V9Ah parallel:
  - capacity: 18Ah;
  - max charge current: 3.60A.
- StarOne STR12120 12V12Ah:
  - capacity: 12Ah;
  - max charge current: 3.60A.

Общие напряжения:

- Over Voltage Disconnect: 15.80V
- Charging Limit: 15.00V
- Over Voltage Reconnect: 15.00V
- Equalization: 14.40V
- Boost/Bulk: 14.40V
- Float: 13.80V
- Boost Reconnect: 13.20V
- Low Voltage Reconnect: 12.60V
- Under Voltage Recover: 12.70V
- Under Voltage Warning: 12.00V
- Low Voltage Disconnect: 11.80V
- Discharging Limit: 11.30V


## Проверка TUF-2000M + TS-2

1. Выберите профиль `TUF-2000M + TS-2 Ultrasonic Flow Meter`.
2. Установите адрес `1` и нажмите `Use profile baud` — профиль использует
   `9600 8N1`.
3. Подключитесь к COM-порту и нажмите `Send read request`.
4. В `Parsed values` расход, скорость, M91 и внутренний диаметр декодируются
   как `LOW_WORD_FIRST`; это подтверждено значением `57.0 mm` из
   REG0221-REG0222.
5. В `Raw transaction / request status` отображаются полные TX/RX кадры с CRC
   для всех шести запросов.

Профиль использует подтверждённый Modbus RTU register map. Он не изменяет
настройки прибора; `LOW_WORD_FIRST` выбран по результату аппаратной проверки.
REG0093/REG0094 выводятся как raw, поскольку таблица Modbus не задаёт масштаб
их отображения M90.

## v0.8.0 заметки

EPEVER вкладка теперь разделена на настройки слева и постоянно видимый TX/RX лог справа. Ложная блокировка voltage order для config.h исправлена: 0x900A и 0x900B считаются отдельными цепочками.
