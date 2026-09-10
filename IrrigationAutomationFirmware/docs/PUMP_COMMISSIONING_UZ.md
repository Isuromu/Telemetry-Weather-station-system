# Nasosni birinchi ishga tushirish

1. Himoya yeri, R/S/T, U/V/W va RS-485 ulanishlarini tekshiring.
2. Nasosga suv to'ldiring. Nasosni quruq ishlatmang.
3. Quvurlar, klapanlar va bosim sharoiti xavfsiz ekanini tekshiring.
4. Serial Monitor ni 115200 baud da oching.
5. `vfd ping` bilan aloqani tekshiring.
6. `vfd config check` bilan barcha parametrlarni o'qing. Bu buyruq hech narsani
   o'zgartirmaydi.
7. Xato bo'lmasa `pump freq 10` ni kiriting.
8. `pump start` ni qisqa muddatga yuboring va aylanish yo'nalishini tekshiring.
9. Yo'nalish noto'g'ri bo'lsa `pump stop` ni yuboring, quvvatni to'liq uzing va
   fazalarni malakali mutaxassis orqali to'g'rilang. Reverse rejimini oddiy ish
   rejimi sifatida ishlatmang.
10. Oqish, shovqin, tebranish, bosim va tokni tekshiring.
11. Chastotani 20, 30, 40 va 50 Hz ga bosqichma-bosqich oshiring.
12. Har bosqichda `pump telemetry` ni tekshiring. Motor toki 3.3 A dan oshmasin.
13. `pump stop` bilan to'xtating va `pump fault` ni tekshiring.

Birinchi aloqa sinovidan keyin P4.1.04 uchun nol bo'lmagan communication
timeout qiymatini tanlab, aloqa uzilgandagi to'xtash holatini nazorat ostida
sinang.
