# Project README

## Project Overview

This project involves implementing a Telegram bot on the NodeMCU board to control a system that manages the watering and light system for a plant. The bot connects to Wi-Fi and allows users to perform various actions such as opening and closing the valve, toggling the light, setting schedules for automating watering and light control, and retrieving the system status.

## Requirements to Run the Project

### Hardware Requirements
- NodeMCU board
- One 200 ohm resistor
- Two 1 Mohm resistors
- Battery: Two 1.5V alkaline batteries
- One photoresistor
- One LED
- Valve with a DC motor
- Breadboard and jumper wires

### Software Requirements
- Arduino IDE
- File configuration for the NodeMCU board
- Required libraries:
  - `ESP8266WiFi.h`
  - `WiFiClientSecure.h`
  - `UniversalTelegramBot.h`
  - `EEPROM.h`

## Project Layout

The project consists of a single `.ino` file containing the entire codebase.

## How to Build, Burn, and Run the Project

1. Install Arduino IDE.
2. Connect the NodeMCU board to your computer.
3. Open the project file in Arduino IDE.
4. Verify and compile the code.
5. Upload the code to the NodeMCU board.

## User Guide

To interact with the Telegram bot, users can send commands to perform various actions:
- `/status`: Get system status
- `/water_now`: Trigger immediate watering
- `/stop_watering`: Stop watering
- `/set_schedule`: Set watering schedule
- `/cancel_schedule`: Cancel watering schedule
- `/lights_on`: Turn lights ON
- `/lights_off`: Turn lights OFF
- `/set_light_schedule`: Set light schedule
- `/get_config`: Get current configuration
- `/help` or `/commands`: Show available commands

## Links

- [PowerPoint Presentation](link_to_ppt)
- [YouTube Video](link_to_video)

## Team Members

- **Edoardo Nicolodi**: Sole contributor to the project. Implemented the entire solution using the NodeMCU board and provided documentation.

