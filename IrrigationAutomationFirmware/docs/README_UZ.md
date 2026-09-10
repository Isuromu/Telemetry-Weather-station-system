# Qisqa foydalanish qo'llanmasi

Ushbu loyiha ESP32 DevKitC V4 orqali DELIXI CDI-E100 chastota o'zgartirgichini
RS-485 Modbus RTU bilan boshqaradi. Ulangan nasos: Grandfar 2CP50/160B.

## Asosiy ulanishlar

| Vazifa | ESP32 |
|---|---:|
| RS-485 RX | GPIO16 |
| RS-485 TX | GPIO17 |
| Debug Serial | 115200 baud |
| Zaxira quvvat boshqaruvi | GPIO2, bu misolda ishlatilmaydi |

RS-485 A+ ni DELIXI SG+ ga, B- ni SG- ga ulang. Hozirgi modul yo'nalishni
avtomatik o'zgartiradi, DE/RE pini kerak emas.

## Birinchi buyruqlar

```text
help
vfd ping
vfd info
vfd config check
pump status
pump freq 10
pump start
pump telemetry
pump stop
```

Dastur ESP32 qayta ishga tushganda nasosni o'zi yoqmaydi. Har bir qayta
ishga tushirishdan keyin `pump start` dan oldin chastotani `pump freq` bilan
qayta kiriting.

Ruxsat etilgan dasturiy oraliq 10..50 Hz. 10 Hz ishlab chiqaruvchi tasdiqlagan
uzluksiz ish nuqtasi degani emas. Sinov natijasiga qarab minimal uzluksiz
chastota yuqoriroq bo'lishi mumkin.

To'liq buyruqlar ro'yxati ingliz tilidagi `SERIAL_COMMANDS.md` faylida.

