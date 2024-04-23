# espiot
A small board built to measure current and voltage of an appliance.<br />
This board was used as a testbed for STM32 ADC oversampler, UART and DMA.<br />
To measure current and voltage values, a 12-bit ADC was used, with an oversampling ratio of 256x and an 8-bit shift to obtain efficient data averaging - this provides stable readings, along with a sampling time of 3.5 cycles for current and 79.5 cycles for voltage. DMA was also used to transfer all the ADC readings into memory without performance losses.
![mobile_app](https://github.com/Kikkiu17/espiot/blob/main/mobile_app.jpg)
![pcb](https://github.com/Kikkiu17/espiot/blob/main/pcb/pcb.png)
