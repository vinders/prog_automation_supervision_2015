#########################################################################

    Human-machine interface
    QNX device driver + local control server + distant supervision client
    -----------------------------------------------------------------
    
    Author         : Romain Vinders
    Driver         : C, QNX kernel
    Local server   : C++
    Distant client : C# .NET
    Date           : 2015
    License        : GPLv2

#########################################################################

QNX device driver :
- communicates with device through PCI 8255 card
- initializes and stops PCI card
- receives/converts data and reads/writes through PCI card

Local control server : 
- uses driver descriptor for input/output
- threads -> parallel tasks
- communicates with distant client through TCP socket
- receives commands from distant client
- notifies client when the state of a sensor changes

Distant supervision client :
- connects to control server
- receives sensors/actuators information, sends actuators commands
- interactive device display -> shows device and actuators/sensors status
- interactive device display -> change actuator status by clicking on it
