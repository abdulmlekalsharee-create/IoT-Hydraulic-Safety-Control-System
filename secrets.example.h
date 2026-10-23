// secrets.example.h
// 1) Copy this file to "secrets.h" (same folder as the .ino sketch).
// 2) Replace the placeholders with your own values.
// 3) NEVER commit or upload secrets.h  (it is listed in .gitignore).
#pragma once

// Wi-Fi (2.4 GHz network or phone hotspot)
#define WIFI_SSID            "YOUR_WIFI_SSID"
#define WIFI_PASSWORD        "YOUR_WIFI_PASSWORD"

// ThingSpeak: channel -> API Keys -> Write API Key
#define THINGSPEAK_API_KEY   "YOUR_THINGSPEAK_WRITE_API_KEY"

// SMTP e-mail alerts (Gmail: use an App Password, not your account password)
#define SMTP_AUTHOR_EMAIL     "sender@example.com"
#define SMTP_AUTHOR_PASSWORD  "YOUR_APP_PASSWORD"
#define SMTP_RECIPIENT_EMAIL  "recipient@example.com"
