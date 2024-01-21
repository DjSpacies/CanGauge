# CanGauge
Can-Bus Gauge

[![Video Title](http://img.youtube.com/vi/2tDzfKrj1qk/0.jpg)](http://www.youtube.com/watch?v=2tDzfKrj1qk)

Here is a basic CANbus gauge which I designed and built to fit into the existing gauge cluser of my Toyota
It's based on an Arduino Mega (Although it should work on Arduino UNO aswell if you remove the splash animation due to ROM size constraints).

Telemetry data is streamed in over the vehicles CANbus system using SeeedStudio's CANbus shield https://www.seeedstudio.com/CAN-BUS-Shield-V2.html
![image](https://github.com/DjSpacies/CanGauge/assets/48278059/fa2137ea-a077-416b-897c-0fa569be4fd4)

I have written some code to identify and extract the relevant CAN frames to be processed and eventually displayed.

The output uses a 128x64 OLED display screen which is integrated into my cars existing gauge cluster. - Unfortunately I ordered a green but they sent me a white, so it looks a little out of place untill I can get a replacement green OLED. I opted for the I2C version rather than SPI for simplicity.

I have implemented 3 parameters so far - Engine Coolant Temperature (ECT), Oil Temp, and Voltage.
Update rate is rather slow, I believe the CANbus input to be the bottleneck, There will need to be some optimisations (or even just cleaner code) to be able to get a suitable refresh rate for something like throttle position or boost pressure.
Output can be displayed in 2 modes, Digital bar graph - which emulates the original style of my cars digital dash, and Basic numerical output (eg: "Voltage: 13.03v")
The display parameters and mode switching is handled by a digital rotary encoder with push button. I was able to handle 100% of signal debouncing in software albeit with some hackery.

Also, it runs DOOM!
I cant take credit for the port, I borrowed the code from here: https://github.com/daveruiz/doom-nano
I modified the controller code to take input from the CANbus and mapped the buttons to my cruise control stalk.
Its quite laggy and to be honest not really playable in its current state, optimisations could be made but It's ok as a PoC for now.
[![Video Title](http://img.youtube.com/vi/0PjEvwb8M5c/0.jpg)](http://www.youtube.com/watch?v=0PjEvwb8M5c)
