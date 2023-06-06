# TCP based on UDP
#### Overview

This project is a simulation of TCP protocol using C++ UDP sockets, in order to realize reliable data transfer even when there is delay or packet loss in the network.

The project is based on MP2 of [ECE/CS 438](https://courses.grainger.illinois.edu/cs438/fa2021/) @ UIUC. It is implemented by one person, even if the course allows to complete it in a team of two.

#### How to run

- The easiest way is to open two virtue machines and set VM's network to be host-only so that the two machines can communicate with each other. Make sure VM's OS' are Linux. Or you can deploy the sender and the receiver in two different machines and configure the network between the two machines.
- Complie source code by `make`

- During the test, we can first use Linux' [tc](https://en.wikipedia.org/wiki/Tc_(Linux)) (traffic control) tool to configure the network card to lose packets and add network delay.

  ```bash
  sudo tc qdisc del dev ens33 root 2>/dev/null
  sudo tc qdisc add dev ens33 root handle 1:0 netem delay 20ms loss 5%
  sudo tc qdisc add dev ens33 parent 1:1 handle 10: tbf rate 20Mbit burst 10mb latency 1ms
  ```

- Then we can begin to transfer it using the simulated TCP.

  ```
  sender machine:
  	./reliable_sender [IP Address] [Port#] [Filename] [Bytes to Transfer]
  receiver machine:
  	./reliable_receiver [IP Address] [Port#] [Filename]
  ```

  For example, suppose there is an input file `FromAbyss.mp4`, we can use the following commands:

  ```bash
  vm01:
      ./reliable_sender 192.168.10.129 5000 FromAbyss.mp4 285647460
  vm02:
      ./reliable_receiver 5000 FromAbyss.mp4
  ```

#### TCP State Machine

Image by [Romit Roy Choudhury @ UIUC](https://croy.web.engr.illinois.edu/)

![](https://github.com/TaKeTube/ECE-438/blob/main/mp2/TCP.jpg?raw=true)
