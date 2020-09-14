# UDP Terminal Communication
This is C program that I built for an assignment from my operating systems course. It sends messages from one terminal to another, whether they're on the same machine, or different machines on the same network.

## How it works
Open two terminals, either on the same machine, or different machines on the same network. On each machine, compile the program with the 'make' command, then enter this command into both terminals:

######./s-talk <My Machine's Port> <Remote Machine Name> <Remote Port Number>
Then send messages from one terminal, and watch them pop up on another!
You can also send the outputs of the terminals to a file using:

######./s-talk <myPortNumber> <remoteMachineName> <remotePortNumber> >> testFile.txt

What works: 

 - Communication between two terminals via localhost
 - Communication between two terminals over a network
 - Sending data to a file by doing ./s-talk <myPortNumber> <remoteMachineName> <remotePortNumber> >> testFile.txt

Citations:

 - Citation (1): https://stackoverflow.com/questions/58430426/c-convert-domain-name-to-ip-address-with-getaddrinfo
 - Used code / took help and ideas from workshops 7, 8, and 9
 - Beej's guide: http://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf