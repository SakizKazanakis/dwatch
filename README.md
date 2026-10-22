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
- `patches/*.patch` наши изменения поверх Gadgetbridge, по порядку имён.
- `.github/workflows/build.yml` сборка APK в облаке.
- `docs/watchface-format.md` как Wearfit Pro грузит циферблаты и картинки на часы SiFli.
- `scripts/make-clock-bg.sh` PNG → eZIP `.bin`, формат картинок часов.
- `dial/monolith/` фон Monolith: исходный PNG, `clock_bg.bin` и готовый `clock_bg.zip` для загрузки.

## Лицензия

Gadgetbridge распространяется под AGPL-3.0, поэтому Monolith тоже AGPL-3.0, а исходники открыты.
