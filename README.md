# RawHid

This is my hacked up version of the PJRC Raw HID Source Files, that I downloaded from the page:
https://www.pjrc.com/teensy/rawhid.html

I am currently experimenting with seeing if it would make sense to have a version of the code that
allows you to have 512 byte buffers instead of 64 bytes on T4.x boards. 

This is discussed in the thread: https://forum.pjrc.com/threads/68620-T4-x-support-for-Raw-HID-512-wonder-if-it-makes-sense-to-add-to-system

I currently have a hacked up branch of the: teensy cores project with the changes up at: 
https://github.com/KurtE/cores/tree/rawhid_512

I hacked up a boards.local.txt, that allows a 512...

```
teensy41.menu.usb.rawhid512=Raw HID 512
teensy41.menu.usb.rawhid512.build.usbtype=USB_RAWHID512
teensy41.menu.usb.rawhid512.fake_serial=teensy_gateway

teensyMM.menu.usb.rawhid512=Raw HID 512
teensyMM.menu.usb.rawhid512.build.usbtype=USB_RAWHID512
teensyMM.menu.usb.rawhid512.fake_serial=teensy_gateway

teensy40.menu.usb.rawhid512=Raw HID 512
teensy40.menu.usb.rawhid512.build.usbtype=USB_RAWHID512
teensy40.menu.usb.rawhid512.fake_serial=teensy_gateway
```
So far my testing for 512 bytes has been done on linux... 

I am currently playing with it on windows 10.  So far I have been able to build with 
Ubuntu prompt under windows and now have it building using Visual Studio after
I installed the current Windows SDK and the WDK or some such name...

Again this is only an experiment, unclear if it will be finished and/or will be merged back into the
official RawHid support