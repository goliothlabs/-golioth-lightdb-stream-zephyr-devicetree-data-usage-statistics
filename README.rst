Extended Golioth Light DB Stream Sample - Get Device Tree Sensors and build CBOR payload for the Thingy91 + Data Usage Statisctics
##############################

Overview
********

This sample extended the basic Light DB stream application, that demonstrates how to connect with Golioth and
periodically send data to Light DB stream. In this sample, it gets sensors from the Zephyr Device Tree and dynamically build a CBOR payload
to be send as measurements to ``/sensor`` Light DB Stream path.  It sends data usage statistics to ``/statistics`` Light DB Stream path. Usage statistics
minimum resolution is kilobytes.

Tested with nrf connect sdk version 1.7.1

Requirements
************

- Golioth credentials
- Network connectivity

Building and Running
********************

Clone the zephyr-sdk repo and follow directions to add the golioth submodule. Clone this repository and add the root directory of this repository as a 
folder in the modules/lib/golioth/samples directory of the deployed zephyr-sdk project.

Configure the following Kconfig options based on your Golioth credentials:

- GOLIOTH_SYSTEM_CLIENT_PSK_ID  - PSK ID of registered device
- GOLIOTH_SYSTEM_CLIENT_PSK     - PSK of registered device

by adding these lines to configuration file (e.g. ``prj.conf``):

.. code-block:: cfg

   CONFIG_GOLIOTH_SYSTEM_CLIENT_PSK_ID="my-psk-id"
   CONFIG_GOLIOTH_SYSTEM_CLIENT_PSK="my-psk"



