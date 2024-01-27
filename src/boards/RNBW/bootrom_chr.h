const unsigned char bootrom_chr[4096] = {
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7E,0x81,0xA5,0x81,0xBD,0x99,0x81,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7E,0xFF,0xDB,0xFF,0xC3,0xE7,0xFF,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x6C,0xFE,0xFE,0xFE,0x7C,0x38,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x10,0x38,0x7C,0xFE,0x7C,0x38,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x38,0x7C,0x38,0xFE,0xFE,0xD6,0x10,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x03,0x0F,0x1C,0x30,0x30,0x60,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0xC0,0xF0,0x3A,0x1E,0x0E,0x1E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x1C,0x1C,0x1C,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x44,0xC6,0xC6,0xFE,0x7C,0x38,0x38,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0D,0x0D,0x00,0x0F,0xDF,0xD8,0x1B,0xDB,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xB0,0xB0,0x00,0xF0,0xFB,0x1B,0xD8,0xDB,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0F,0x1F,0x38,0x75,0x65,0x65,0x60,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFE,0xFE,0x06,0x56,0x56,0x56,0x06,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7F,0x63,0x7F,0x63,0x63,0x67,0xE6,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x99,0x5A,0x3C,0xE7,0xE7,0x3C,0x5A,0x99,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x80,0xE0,0xF8,0xFE,0xF8,0xE0,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x02,0x0E,0x3E,0xFE,0x3E,0x0E,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x18,0x3C,0x7E,0x18,0x18,0x7E,0x3C,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x66,0x66,0x66,0x66,0x66,0x00,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7F,0xDB,0xDB,0x7B,0x1B,0x1B,0x1B,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7E,0xC3,0x78,0xCC,0xCC,0x78,0x8C,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x60,0x60,0x30,0x30,0x1C,0x0F,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x0C,0x0C,0x38,0xF0,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x08,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x38,0x38,0x38,0x38,0x38,0x28,0x38,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xDB,0x1B,0xD8,0xDF,0x0F,0x00,0x0D,0x0D,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xDB,0xD8,0x1B,0xFB,0xF0,0x00,0xB0,0xB0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x60,0x60,0x60,0x60,0x60,0x7F,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x06,0x06,0x06,0x06,0x06,0xFE,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x18,0x3C,0x7E,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0xFF,0xFF,0x7E,0x3C,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x30,0x78,0x78,0x30,0x30,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x30,0x7C,0xC0,0x78,0x0C,0xF8,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x60,0x60,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x18,0x30,0x60,0x60,0x60,0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x60,0x30,0x18,0x18,0x18,0x30,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x30,0x30,0xFC,0x30,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x70,0x30,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x78,0xCC,0xDC,0xFC,0xEC,0xCC,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x30,0xF0,0x30,0x30,0x30,0x30,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x78,0xCC,0x0C,0x38,0x60,0xCC,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x78,0xCC,0x0C,0x38,0x0C,0xCC,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFC,0xC0,0xF8,0x0C,0x0C,0xCC,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x38,0x60,0xC0,0xF8,0xCC,0xCC,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFC,0xCC,0x0C,0x18,0x30,0x60,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x78,0xCC,0xCC,0x78,0xCC,0xCC,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x78,0xCC,0xCC,0x7C,0x0C,0x18,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x30,0x30,0x00,0x30,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x30,0x30,0x00,0x70,0x30,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x18,0x30,0x60,0xC0,0x60,0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xFC,0x00,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x78,0xCC,0x0C,0x18,0x30,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x30,0x78,0xCC,0xCC,0xFC,0xCC,0xCC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFC,0x6C,0x66,0x66,0x66,0x6C,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x3C,0x66,0xC0,0xC0,0xCE,0x66,0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xCC,0xCC,0xCC,0xFC,0xCC,0xCC,0xCC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x78,0x30,0x30,0x30,0x30,0x30,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xC6,0xEE,0xFE,0xD6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x38,0x6C,0xC6,0xC6,0xC6,0x6C,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x78,0xCC,0xCC,0xCC,0xDC,0x78,0x1C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFC,0x66,0x66,0x7C,0x78,0x6C,0xE6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x78,0xCC,0xE0,0x38,0x1C,0xCC,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFC,0xB4,0x30,0x30,0x30,0x30,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xCC,0xCC,0xCC,0xCC,0xCC,0x78,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xC6,0xC6,0x6C,0x38,0x6C,0xC6,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xCC,0xCC,0xCC,0x78,0x30,0x30,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFE,0xCC,0x98,0x30,0x62,0xC6,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x78,0x60,0x60,0x60,0x60,0x60,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x78,0x18,0x18,0x18,0x18,0x18,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x30,0x30,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xE0,0x60,0x7C,0x66,0x66,0x66,0xBC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x78,0xCC,0xC0,0xCC,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x1C,0x0C,0x0C,0x7C,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x78,0xCC,0xFC,0xC0,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x38,0x6C,0x60,0xF0,0x60,0x60,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x30,0x00,0x70,0x30,0x30,0x30,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x18,0x00,0x78,0x18,0x18,0x18,0xD8,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x70,0x30,0x30,0x30,0x30,0x30,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xEC,0xFE,0xD6,0xC6,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xF8,0xCC,0xCC,0xCC,0xCC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x78,0xCC,0xCC,0xCC,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xD8,0x6C,0x6C,0x60,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x7C,0xC0,0x78,0x0C,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x10,0x30,0x7C,0x30,0x30,0x34,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xCC,0xCC,0xCC,0x78,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xC6,0xC6,0xD6,0xFE,0x6C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xCC,0xCC,0xCC,0x7C,0x0C,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xFC,0x98,0x30,0x64,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x1C,0x30,0x30,0xE0,0x30,0x30,0x1C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xE0,0x30,0x30,0x1C,0x30,0x30,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x10,0x38,0x6C,0xC6,0xC6,0xC6,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x03,0x0F,0x1E,0x3E,0x3F,0x7E,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0xC0,0xF0,0x78,0x7C,0xFC,0x7E,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x07,0x1F,0x3F,0x78,0x60,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xE0,0xF8,0xFC,0x1E,0x06,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x20,0x70,0xF8,0x7C,0x3F,0x1F,0x0F,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x08,0x1C,0x3E,0x7C,0xF8,0xF0,0xE0,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x0C,0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x03,0x05,0x0E,0x1C,0x38,0x70,0xE0,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x51,0x71,0x71,0x71,0x7F,0x71,0x7F,0x71,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFA,0xFE,0xFE,0xFE,0xFE,0xFE,0xFE,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x3F,0x07,0x0F,0x1D,0x39,0x31,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x23,0x27,0x2E,0x3C,0x38,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xC0,0x47,0x47,0xE7,0x00,0x00,0xE0,0x27,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0xFF,0xFF,0xFF,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x01,0x02,0x02,0x04,0x04,0x04,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x70,0x8C,0x02,0x32,0x81,0x01,0x01,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7E,0x7E,0x3E,0x3E,0x1F,0x0F,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7E,0x7E,0x7C,0x7C,0xF8,0xF0,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x83,0x87,0x0C,0x08,0x00,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xC1,0xE1,0x30,0x10,0x00,0x80,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0F,0x1F,0x3F,0x7C,0xF8,0x70,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xE0,0xF0,0xF8,0x7C,0x3E,0x1C,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x27,0x53,0xA9,0x54,0x2A,0x15,0x0A,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x80,0xC0,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7F,0x71,0x7F,0x31,0x3F,0x31,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFE,0xFE,0xFE,0xFC,0xFC,0xFC,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x44,0xEE,0x7C,0x38,0x7C,0xEE,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x08,0x14,0x22,0x14,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x47,0xE7,0x00,0x00,0xE0,0x47,0x27,0xE7,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFF,0xFF,0x00,0x00,0x00,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x02,0x05,0x0E,0x1C,0x38,0x70,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x02,0x8C,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x3C,0x7C,0xFF,0xFF,0x80,0x80,0x1F,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xF8,0xF8,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x3C,0xFC,0xFF,0xFF,0xFF,0x00,0xFF,0xFF,0x40,0x82,0x00,0x00,0xFF,0x00,0x00,0x00,
0x00,0x00,0xFF,0xFF,0xFF,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,
0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x30,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xC0,0x60,0x70,0x78,0x3C,0x04,0xFC,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x3F,0x3F,0x3F,0x3F,0x3F,0x3E,0x3E,0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xC0,0xE0,0xF0,0xF8,0xFC,0x7C,0x7C,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x01,0x01,0x03,0x02,0x06,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x80,0x80,0xC0,0x40,0x60,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x3F,0x7E,0x7E,0x7E,0x7E,0x7E,0x7E,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFC,0x7E,0x7E,0x7E,0x7E,0x7E,0x7E,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x3F,0x7C,0x79,0x7F,0x7F,0x7E,0x7E,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFC,0x3E,0x9E,0x9E,0x3E,0x7E,0x7E,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x07,0x04,0x3F,0x7F,0x00,0x35,0x35,0x35,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xF0,0x10,0xFE,0xFF,0x00,0x56,0x56,0x56,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x1F,0x1F,0x3F,0x3F,0x3F,0x3F,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFE,0xFE,0xFE,0xFC,0xFC,0xFC,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x30,0x3F,0x30,0x3F,0x30,0x3F,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0C,0xFC,0x0C,0xFC,0x0C,0xFC,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x30,0x30,0x3E,0x3E,0x3E,0x3F,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0C,0x0C,0x7C,0x7C,0x7C,0xFC,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0E,0x0E,0x1E,0x1F,0x3E,0x3E,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x70,0x70,0x78,0xF8,0x7C,0x7C,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7E,0x7E,0x7F,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7E,0x7E,0xFE,0xFC,0xF8,0x38,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7E,0x7E,0x7F,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7E,0x7E,0xFE,0xFC,0xF8,0x38,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x35,0x35,0x35,0x35,0x35,0x35,0x3F,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x56,0x56,0x56,0x56,0x56,0x56,0xFE,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x0C,0x1E,0x3E,0x38,0x70,0x60,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x01,0x07,0x08,0x10,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0xE0,0xF8,0x3C,0x1E,0x0E,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xFF,0xFF,0xFE,0xFC,0xF8,0xF1,0xE3,0xE7,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x08,0x1E,0x33,0x73,0xFE,0xFC,0xF8,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x1E,0x18,0x18,0x18,0x18,0x18,0x18,0x1E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x01,0x01,0x00,0x00,0x1A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x03,0x07,0x2E,0xFC,0xF8,0xFC,0x78,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x18,0x18,0x1C,0x0F,0x0F,0x1C,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x07,0x0F,0x1C,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0xE0,0xF0,0x38,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x3F,0x3F,0x3F,0x3F,0x3E,0x3E,0x3F,0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xC0,0xE0,0xF0,0xF8,0x7C,0x7C,0xFC,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x60,0x20,0x20,0x10,0x08,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xE7,0xE1,0xC5,0xC8,0xC0,0xE3,0xFF,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xE0,0xC0,0x80,0x10,0x30,0xF0,0xF0,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x1C,0x3E,0x1F,0x1F,0x3F,0x72,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x18,0x80,0x00,0x80,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x18,0x18,0x38,0xF0,0xF0,0x38,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x18,0x18,0x1C,0x0F,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x18,0x18,0x38,0xF0,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x3E,0x3E,0x3E,0x3E,0x3E,0x3F,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x7C,0x7C,0x7C,0x7C,0x7C,0xFC,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x18,0x17,0x16,0x36,0x36,0x30,0x1F,0x1E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x79,0xB6,0x32,0xBA,0x32,0x78,0xFF,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x02,0x82,0x84,0x84,0x88,0x88,0x80,0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x11,0x9C,0x1E,0x0F,0x1F,0x5F,0x3F,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0xC0,0xE4,0xF0,0xFC,0xFE,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x40,0x40,0x20,0x20,0x20,0x20,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x01,0x03,0x03,0x07,0xFF,0xFF,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x80,0x80,0xC0,0xFE,0xFE,0xFC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0F,0x18,0x30,0x67,0xCC,0x98,0x91,0x93,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xE0,0x30,0x18,0xCC,0x66,0x32,0x12,0x92,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x07,0x1F,0x3E,0x77,0x6F,0xFE,0xDC,0xD0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xE0,0xF8,0x7C,0xBE,0xBE,0x17,0x07,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x01,0x03,0x07,0x0F,0x1E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x20,0x70,0xF0,0xE0,0xC0,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x04,0x0E,0x0F,0x07,0x03,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x80,0xC0,0xE0,0xF0,0x78,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x0E,0x67,0xF3,0x50,0x02,0x7C,0xFE,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xE7,0x06,0xF8,0x00,0x04,0x03,0x07,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x2C,0x2C,0x38,0x00,0x00,0xE0,0xF0,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x10,0x10,0x00,0x7F,0x9F,0x97,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x08,0x08,0x00,0xFE,0xFF,0xFF,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x02,0x02,0x04,0x04,0x04,0x04,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x3F,0x1F,0x1F,0x1F,0x3F,0x3F,0x78,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xF8,0xF0,0xF0,0xF0,0xF8,0xF8,0x3C,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x91,0x00,0x03,0x07,0x07,0x03,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x12,0x00,0x80,0xC0,0xC0,0x80,0x80,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0xC8,0xC4,0xE6,0x67,0x73,0x3E,0x1F,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x1F,0x1F,0x07,0x86,0x8E,0x3C,0xF8,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x1E,0x0F,0x07,0x03,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x80,0xC0,0xE0,0xF0,0x70,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x01,0x03,0x07,0x0F,0x0E,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x78,0xF0,0xE0,0xC0,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};