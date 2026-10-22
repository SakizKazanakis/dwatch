# Monolith

Своё приложение-компаньон для часов HK8 Pro Max вместо Wearfit Pro.

Основа: [Gadgetbridge](https://codeberg.org/Freeyourgadget/Gadgetbridge), открытое приложение,
которое уже поддерживает часы семейства Wearfit (HK8 Ultra). Monolith собирается из исходников
Gadgetbridge с нашим названием, своим `applicationId` и нашими изменениями из `patches/`.

## Как собрать

Ничего ставить не нужно: GitHub Actions собирает APK на каждый push в `main`.
Готовый файл лежит во вкладке **Actions → последний запуск → Artifacts → monolith-debug-apk**.

Локально (нужны JDK 21 и Android SDK):

```bash
git clone --depth 1 https://codeberg.org/Freeyourgadget/Gadgetbridge.git upstream
scripts/apply-branding.sh upstream
cd upstream && ./gradlew assembleMainlineDebug
```

## Устройство репозитория

- `scripts/apply-branding.sh` меняет название и `applicationId`, затем накладывает патчи.
- `overlay/` наши новые файлы: экран «Пульт» (`MonolithActivity`), ресурсы, шрифты. Копируются в Gadgetbridge, `MonolithActivity` становится стартовым экраном.
- `scripts/apply-overlay.py` копирует overlay и правит манифест.
- `patches/*.patch` наши изменения в коде Gadgetbridge, по порядку имён.
- `.github/workflows/build.yml` сборка APK в облаке.

## Лицензия

Шрифты Michroma, IBM Plex Mono и IBM Plex Sans распространяются под SIL OFL 1.1, тексты лицензий в `third_party/fonts/`.


Gadgetbridge распространяется под AGPL-3.0, поэтому Monolith тоже AGPL-3.0, а исходники открыты.
