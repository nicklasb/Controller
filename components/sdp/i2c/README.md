# I2C wired protocol for Robusto


Robusto can use I2C for wired communications. 

## Scheme: Only master when sending

The method it uses is slightly atypical; all peers are both masters and slaves.  

A peer is a master only when it is sending and receiving acknowlegdements for that.  
Then it immitiately goes in to slave mode and listens to the bus for packets with its address.

This way peers can freely commununicate and adjust communication speeds without interfering with others. 
Changing modes is relatively quick, especially stopping being the master, which makes the performance impact small.

## Performance optimizations
To minimize the time it hogs the line to optimize throughput, Robusto  
predicts the time a transmission will take and adjust timeouts to this. 

TODO: This should be possible to extend to other technologies.