#  The Robusto framework

<i>When the job has to be done and the data has to get there</i>

Robusto will be a multichannel communication and orchestration framework for microcontrollers.

Its final goal is to gracefully degrade both communications and operations: 
* If a cable is chafed, it uses wireless. 
* If a microwave breaks, it uses wires.
* If a peer moves far away, it uses LoRa or UMTS. 
* If it comes close enough, it uses ESPNOW (wifi).
* If a peer stops working, another will be used.

This is hopefully made possible by the abilities of today's cheap microcontrollers and their peripherals to interact and perform.

Features: 
* Adaptive and multichannel redundant communication - BLE, ESPNOW, LoRa, UMTS, Wire
* Orchestration - Scheduling and timing of node conciousness
* Task management - Queueing, timeboxing
* Monitoring - System health, QoS, Throughput, Mesh

