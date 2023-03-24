## Audi A3 8V TPMS Retrofit
<br>

![TPMS screen](/img/TPMS_screen.jpg)

This project addresses the problem of the lack of a TPMS (Tire Pressure Monitoring System) ECU  fully compatible with the MIB1 multimedia system installed in Audi A3/S3 8V "pre facelift" (2013 to 2016) models. It uses an Arduino board connected to a pair of CAN Bus shields to read CAN Bus data sent from a 8S0907273 TPMS module and make the appropriate changes on the messages before they are forwarded to the car's CAN Bus, so that pressure and temperature data are correctly displayed on the MMI screen.

## The problem:

The Audi A3/S3 8V series was released without a direct TPMS available, and used the MIB1 version of MMI until the "facelift" in 2017, when MIB2 became standard and the 8S0907273 TPMS ECU was made available for Audi RS3. This module could be successfully installed in any A3/S3 that had a MIB2 from factory or retrofitted. Owners of PFL cars with stock MIB1, however, could only count on the unreliable, ABS based, indirect TPMS or retrofit direct TPMS with a 5Q0907273B module, which is compatible with VW Passat, Golf, Tiguan, or Seat Leon, but only partially compatible with Audi's MIB1. My suspect is that a specific TPMS module was planned for the PFL cars and never released, but the TPMS display functionality was kept in the MMI software anyway, even though it had no use.

