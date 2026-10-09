# Xavfsizlik

Bu tizim 380 V uch fazali elektr uskunani boshqaradi. O'rnatish va xizmat
ko'rsatishni faqat malakali mutaxassis bajarishi kerak.

- Elektr qismlariga tegishdan oldin kirish quvvatini uzing va bloklang.
- DELIXI qo'llanmasidagi kutish vaqtiga rioya qiling va kuchlanish yo'qligini
  o'lchab tekshiring.
- Tarmoqni faqat R/S/T ga ulang. R/S/T ni U/V/W ga ulash mumkin emas.
- Motor U/V/W va himoya yeriga ulanadi.
- Chastota o'zgartirgich ishlayotgan paytda motor simlarini uzmang yoki
  almashtirmang.
- Boshqaruv va RS-485 simlarini kuch va motor kabellaridan uzoq o'tkazing.
- Nasosni suvsiz ishlatmang.
- 50 Hz va motorning 3.3 A nominal tokidan oshmang.
- Hozirgi RS-485 modul galvanik izolyatsiyasiz. Doimiy o'rnatish uchun
  izolyatsiyalangan RS-485 interfeysi tavsiya etiladi.

`pump estop` faqat RS-485 orqali free stop buyrug'ini yuboradi. Bu jismoniy
favqulodda to'xtatish zanjirining o'rnini bosmaydi. Xavf tahliliga mos alohida
favqulodda to'xtatish va elektr ajratgich bo'lishi kerak.

## `estop` (free stop) — bu yaxshiroq to'xtatish emas, balki zararli to'xtatish

Ikki xil to'xtatish bor, farqi muhim:

| buyruq | DELIXI yozuvi | xatti-harakat |
|---|---|---|
| `pump stop` (fPort 50, op 01) | `0x0006` deceleration stop | drayv motorni o'z decel vaqti bo'yicha sekin-asta to'xtatadi |
| `pump estop` (fPort 50, op 02) | `0x0005` free stop | drayv chiqishini uzadi; motor va impeller o'z inersiyasi bilan erkin aylanib to'xtaydi |

- Free stop oqim tezligini boshqarishning yagona vositasini olib tashlaydi.
  Statik bosimga qarshi suv ustuni o'zi sekinlashadi va impeller to'xtaguncha
  teskari oqimga o'tishi mumkin — teskari oqimga qarshi qaytmas klapan yopiladi
  va quvur, klapan hamda korpusda bosim sakrashi (gidroudar) bo'ladi.
  Deceleration stop esa oqimni boshqarib sekinlashtiradi.
- Takroriy free stop qaytmas klapanga zo'r beradi va uzun tushishda nasos
  kavitatsiyaga uchrashi mumkin. Free stopni muntazam to'xtatish emas, balki
  tekshirishni talab qiladigan hodisa deb hisoblang.
- Xavfli tomoni regeneratsiya emas: erkin aylanishda tormoz momenti yo'q, shuning
  uchun VFD DC shinasiga energiya qaytmaydi. DC shinasiga qaytish **tez
  deceleration** stopda bo'ladi va drayvni overvoltage xatosi bilan uzdırishi
  mumkin — bu drayvning decel vaqt sozlamasi, bu buyruqqa aloqasi yo'q.
- Shuningdek bu LoRaWAN orqali ketadigan dasturiy buyruq, jismoniy favqulodda
  to'xtatish zanjiri emas — shuning uchun u ham boshqa downlink kabi yo'qolishi
  mumkin.

**Qoida.** Javobsiz qolgan pump stop uchun ketma-ketlik shunday bo'lishi kerak:
nasos hali xabar berib turganda **deceleration stop** ni qayta yuborish, va
free stopga faqat u ham javobsiz qolganda o'tish. Bitta yo'qolgan downlink
nasos boshqaruvdan chiqdi degani emas. Eskalatsiyadan qochish uchun
`pumpStopTimeoutSec` ni oshirmang — yetishmayotgani takroriy yuborish, uzoqroq
kutish emas.

2026-10-09 da kuzatildi: bitta yo'qolgan `stop` downlink sequencerni 150 s
deadline'da to'g'ridan-to'g'ri free stopga olib bordi. Nasos butun vaqt xabar
berib turgan edi (o'sha oraliqda 11 uplink), ya'ni free stop sog'lom nasosga
faqat bitta freym yetib bormagani uchun berilgan.
