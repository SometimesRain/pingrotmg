## Tiny RotMG Ping Tester
Compiles to a 11 KB executable.

![](/pingrotmg.png)

### About

* Simultaneously tests ping to all the servers
* Uses Windows sockets with the IO completion port model
* The completion port is executed in two threads

### Shortcuts taken

* Only takes one sample per server
* Does not dynamically load the server IP addresses
