name = 'WS231x Weather Station Receiver'
description="""
<h1>WxRx</h1>

<h2>Overview</h2>
<p>
The purpose of this project is to implement a replacement for a LaCrosse WS231x 
console (the inside) unit using an Arduino Duemilanove with an attached RXN3-B OOK receiver.
</p>

<p>
This device outputs data at 115200bps via the USB/serial link to allow all the transmissions
from a LaCrosse outside unit to be sent to a computer. The data format is compatible with the
Open2300 log2300 software (but without the time stamps). It also implements the 1 hour and 
24 hour rainfall algorithms.
</p>

"""
