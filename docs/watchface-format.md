# Циферблаты HK8 Pro Max: что делает Wearfit Pro

Разбор Wearfit Pro `com.wakeup.howear` версии hw_5.6.17 (база + `config.arm64_v8a.apk`).
Часы HK8 Pro Max построены на SiFli (в коде приложения SiFli называется «Siche», 思澈),
поэтому все загрузки на них идут через SDK SiFli, а не через старый протокол Wearfit.

## Как картинки и циферблаты попадают на часы

Любая загрузка на часы SiFli выглядит одинаково:

1. Приложение собирает обычный `.zip`. Пути внутри архива становятся путями в файловой системе часов.
2. `SifliWatchfaceService.startActionWatchface(context, zipPath, mac, type)` распаковывает архив,
   сортирует файлы и шлёт их по BLE-протоколу SiFli (`com.sifli.watchfacelibrary` + `serialTransService`).
3. Для `type == 3` и `4` каждый файл выравнивается и получает CRC перед отправкой.

Типы (`SFWatchfaceType`): `0` FACE (циферблат из магазина), `1` языки, `2` BG_IMAGE,
`3` CUSTOM (произвольные файлы), `4` музыка, `5` JS, `6` EQ, `7` превью-видео.

Что приложение кладёт в архив:

| Что | Тип | Путь внутри zip | Содержимое |
|---|---|---|---|
| Свой фон (кастомный циферблат) | 3 | `dyn/resource/clock_bg.bin` и `clock_bg_thum.bin` | eZIP rgb888 |
| DIY-циферблат | 3 | `dyn/resource/clock_diy_bg.bin` | eZIP rgb888a |
| AI-фон | 3 | `dyn/resource/ai_clock_crop_bg.bin` | eZIP rgb888 |
| Видео-циферблат | 3 | `dyn/video/video_clock.h264` + `dyn/resource/video_clock_thum.bin` | H.264 + eZIP превью |
| Галерея | 3 | `dyn/album_photo/*.bin` | eZIP rgb565 |
| Циферблат из магазина | 0 | архив с сервера как есть | формат не известен, нужен образец |

Старые прошивки используют `ex/resource/` вместо `dyn/resource/`. `dyn/` включается для типов
устройств 5, 8, 13 (`SicheUtils.isNewPath`). После загрузки фона приложение шлёт команду стиля
(`TransferOrder.sendStyle` / `sendStyleNew` / `sendStyleVideo`), которая говорит часам, где и
каким цветом рисовать время поверх фона.

## Формат картинок eZIP

Картинки кодирует `libezip.so` (`sifliEzipUtil.pngToEzip(png, "rgb888", 0, 1, boardType)`).
Тот же формат делает официальный конвертер из SiFli SDK, у которого есть сборка под Linux:
`tools/png2ezip/ezip_linux` в [OpenSiFli/SiFli-SDK](https://github.com/OpenSiFli/SiFli-SDK).

Файл = 4 байта заголовка LVGL (little-endian) + поток eZIP:

| биты | 31..21 | 20..10 | 9..5 | 4..0 |
|---|---|---|---|---|
| поле | высота | ширина | резерв | формат: 1 eZIP, 2 eZIP+альфа, 4 пиксели, 5 пиксели+альфа |

`scripts/make-clock-bg.sh` собирает такой файл из PNG. Проверено: фон Monolith 485×520
кодируется в `clock_bg.bin` (159 КБ, формат 1) и обратно декодируется без потерь.

`boardType` выбирается по типу устройства (`SicheUtils.getBoardType`): 4 и 8 → 1, 5 → 2, 13 → 3,
остальные → 0.

## Компилируемый формат XML → .bin

В приложении есть второй, полностью описанный формат циферблата: `com.compress.utils.DialConvertTool`
собирает `.bin` из XML и PNG. Пример исходника лежит в APK (`assets/watchFace/watchFace.xml`).
Картинки в нём жмёт другой кодек (`libnativelib.so`, «ICE V5R1 HTC»/HFBC), и этот путь приложение
использует для видео-циферблатов, поэтому для SiFli он вторичен. Раскладка на всякий случай:

Заголовок, little-endian, `204 + N` байт (N — число элементов):

```
char  name[16]          // title + время сборки
u16   width, height     // <= 960
char  version[16]
u16   preview_img_id
char  description[128]
u16   0
u32   img_offset        // смещение таблицы картинок
u8    view_count
u8    display_capability // 0 normal, 1 aod, 2 оба
u32   ambient_period, normal_period
u32   uuid
u8    watch_type
char  watch_version[16]
u8    0
u8    view_type[N]      // 0 static, 1 sequence, 2 rotate, 3 option, 4 digital, 5 label,
                        // 6 arc_label, 7 box_progress, 8 arc_progress, 9 video, 10 click, 11 kaleidoscope
```

Дальше идут элементы подряд, потом таблица картинок: `u32 dir_id = 0x0FEF`, `u32 count`,
`count × {u32 offset, u32 size}` и сжатые данные. Id картинок начинаются с 1.

- `static_img` (18 байт): pos x,y; center x,y; f32 rotate; u8 count; u8 display_mode; u16 language; u16 img_id
- `digital_img` (38 байт): u8 align|int_len<<4; u8 decimals; u16 img_ids[10]; u16 sign; u16 dot; i16 space; align_pos x,y; u16 bind_data; u8 periodic; u8 display_mode; u8 0
- `option_img` (12 + 2n): pos; u16 bind_data; u8 periodic; u8 count; u8 0; u16 language; u8 display_mode; u16 img_ids[n]
- `sequence_img` (14 + 2n): pos; u16 interval_ms; u8 repeat; u8 1; u8 0,0; u8 n; u8 0,0; u8 display_mode; u16 img_ids[n]
- `rotate_img` (20): pos; center; u16 start, end; u16 img_id; u16 bind_data; u8 periodic; u8 display_mode; u8 ambient_periodic
- `label` (148): u8 break_mode, align, font_size, periodic; rect; u32 color; char font[64]; char text[64]; u16 bind_data; u8 display_mode; u8 ambient_periodic

`display_mode`: 0 normal, 1 ambient, 2 always. Значения `bind_data` из примера: 1 часы, 6 минуты,
70 день, 80 месяц, 83 день недели.

## Магазин циферблатов

API: `https://howear.iwhop.com/howearapi/`. Каталог (`auth/dialMarketScreen/*`) требует токен
пользователя: без него сервер отвечает 403 «not found token». Запросы идут по `code` (код модели
часов, `bandVersionCode`, приходит от часов при подключении), `bluetoothName`, `mac`, `majorVersion`.
Ссылку на архив отдаёт `auth/dialMarketScreen/dialCheck`. Установка: `DialUtil.goInstallRttDial`,
тип 0.

## Что дальше

1. Перенести протокол загрузки SiFli в Monolith (Gadgetbridge): BLE-сервис SiFli, команды
   start/file start/data/end, выравнивание и CRC32-MPEG2.
2. Первый рабочий циферблат: залить `dyn/resource/clock_bg.bin` (тип 3) и отправить команду стиля.
   Часы сами рисуют время поверх нашего фона.
3. Анимация пульса: видео-циферблат (`dyn/video/video_clock.h264`), без своего движка.
4. Полностью свой макет (пульс, координаты, температура) потребует формат циферблата из магазина.
   Для этого нужен хотя бы один архив из магазина для HK8 Pro Max.
