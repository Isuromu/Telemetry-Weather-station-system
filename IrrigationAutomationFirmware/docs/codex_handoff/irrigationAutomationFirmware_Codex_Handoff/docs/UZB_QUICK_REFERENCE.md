# Irrigation Controller — Qisqa Texnik Ma'lumot

## Asosiy nomlar

- `PressureControlValve` — bosimni boshqaruvchi/pasaytiruvchi klapan.
- `PressureControlValveSolenoid` — 12 V latching solenoid.
- `UpstreamPressureSensor` — klapandan oldingi bosim sensori.
- `DownstreamPressureSensor` — klapandan keyingi bosim sensori.
- `PumpVfd` — nasos chastota o'zgartirgichi.
- `MotorizedValveActuator` — alohida RS485 motorli klapan aktuatori.

## Latching solenoid ishlashi

Solenoid doimiy tok bilan ishlamaydi.

- OPEN uchun bir qutblanishda qisqa impuls beriladi.
- CLOSE uchun teskari qutblanishda qisqa impuls beriladi.
- Impulsdan keyin L298N chiqishlari LOW qilinadi.
- Keyin L298N quvvati GPIO27 orqali o'chiriladi.

## Hozirgi pinlar

- Battery ADC: GPIO35
- PCV IN1: GPIO16
- PCV IN2: GPIO17
- PCV power: GPIO27
- I2C-1 SDA/SCL: GPIO21/GPIO22
- I2C-2 SDA/SCL: GPIO13/GPIO4

## Keyingi plata uchun

Ikki bir xil I2C adresli bosim sensorini bitta I2C shinasiga ulash uchun I2C multiplexer ishlatish tavsiya qilinadi.

Shunda:
- GPIO13 -> kelajakdagi RS485 TX
- GPIO34 -> kelajakdagi RS485 RX
- GPIO4 -> kerak bo'lsa DE/RE

## Muhim

ESP32 qayta ishga tushgandan keyin latching klapanning haqiqiy holati avtomatik ravishda ma'lum emas. Dastur `Unknown` holatini qo'llashi kerak.
