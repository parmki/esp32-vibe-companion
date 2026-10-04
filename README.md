# 👾 ESP32 Vibe Companion

An autonomous, expressive cyber-companion running on an **ESP-WROOM-32**. Features smooth procedural vector eye animations, autonomous mood shifts every 5 minutes, and a free two-way Telegram bot interface with zero recurring API costs.

---

## ✨ Features

- **Procedural Vector Eyes:** Rendered dynamically using math (LERP smoothing) instead of clunky bitmaps—achieves 40+ FPS with organic blinking and smooth transitions.
- **Autonomous Mood Cycles:** Automatically changes emotional states (Neutral, Happy, Angry, Sleepy, Smug) every 5 minutes when idle.
- **Free Telegram Bot Integration:** Chat with your companion anywhere over Wi-Fi via Telegram (`@BotFather`).
- **Keyword Reaction Engine:** Detects key phrases in incoming messages to trigger specific expressions and curated replies, with a fallback pool for unmatched queries.
- **Zero API Fees:** Runs 100% locally on the microcontroller; no paid LLM subscriptions or token rate limits required.

---

## 🛠️ Hardware Requirements

| Component | Description |
| :--- | :--- |
| **Microcontroller** | ESP-WROOM-32 (NodeMCU / DevKit V1) |
| **Display** | 0.96" or 1.3" I2C OLED (SSD1306 / SH1106, 128x64) |
| **Wiring** | 4-pin female-to-female jumper wires |
| **Power** | Micro-USB cable |

### Pinout (I2C Default)

| ESP-WROOM-32 Pin | OLED Pin |
| :--- | :--- |
| `3V3` | `VCC` |
| `GND` | `GND` |
| `GPIO 22` | `SCL` |
| `GPIO 21` | `SDA` |

---

## 📦 Project Structure

```text
esp32-vibe-companion/
├── .gitignore
├── LICENSE
├── README.md
├── esp32-vibe-companion.ino     # Main loop, animations, and Telegram handler
├── config.h.example             # Template for credentials
└── config.h                     # Secret credentials (ignored by git)
