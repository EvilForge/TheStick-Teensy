# TheStick-Teensy
An ESP8266/TeensyLC GPS Geocaching Walking Stick. (No.. seriously..)

## Overview:
Trying to push an ESP8266 to its limits, I suppose. Ran into issues. Teensy to the recue! (Teensy fixes a lot of issues, when you start goin crazy with microcontrollers... check them out at [PJRC's Website](https://www.pjrc.com).

I *still* find Geocaching fun. I don't do it near as much however. But I've got the gear.. or do I?  I mean I have a walking stick (custom made of course with built in LED headlights, and a compass on the top). And I have a GPS. And I have a phone and watch to track my distance. 

But I don't have a walking stick that takes the place of "everything" above. (Yet)

So it's time to remedy that. Ouila, the GPS Geocaching Walking Stick was born.

The GPS works, the sensors work, the LED's work, the code is starting to come together (minus the navigation part but I'm close). I even have the new stick cut up and places to mount the pixels, battery holders (still debating on that.. it needs a quick way to swap an 18650).

The code is in initial stages with most of the components I need working.. so.. here's the first upload.

NOTE this is a pet project, so updates may be intermittent. But I have high hopes.. 

## Requirements:
This is just the Teensy part of the project. There's an ESP8266 running the web interface. This just runs Pixels and GPS and input sensors, and its serial linked to the ESP8266. 

Why a multi-tier project?  Well.. convention dev wisdom says you isolate Presentation from App/Logic and DB layers. And thats kind of what I am doing here. 

Nah I'm full of it. What really happened is that the ESP8266, for all its power, can't reliably run SoftwareSerial (the new one, even), to read GPS, also run pixels, and a web server without something giving.. in my case, SoftwareSerial got interrupted too much and serial GPS data started getting trashed. (I tried limiting things, I don't use sleep or wait, I yielded.. its just that without a true hardware serial port, you run outta timers or into conflicts.. I guess.)

BUT, a low baud rate link to a Teensy, which does have lots of hardware serial, resolves it. And lets you isolate code and such (Teensy LC doesent have a ton of program space anyhow). So this repo is supposed to work with the Teensy version, at the same time, in concert. to provide the full solution.

I dont have the hardware fully finalized but i will put a diagram and BOM up there in the files, at some point.

If you feel like it, feel free to [buy me a coffee!](https://www.buymeacoffee.com/rbef)
