Dot detector
============

This program grabs images from the camera latest added to the system (TODO Grab from given camera),
and finds dots in it. These dots are then sent to given port (udp) on given IP. 
When the program starts it will present three windowses, one with the image grabbed from the camera (plus some overlay),
one black and white showing the currently detected shapes (one shape is one dot), and one warped image.
Before any sensible data can be had from the dot detector the transformation area has to be set. This is done by pressing 't'
on the keyboard and then click the corners of the screen where dots should be detected. Now the warped image should show the
image within the transformation area as a rectangle. If not you just redo the process. If you don't want to relocate a point
just right click instead and you'll skip the the next point.

When the transformation area is set you can improve the performance of the dot detector by turning off the warp image ('w'). 
The points will still be transformed, but there is no need to transform the whole image.

If you are detecting dots outside the area of interest you can press 'm' to set the masking area. You get no live feedback
about where you clicked, but the overlay in the top tells you what point to set next.

Keys
-----
* ESC - close the program
* e - start automatic calibration of exposure time
* g - start auto-detection of calibration shapes
* c - cycle camera to use
* t - recalibrate the transformation area (right click to skip point)
* m - recalibrate the mask area (right click to skip point)
* f - flip the image horizontally
* v - flip the image vertically
* s - stop updating the image
* n - cycle noice reduction mode
* w - toggle showing of warped area

Noice reduction
---------------

There are currently three types of noice reduction. They appear in order
* None - No noice reduction is done at all
* Erode - Kind of a floor for pixels. Small areas of matching color are removed
* Delite - Kind of a ceil for pixels. Small areas of matching color are enlarged


Install instructions
====================
make && ./dotdetector [server ip] [server port]

On rasberry pi
-------------

    # if your pi has only 256 MB memory (check using 'free -m'), run this and reboot:
    # sudo sh -c 'echo "CONF_SWAPSIZE=500" > /etc/dphys-swapfile'

    sudo apt-get install git
    git clone https://github.com/FireArrow/dotdetector --depth=1
    git clone https://github.com/Zazcallabah/Install-OpenCV --depth=1
    cd Install-OpenCV/RaspberryPi
    # the following step can take 48+ hours, I'd recommend distcc (see links below) to speed up the process
    ./opencv_latest.sh
    cd ../../dotdetector
    make
    ./dotdetector

in another terminal:

    sudo apt-get install nodejs npm
    git clone https://github.com/FireArrow/MultiDuckhunt --depth=1
    cd MultiDuckhunt
    npm install ws
    nodejs server.js

then surf to http://localhost:8888/ using your favorite browser. (must support webgl)

Note that it was quicker for me to do all of the following:
* create a new virtual server instance
* install ubuntu
* install distcc
* configure distcc
* write a new raspbian-image to a sdcard
* start the pi and install raspbian
* install distcc on the pi
* configure distcc on the pi
* install the raspberry toolchain on both computers
* install all opencv prerequisites on the pi
* configure cmake for opencv on the pi
* try build opencv using distcc, fail
* debug gcc on the build server
* figure out the problem being 64bit vs 32bit libraries 
* install correct libraries on the build server
* retry
* finish opencv build

than to just build opencv on the raspberry directly. Despite not having heard of distcc before now. As in, I started building opencv on one raspberry first, and while that slowly progressing, I did all the other stuff on another raspberry and it still was quicker. By at least 12 hours.

for reference:
* http://askubuntu.com/questions/73491/no-such-file-or-directory-for-existing-executable/165536#165536
* http://jeremy-nicola.info/portfolio-item/cross-compilation-distributed-compilation-for-the-raspberry-pi/

