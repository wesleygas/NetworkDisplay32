# NetworkDisplay32

Display JPEG images on your TFT-display directly from the internet.

This project interfaces an ESP32 with a TFT display (currently configured to
any driver compatible with the ST7735). The pinout is available in the `src/main.cpp` file

The original usecase is for a media monitor. Because of that, the screen is divided in two sections:

1. Square 128x128 pixel JPEG image
2. Bottom 128x36 region reserved for arbitrary text

The data is sent via HTTP request:
  - POST `/upImg` receives the image data with ```'Content-Type': 'image/jpeg'```
  - GET  `/meta` with a 's' parameter containing the string to be displayed ex: ```http://node.local/?s=VeryGoodMusic```

![example_image](display_layout.jpg)
