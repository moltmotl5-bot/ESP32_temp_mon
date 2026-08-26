# Temperature Dashboard (GitHub Pages)

Cloud dashboard for the ESP32 temp monitor. Data comes from **Firebase Realtime Database**; ESP32 pushes readings when online.

## One-time setup

### 1. Firebase Web config

1. Firebase Console → Project settings → Your apps → Web app
2. Copy `firebaseConfig` values into `firebase-config.js` (or copy from `firebase-config.example.js`)

### 2. Authorized domain

Firebase Console → Authentication → Settings → Authorized domains → add:

`YOUR-GITHUB-USERNAME.github.io`

### 3. Enable GitHub Pages

Repo **Settings → Pages**:

- Source: **Deploy from a branch**
- Branch: `main` (or your default) → folder **`/docs`**
- Save

Site URL: `https://YOUR-GITHUB-USERNAME.github.io/ESP32_temp_mon/`

## Sign in

Use the Email/Password user you created in Firebase Authentication.

## Device path

Reads: `devices/esp32-s3-kls/meta` and `devices/esp32-s3-kls/readings`

Change `DEVICE_ID` in `firebase-config.js` if you use another device id.
