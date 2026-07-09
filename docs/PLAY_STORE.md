# Google Play Store — publication guide

Step-by-step instructions for publishing **Music for Reading**.

## 1. Developer account

1. Go to [Google Play Console](https://play.google.com/console)
2. Pay the one-time **$25** registration fee
3. Complete identity verification if prompted

## 2. Prepare release build

### Create signing key

```bash
keytool -genkey -v -keystore release.keystore -alias reading-music \
  -keyalg RSA -keysize 2048 -validity 10000
```

Store the keystore and passwords securely. Loss of the key prevents updating the app.

### Configure Gradle signing

Create `keystore.properties` in the project root:

```properties
storeFile=release.keystore
storePassword=YOUR_STORE_PASSWORD
keyAlias=reading-music
keyPassword=YOUR_KEY_PASSWORD
```

Add to `app/build.gradle.kts` inside `android { }`:

```kotlin
val keystorePropertiesFile = rootProject.file("keystore.properties")
val keystoreProperties = java.util.Properties()
if (keystorePropertiesFile.exists()) {
    keystoreProperties.load(keystorePropertiesFile.inputStream())
}

signingConfigs {
    create("release") {
        storeFile = file(keystoreProperties["storeFile"] as String)
        storePassword = keystoreProperties["storePassword"] as String
        keyAlias = keystoreProperties["keyAlias"] as String
        keyPassword = keystoreProperties["keyPassword"] as String
    }
}

buildTypes {
    release {
        signingConfig = signingConfigs.getByName("release")
        // ... existing release config
    }
}
```

### Build AAB

```bash
./gradlew bundleRelease
```

Upload file: `app/build/outputs/bundle/release/app-release.aab`

## 3. Create app in Play Console

1. **All apps → Create app**
2. App name: `Music for Reading`
3. Default language: English (United States) or Russian
4. App / Game: App
5. Free / Paid: Free

## 4. Store listing

| Field | Suggested content |
|-------|-------------------|
| **Short description** (80 chars) | Offline instrumental music for reading. No words, no distractions. |
| **Full description** | See below |
| **App icon** | 512×512 PNG (export from `ic_launcher_foreground`) |
| **Feature graphic** | 1024×500 PNG |
| **Phone screenshots** | Min 2, recommended 4–8 |
| **Category** | Music & Audio |

### Full description (EN)

```
Music for Reading generates calm instrumental ambient soundscapes designed for focused reading.

• Fully offline — no internet required
• No vocals, no drums — nothing to distract you
• Six styles: Deep Ambient, Soft Piano, Rain & Pad, Zen Garden, Lo-fi Haze, Night Forest
• Infinite procedural music — never repeats on a short loop
• Sleep timer: 15, 30, 60, or 90 minutes
• Works in the background with screen off

Perfect for books, study, and quiet focus.
```

### Full description (RU)

```
Музыка для чтения — спокойные инструментальные саундскейпы для сосредоточенного чтения.

• Полностью офлайн — интернет не нужен
• Без слов и ударных — ничего не отвлекает
• 6 стилей: Глубокий эмбиент, Мягкое пианино, Дождь и пэд, Дзен-сад, Lo-fi туман, Ночной лес
• Бесконечная процедурная музыка
• Таймер сна: 15, 30, 60 или 90 минут
• Работает в фоне при выключенном экране
```

## 5. Privacy policy

Google requires a privacy policy URL even if no data is collected.

1. Host `docs/PRIVACY_POLICY.md` on GitHub Pages, Notion, or your website
2. Enter the URL in **App content → Privacy policy**

## 6. App content declarations

### Data safety

- **Does your app collect or share user data?** → No
- All categories: **No data collected**

### Ads

- **Contains ads?** → No

### Content rating

Complete the IARC questionnaire:

- Violence: No
- Sexuality: No
- Language: No
- Controlled substances: No
- User interaction / sharing: No
- Expected rating: **Everyone (3+)**

### Target audience

- Target age: 13+ or All ages (no child-directed content)
- Not designed primarily for children

## 7. Release

1. **Testing → Internal testing** (recommended first)
   - Create release, upload AAB
   - Add tester emails
   - Verify install and playback on a real device

2. **Production**
   - Create new release
   - Upload the same or newer AAB
   - Add release notes
   - Submit for review

Review typically takes **1–3 business days**.

## 8. Checklist before submit

- [ ] `targetSdk = 35`
- [ ] `versionCode` incremented for each upload
- [ ] No `INTERNET` permission in manifest
- [ ] Foreground service type `mediaPlayback` declared
- [ ] Privacy policy URL live
- [ ] Store listing complete (icon, screenshots, descriptions)
- [ ] Data safety form submitted
- [ ] Content rating received
- [ ] Tested on Android 8+ device with headphones and speaker

## 9. After launch

- Monitor reviews and crash reports in Play Console
- For each update: increment `versionCode`, build new AAB, add release notes
