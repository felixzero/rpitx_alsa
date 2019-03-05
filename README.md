# rpitx_alsa
An ALSA sound driver interface for sending samples to F5OEO's rpitx.
This is only a proof-of-concept demo. It should work, but it is in no way complete and/or perfect.

## How does it work

This program is comprised of a kernel module, whose job is to collect the audio data from whichever amateur radio program you want to use, and a daemon, whose job is to call `librpitx` to transmit it. Internally, they communicate through the `/dev/rpitxin` character device.

## Compile and install

First, I assume you have already tried the "regular" rpitx and everything has been configured properly on your Pi. I.e., follow all the instructions of rpitx first: https://github.com/F5OEO/rpitx.

Then, you need to clone this repository. Since it depends on the F5OEO's "librpitx" repository, you need to run:
```
$ git clone --recursive https://github.com/felixzero/rpitx_alsa.git
```

Then you need to build the kernel module (the ALSA driver). You need the kernel headers and of course, gcc. But you need to make sure your system is up-to-date first, otherwise the build will fail. Run:
```
sudo apt update
sudo apt upgrade
```
And reboot if any significant update was done. Then, run:

```
$ sudo apt install raspberrypi-kernel-headers
```

Then:

```
$ cd rpitx_alsa/
$ cd kernel_module/
$ make
```

If there are no error, you can now compile librpitx and the daemon:

```
$ cd ../daemon/librpitx/src/
$ make
$ cd ../..
$ make
```
If there are no errors, you are done!

## Run

To be able to use the transmitter, you must first load the kernel module. In the `kernel_module` folder, just run:

```
$ sudo insmod snd-rpitx.ko
```

Then, go up one folder (`$ cd ..`), and run the daemon in background, still as root:

```
$ sudo ./rpitxd &
```

Now, you are all set, you can configure your favorite amateur radio software (Quisk, fldigi, WSJT-X, QSSB...) to send data to one of the following sound devices:
- `hw:rpitx,0` for software producing stereo I/Q data (for instance Quisk or all SDR software)
- `hw:rpitx,1` for software producing mono SSB data (most digimode programs). The sound driver will generate the adequate Q data, assuming the original sound is USB.

You can also dynamically tune the frequency and harmonics of rpitx by writing to:

```
$ sudo su -c "echo 14070000 > /sys/devices/rpitx/frequency"
$ sudo su -c "echo 1 > /sys/devices/rpitx/harmonic"
```
Have fun!
