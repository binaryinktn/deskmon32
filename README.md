# DeskMon32

DeskMon32 is an open-source PC system statistics monitor for small TFT displays. The project runs on microcontroller boards (PlatformIO) and displays CPU, GPU, memory and disk statistics reported from a PC host. It uses the Aroma UI and TFT_eSPI libraries for UI and display drivers.

**Preview:**

![Project Image](images/picture.jpg)

**PlatformIO Quick Start**
- **Requirements:** Install the PlatformIO VS Code extension or the PlatformIO Core CLI.
- **Build:**

	`pio run`

- **Upload / Flash:**

	`pio run -t upload -e <env>`

- **Open serial monitor:**

	`pio device monitor -b 115200`

- **Notes:** Replace `<env>` with the environment name for your board in `platformio.ini`. Checkout DeskMon32_Service for the windows service that sends Serial data to the board.


**Contributing & License**
- This repository is open-source. See the repository `LICENSE` file or individual library folders for licenses. Contributions and issues are welcome.



