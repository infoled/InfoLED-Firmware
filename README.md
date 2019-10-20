# infoled-InfoLED-Firmware

An example firmware for appliances supporting InfoLED

To use this code. Please copy the code in `firmware.cpp` and paste it in particle.io WebIDE.

This app is tested on Particle photon. A common cathode RGB should be used, with its red, green, and green anode pins connected to the D0, D1, D2 pins on Particle photon and cathode pin connected to GND.

For more information, please see our paper [InfoLED: Augmenting LED Indicator Lights for Device Positioning and Communication](https://doi.org/10.1145/3332165.3347954).

If you find this helpful, please cite with the following bibtex:
```
@inproceedings{Yang:2019:IAL:3332165.3347954,
 author = {Yang, Jackie (Junrui) and Landay, James A.},
 title = {InfoLED\&\#58; Augmenting LED Indicator Lights for Device Positioning and Communication},
 booktitle = {Proceedings of the 32Nd Annual ACM Symposium on User Interface Software and Technology},
 series = {UIST '19},
 year = {2019},
 isbn = {978-1-4503-6816-2},
 location = {New Orleans, LA, USA},
 pages = {175--187},
 numpages = {13},
 url = {http://doi.acm.org/10.1145/3332165.3347954},
 doi = {10.1145/3332165.3347954},
 acmid = {3347954},
 publisher = {ACM},
 address = {New York, NY, USA},
 keywords = {augmented reality, indicator light, internet of things, select and control, smartphone, visible light communication},
} 
```
