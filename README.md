
# Controller

This is a central unit for managing and collecting data from sensors using:
* BLE for common, always-on situations
* Wake on GPIO, then BLE for low-power situations and alarms.  
*Like intrusion (theft, camera), danger (fire, leaks)*
* GSM SMS reporting and control
* GPS for positioning
* LGVL-driven touch TFT for interaction
* Competent multi-core usage for maximal performance and stability
* The ESP32 WROVER-E MCU

The first evolution is hard-coded to a specific setup, the next will attempt to be more dynamic. 

## Platform

### Software
This is an esp-idf project, but as I also like PlatformIO, I use /src rather than /main folder structure. 
Otherwise it looks the same. Also not sure why esp-idf wants to deviate from the norm.

### Hardware

- LilyGO-T-SIM7000G
    - With built-in GSM and GPS-modules
    - ESP32 WROVER-E
- ILI9488 4" TFT 
    - 320*400 pixels  
    - Resistive touch XPT2046


## Custom 

These are settings that might be, and probably is, specific to my setup.  


### Serial flasher config

- Flash SPI mode (DIO)  --->
- Flash SPI speed (40 MHz)  --->  
*These where the flashsettings that worked for the LilyGO-T-SIM7000G*
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
    [\*] GPIO for MISO (Master In Slave Out)  
    (19)    GPIO for MISO (Master In Slave Out)  
    (0)     MISO Input Delay (ns)  
    (18) GPIO for CLK (SCK / Serial Clock)  
    [\*] Use CS signal to control the display  
    (5)     GPIO for CS (Slave Select)  
    [\*] Use DC signal to control the display  
    (27)    GPIO for DC (Data / Command)  
    [\*] Use a GPIO for resetting the display  
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
    - [\*] Swap XY.   -->  
    *These needs to be swapped, as it is portrait*
    - [ ] Invert X coordinate value.  
    - [\*] Invert Y coordinate value.  
    *Yep. For some reason Y is inverted.*  
    - Select touch detection method. (IRQ pin only)  --->  
    *Have tried both, needs to evaluate this further.*


    ## TODO:

These are slightly strange configs that needs to be investigate so that they are really needed and understood:  

- CONFIG_LV_USE_USER_DATA=y
     [\*] Add a 'user_data' to drivers and objects.  
     *Huh?*
- (0) Default image cache size.  
*This is probably only used in some cases?*
- CONFIG_LV_LABEL_LONG_TXT_HINT  
*Not sure about this one either.*