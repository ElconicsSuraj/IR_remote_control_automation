# Havells Lloyd AC IR Controller

This project allows controlling a Havells Lloyd Air Conditioner using an IR transmitter module.  
The system can:

- Power ON the AC
- Power OFF the AC
- Set temperature from **16°C to 30°C**

---

# Features

✅ IR based AC control  
✅ Temperature control from 16°C to 30°C  
✅ Power ON/OFF support  
✅ Compatible with Havells Lloyd AC remote protocol  
✅ Easy integration with ESP8266 / ESP32 / Arduino  

---

# Hardware Required

- ESP8266 / ESP32 / Arduino
- IR LED Transmitter
- Resistor (100Ω–220Ω recommended)
- Jumper wires
- Havells Lloyd AC

---

# Connections

| IR Transmitter | Controller Pin |
|----------------|----------------|
| VCC            | 3.3V / 5V      |
| GND            | GND            |
| SIGNAL         | GPIO Pin       |

Example:

```cpp
#define IR_LED_PIN 4