Basically, MIB1 has fields for displaying tire pressure and temperature on the TPMS screen and expects to receive temperature data from the TPMS ECU, along with tire pressures, but 5Q0 module only supplies pressure data (it actually does internally handle temperature data sent by the sensors, but doesn't send them on the CAN Bus). As the MMI doesn't receive any temperature data, the screen only displays pressure data, and the fields that should display tire temperatures are never updated, remaining with the number that corresponds to receiving binary zeros from the module (-60º Celsius or -76º Fahrenheit, depending on the units set by the user).

It's possible to install a 8S0 module in a PFL A3/S3 with MIB1, but the multimedia screen will not display the tire pressure diagram screen. Only the "store tire pressures" screen will be displayed. It happens because the module sends a set of messages that seem to disable the tire diagram screen. Despite the incompatibility with the MMI, the TPMS system continues to work without generating any errors on the cluster, since the message that communicate TPMS alerts (CAN Bus message ID 0x5F9) uses a format that seems to be standard across the modules used in the MQB platform. It only transmits flags indicating pressure loss, flat tire or system malfunction, though. Message 0x5F9 data don't contain any pressure or temperature values. These are only transmitted through a set of extended ID messages.

## How to solve that:

If the failure of the MMI to display the TPMS screen was just caused by the lack of a message expected by the MMI but not sent by the TPMS ECU, it could be easy to solve the problem by building a circuitry with a single CAN Bus interface to be connected in parallel with the receiver, in order to inject the lacking message. But that's not the case. There are messages that have to be modified, others inserted and others deleted. For this reason, the ECU has to be isolated from the bus and its messages have to be passed through a filter before being sent to the car. The circuit proposed in this project to perform such task consists in two CAN Bus interfaces, one connected to the car's Extended CAN Bus and the other to the TPMS ECU, with an Arduino board in the middle, programmed to forward the messages while doing the necessary changes.

## Summary of the messages exchanged:

Only four CANBUS message IDs were observed as being sent by the TPMS ECU. The message ID 0x5F9 is the only one with standard ID, and, as mentioned in the beggining of this text, contains alert flags related to faults like pressure loss, flat tires or system malfunction. If everything is OK, the message body is composed by all zeros. It's sent by the ECU at every 500ms. If for whatever reason the car doesn't receive any such messages for about 4 seconds, a "TPMS malfunction" alert is displayed on the instrument cluster.

All other three message IDs are of the extended type (11 bit ID). The IDs 0x1B00000B and 0x17F0000B are sent by the ECU at regular intervals and I couldn't identify their function. Maybe they are related to some kind of control or synchronism, but the fact is that the TPMS system seems to work normally even if they are completely blocked, so I didn't perform any deeper analysis on them.

The message ID 0x17330710 seems to contain all the relevant information for the MMI. As a CAN Bus message body is only 8 byte long, the information is broken into a series of messages where the first and second bytes identify what kind of data (flags, tire pressures, status, temperatures) is contained in the remaining bytes. The message body for ID 0x17330710  is modified by the code running in the Arduino module as needed before being forwarded to the CAN Bus. All other messages from the TPMS ECU are forwarded unchanged.

Tire pressure and temperature data are contained in messages that have the first byte containing the values 0x31 or 0x41 (decimal 39 or 65) and the second byte containing the following (decimal) values:
- 208 – Related to the highlight colors to be displayed under the pressure fields on MMI screen
- 209 – Tire pressures
- 210 – Tire temperatures
- 211 – Specified tire pressures

Third field in the messages 209, 210 and 211 correspond to the measuring units corresponding to the data. Value zero corresponds to default, BAR for pressure data and Celsius for temperature data. Value 1 corresponds to PSI and Fahrenheit. Fields from fourth to seventh contain the values for each tire in the order: front left, front right, rear left, rear right. Eigth field seems to be intended to store values for a spare tire, but I couldn't confirm that.
There are also another few messages with the second byte containing the (decimal) values 205, 206, 207 and 215, but I couldn't identify what kind of info they contain.

## The code:

The Extended CAN Bus works at 500 kbps and has intense traffic, with hundreds of messages bein sent per second. As no change is needed on the messages to be sent from the bus to the TPMS module (only from the module to the bus), the messages could be simply forwarded from the car side interface to the ECU side, and that's what I did in the first versions of the filter, but it soon became evident that the Arduino processor wasn't fast enough to handle the high volume of messages. Some of them were being lost in the interface buffers and that was a problem because there is a set of four messages that the ECU has to receive regularly from the car, otherwise it generates a DTC in the module and a "TPMS malfunction" alert may be displayed on the cluster. These messages are: 0xFD  (ESP_21), 0x30B (Kombi_01), 0x3C0 (Klemmen_Status_01) and 0x585 (Systeminfo_01).

Additionally, it has to receive the message 0x643 (Einheiten_01) that defines the units configured in the car, including pressure (Bar or Psi) and temperature (ºC or ºF), but no alert is generated if the message is not received by the ECU. In this case, it defaults to the units Bar and ºC. Another two messages considered relevant are 0x6B2 (Diagnose_01) which contains information on car's mileage and date/time data (for diagnostic purposes and allows the ECU to include such info in the Freeze Frame data in case a DTC is generated) and 0x6B7 (Kombi_02) which contains exterior temperature info.

In order to reduce the load on the processor, the masking/filtering capabilities of the MCP2515 chip in the CANBus interfaces were programed to allow a few ranges of message IDs that contain the messages identified as useful, thus blocking other ranges that contain unnecessary but very frequently sent messages, especially ESP_19 (0xB2) and TSK_06 (0x120). 

Additionally, the part of the code that reads from the car side interface was programmed to forward only the seven messages identified as useful and discard any others but the range from 0x700 and 0x7FF, which corresponds to UDS diagnostics, so it's possible to use diagnostic tools like VCDS, VCP or ODIS to access the ECU.

By discarding all of the unnecessary messages, the Arduino processor seemed to be able to handle the processing with minimal losses. Nevertheless, there were still a few timeout errors for the Systeminfo_01 message being recorded internally as DTCs by the TPMS module, probably because that message is only sent in a wider interval of 1000 ms, so just a couple of losses might be sufficient to exceed the timeout setting. In order to solve that issue, coding was added to store the last Systeminfo_01 message received from the bus and resend it to the module if no new one is received at every 10 Klemmen_Status_01 messages. Using a count of  messages of another ID instead of a time count was preferred so the Systeminfo message would not be repeated indefinitely by the Arduino module when ignition is switched off and all messages stop to be received.

## What is changed by the converter circuit:

Changes are made only on the messages with the ID 0x17330710. These are the changes made by the converter circuit:

1 -  The message with first byte containing the values 0x80 or 0x90 (decimal 128 or 144) seem to contain a series of flags that define the way the TPMS ECU sends data, especially what messages are to be expected by the MMI to be sent by the ECU. The flags sent by the module 5Q0 are different from the ones sent by the 8S0 and neither are fully compatible with MIB1. Sending the message exactly like it's sent by a 5Q0 module makes the MMI to expect for a few messages that are unnecessary and that aren't sent by the 8S0, and also makes the MMI to ignore temperature data messages that the 8S0 module sends. Sending the 8S0 message unchanged disables the TPMS screen. A working set of flags, that at same time makes the screen to be displayed and accepts temperature data, was found by trial and error, since there is no module specifically designed to work with the MIB1 to extract the right data. Basically, the original 8S0 message is kept and only byte 6 (seventh byte) is changed from 0xE1 to 0xF1.

2 - The message that contains pressure data (first byte 209) has the default values 0xFF (displayed as 25.5 Bar or 127.5 Psi) replaced with zeros.

3 - The message that contains temperature data (first byte 210) has the default values 0 (displayed as -60ºC or -76ºF) replaced with decimal 60, if the unit set is ºC, or decimal 38, if the unit set is ºF, so the tire temperatures are displayed as zero.

4 - A message with the second byte with the value 211, which is not sent by the 8S0 module, is sent after the message 210 (tire temperatures). That message is normally sent by the 5Q0 module and seems to contain the specified pressures for each tire according to the tire set choosen, but it makes no sense for the Audi MIB1 since it doesn't have a table of tire sets. But if it's not received by the MMI the TPMS screen fails to be displayed. This is probably because the set of flags I found by trial and error isn't perfect. It's just a combination that at same time allows the tire pressure screen to be displayed and temperature data to be accepted. There must be a flag that sets the "211" message as not required, but it wasn't found in the tests.

The project uses an Arduino Nano board connected to two CAN Bus shieds based on the MCP2515, using MCP CAN lib by Cory J. Fowler (https://github.com/coryjfowler/MCP_CAN_lib).

## Building the circuit

The circuit is composed by an Arduino board connected to two MCP2515 based CANbus shields through SPI. One shield is connected to the car's Extended CANbus wiring and the other is connected to the TPMS ECU. A circuit diagram using an Arduino Nano and Ni-Ren style CANbus shields is shown on the picture below. A voltage regulator was added because some sources don't recommend to connect the Arduino Nano board directly to 12V due to limitations of its own internal regulator.
<br>
![Filter circuit](/img/8S0_filter_circuit.jpg)
<br> <br> <br>
This is how the circuit looked like with the wiring connections in place. The heading pins were removed from the CANBus shields so wires could be soldered directly to the circuit boards. The pins corresponding to the terminating resistor jumper were closed with a soldered piece of wire.
<br>
![Circuits and wiring](/img/Circuits_and_wiring.jpg)
<br> <br> <br>
