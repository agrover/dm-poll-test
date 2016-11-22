This is a demo/test program for a proposed dm event mechanism using poll().

Steps:

* Build and boot a kernel with the poll DM event patches
* compile this program with `make`
* Set up some dm devices that might generate an event, such as a thin LV on a massively-underprovisioned thinpool
* run this
* start using the thin LV, it should generate a low-water event, and this should tell you that and print its status

