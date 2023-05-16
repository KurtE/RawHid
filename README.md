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
teensy41.menu.usb.rawhid.upload_port.usbtype=USB_RAWHID
teensy41.menu.usb.rawhid512.fake_serial=teensy_gateway

teensyMM.menu.usb.rawhid512=Raw HID 512
teensyMM.menu.usb.rawhid512.build.usbtype=USB_RAWHID512
teensyMM.menu.usb.rawhid.upload_port.usbtype=USB_RAWHID
teensyMM.menu.usb.rawhid512.fake_serial=teensy_gateway

teensy40.menu.usb.rawhid512=Raw HID 512
teensy40.menu.usb.rawhid512.build.usbtype=USB_RAWHID512
teensy40.menu.usb.rawhid.upload_port.usbtype=USB_RAWHID
teensy40.menu.usb.rawhid512.fake_serial=teensy_gateway
```
My earlier testing for 512 bytes has been done on linux... 

I have since then done most of my testing on Windows.  Earlier on Windows 10, now on
Windows 11.  The earlier builds were done by doing the  build with 
Ubuntu prompt under windows.

I am now doing most of my stuff using Visual Studio 2022.  I needed to install the WDK.  I used the
instructions up at: https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk

There was one additional package I needed to install, having to do with Spectre. 

I had to rerun the Visual Studio installer with modify and added some packages.  Go to the Individual Components and I believe it had a name like: ... VS 2022 C++ x64/x86 Spectre-mitigated libs (latest)

Again this is only an experiment,  But it is working pretty good. Hopefully it will be integrated
back into the official RawHid support