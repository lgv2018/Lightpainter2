# Light painter 2

Light painter 2 is a device that drives a LED strip to create persistance of vision (POV) images from 24-bit bitmap files, that can be used to paint images in long exposure photography. This is a port of the original [Lightpainter sketch](https://github.com/reven/Light-painter) to the ESP2866. The objective of using an ESP2866 were:

* Do on-the-fly file conversion
* Leverage some of the ESP's capabilities: OTA updates, web interface, OTA file upload
* Change to new display and have expanded menus.
* Change to fastLED library.

Images are kept on an SD card and through an interface on a OLED screen we can select the image to display, adjust brightness, speed and delay, as well as save the settings to EEPROM.

I will eventually get around to doing a writeup of the project, and I'll link in the STL files for the enclosure and the PCB files as well.

**Impotant**: Please read the known issues and understand well the current draw of your LEDs and the capabilities of your power source.

## Setup
This sketch works on an ESP2866 (NodeMCU module). You will also need an adressable LED strip, a power source (an UBEC and an AA battery pack work well), an OLED display (preferably with I2C module), some momentary push buttons, a slider switch and an SD card reader module.

The hardware will depend of your choice of materials or what you already have available. I 3D printed the enclosure and other parts, and instead of using readily available modules I made a custom PCB that integrates all the parts.

## Images
The images need to be 24bit bitmaps, without compression or layers.

## Web interface
Upon boot, the module will try to connect to it's saved WiFi network. If not found or not configured, it will create an ad hoc access point. You can connect to it (192.168.1.4) to configure the network or to control it.

## Main configuration options
Edit the `config.h.example` file to add your passwords or define your prefered IP addresses. Then remove the `.example` suffix.

Other options are well commented in sketch. Mainly, define your LED type, their number, and the pins you are using for your inputs/outputs. Additionally:

* set `#define CURRENT_MAX` to a reasonable value that your power supply can deliver, **but be sure to read and understand the limitations explained bellow in known issues**.

## To do:
* Last file selection bug
* Clean up code
* Publish STL files
* Publish PCB files
* Implement diagnostics
* Write up build instructions
* Use interrupts for buttons

## Rationale
The old sketch on teh Arduino relied on parsing the files and storing a raw bit format of them on the SD card in order to push those bits to the LED strip directly. This had a lot of tradeoffs, mainly we needed asembly code for the routine to write to the LEDs and the raw files had to be regenerated every time the brightness was changed.

The ESP2866 is capable of reading the files and calculating the brightness adjusted values on the fly, which simplifies things a lot. And we also get some slick file management, OTA updates and image uploads, and web capabilities. The added memory and the way the SD card is managed also take away the spartan file number limits that the original sketch had.

## Known Issues:

#### Measured current vs calculated current drawn.
As a general rule of thumb for WS2812B strips, we have a current of ~20 mA/LED. Each pixel will draw 60mA at top brightness, so for a 1m 144 strip we have a max of 60mA x 144 = 8640mA

This is a theoretical max for a full white image. The *lineMax* variable is defined as the cumulative brightness of the brightest line in a file, and that value is used to scale down the brightness of the image, so max current should be 3000mA or lower (recommended `CURRENT_MAX` value). As long as our power supply can deliver that and we use adecuate connectors (JST SM or XH are rated for 3A), we should be ok.

Besides the controls in the sketch and given it's intended use, we shouldn't reach the maximum current your LED strip is able to draw. ** *However*, you should understand the limits and ensure precuations!** You are responsible for the safety of your implementation.

I found that the measured current was well bellow the theoretical current limit set in the sketch, there seems to be some overshoot, at least in my measurements. With a max of 3500mA I was reading around 980mA with a really bright image. Though it's possible that the max current is being reached for very short periods; I don't have the proper equipment to measure the current precisely (possibly an osciloscope would give a better picture if it depends on the duty cycle).

The correction works best when the user defined brightness is maximum. Lowering that will cascade a lot of losses (safety margin + gamma correction, etc). Make sure you understand the capabilities of your power source and the chatacteristics of your LEDs and you should be good.

## Acknowledgments
The original Lightpainter sketch was based on the the one written by [Phil Burgess / Paint Your Dragon](https://github.com/PaintYourDragon) for Adafruit, that can be found [here](https://github.com/adafruit/NeoPixel_Painter). A complete write-up with instructions and hardware design can be found on [Adafruit's website](https://learn.adafruit.com/neopixel-painter/overview).
