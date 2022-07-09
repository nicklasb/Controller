
# Controller

This is a central unit for managing and collecting sensor data from [peripherals](https://github.com/nicklasb/Peripheral) using:  
* LGVL-driven touch TFT for interaction
* Competent multi-core usage for maximal performance and stability
* BLE for common, always-on situations 
* GSM SMS reporting and control
* GPS for positioning
* Wake on GPIO, then BLE for low-power situations and alarms.   
*Like intrusion (theft, camera), danger (fire, leaks)*

The first evolution is hard-coded to a specific setup, the next will attempt to be more dynamic. 

# Progress
## Currently implemented:
* A basic UI implementation with touch
* BLE client+server, based on an unholy mix of the esp-idf client/server demos
* Create a project for peripherals that work in unison with the controller 
* Create a custom BLE service that just does what is needed
* A framework for a queue-based work item handling
* Runtime monitor for statistics (historical memory tracking mostly, should be possible to add warnings for leaks and other sus stuff)
* Make data from peripherals appear on the controller screen 
* Add some checks to the monitor, like memory issues

## WIP:

* Reporting: Create simple alarm and reporting handling so the controller can react, raise alarms and other stuff


## Upcoming:

* Communication: Implement proper protocol for communicating structured sensor data
* Security: Make it so that the central only discovers peripherals that are a part of the network (currently it is security by dysfunction)
* Compatibility: Implement support for a lot of different sensors in the peripherals (see that repo)
* Communication: Extend reporting to GSM, implement some control over SMS
* Security: Add some SMS-security feature
* Quality: Test coverage (no, this is not like writing tests for some web app, sadly)
* Architecture: Pythonize - When the dust have settled, move invokation details and customization into Python code.
* Architecture: Modularize - Move all into some proper dependency handling. 



## Platform

### Software
This is an esp-idf project, but as I also like PlatformIO, I use /src rather than /main folder structure. 
Otherwise it looks the same. Also not sure why esp-idf wants to deviate from the norm (combined projects?).

### Hardware

- LilyGO-T-SIM7000G
    - With built-in GSM and GPS-modules
    - Bluetooth Low Energy (BLE)
    - ESP32 WROVER-E
- ILI9488 4" TFT 
    - 320*400 pixels  
    - Resistive touch XPT2046


## Custom 

These are settings that might be, and probably is, specific to my setup.  


### Serial flasher config

- Flash SPI mode (DIO)  --->
- Flash SPI speed (40 MHz)  --->  
*These were the flash settings that worked for the LilyGO-T-SIM7000G.*
### Component config 

#### LVGL configuration
(320) Maximal horizontal resolution to support by the library.  
(480) Maximal vertical resolution to support by the library.  
    Color depth. (16: RGB565)  --->  
*It is a multicolor display*

#### LVGL ESP Drivers  
##### LVGL TFT Display controller

- Display orientation (Portrait)  --->
  
  *That's just how I want it now.*

- Select a display controller model. (ILI9488)  --->
  
  *The TFT display chipset.*


- Use custom SPI clock frequency.
        Select a custom frequency. (20 MHz)  --->
        
    *This had to be set lower, could probably go faster with a shorter cable.*

- Display Pin Assignments  --->

    (23) GPIO for MOSI (Master Out Slave In)  
    [x] GPIO for MISO (Master In Slave Out)  
    (19)    GPIO for MISO (Master In Slave Out)  
    (0)     MISO Input Delay (ns)  
    (18) GPIO for CLK (SCK / Serial Clock)  
    [x] Use CS signal to control the display  
    (5)     GPIO for CS (Slave Select)  
    [x] Use DC signal to control the display  
    (27)    GPIO for DC (Data / Command)  
    [x] Use a GPIO for resetting the display  
    (33)    GPIO for Reset  

    *These are specific to what pins i use for the communication.*


##### LVGL Touch controller

- Select a touch panel controller model. (XPT2046)  --->  
    *Well, that is the controller.*
- Touch Controller SPI Bus. (SPI2_HOST)  --->  
    *The WROOM-E has two SPI controllers*
- Touchpanel (XPT2046) Pin Assignments  --->  
(19) GPIO for MISO (Master In Slave Out) -- Maps to "TDO"
(23) GPIO for MOSI (Master Out Slave In) -- Maps to "TDI"  
(18) GPIO for CLK (SCK / Serial Clock) -- Maps to "TCK"
(33) GPIO for CS (Slave Select) -- Maps to "TCS"  
(32) GPIO for IRQ (Interrupt Request) -- Maps to "PEN"

*These are the pins used for the touch panel* 

- Touchpanel Configuration (XPT2046)  --->  
    - (160) Minimum X coordinate value.  
    (150) Minimum Y coordinate value.  
    (1900) Maximum X coordinate value.  
    (1850) Maximum Y coordinate value.  
    *All values above were painstakingly guessed and works good enough but not perfectly, which is a bit annoying.  
    Need a better way to calibrate the touch screen for LVGL.*
    - [x] Swap XY.   -->  
    *These needs to be swapped, as it is portrait*
    - [ ] Invert X coordinate value.  
    - [x] Invert Y coordinate value.  
    *Yep. For some reason Y is inverted.*  
    - Select touch detection method. (IRQ pin only)  --->  
    *Have tried both, needs to evaluate this further. (this is the PEN-pin)*


#### Bluetooth
Uses Nimble, and mostly defaults.  
*(in contrast, the [ble_server](https://github.com/nicklasb/ble_server) and [ble_client](https://github.com/nicklasb/ble_client) repos explore the extremes of MTU sizes and transfer rates and other tricks, if that is of interest)*  

- [x] Bluetooth
    - Bluetooth controller(ESP32 Dual Mode Bluetooth)  --->  
     Bluetooth controller mode (BR/EDR/BLE/DUALMODE) (BLE Only)  --->  
    - Bluetooth Host (NimBLE - BLE only)  --->  
    - NimBLE Options  --->  


## TODO:

These are slightly strange configs in the sdkconfig.esp-wrover-kit file that I am not yet sure how they got there.
The need to be investigated to make sure that they're really needed and understood:  

- CONFIG_LV_USE_USER_DATA=y
      [x] Add a 'user_data' to drivers and objects.  
     *Not sure I will use those.*
- (0) Default image cache size.  
*This is probably only used in some cases? Could it be opposite to something not being used by multicolor screens like double buffering?*
- CONFIG_LV_LABEL_LONG_TXT_HINT  
*Not sure about this one either.*
