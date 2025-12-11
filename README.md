# Webserver
This is a simple implementations of a webserver. The server is based on TCP-Sockets and implements parts of the HTTP-protocol. This repository is the reusl of a 
universityproject in the course *Rechnernetze und Verteilte Systeme (Computernetworks and Distributed Systems)* at the TU-Berlin. The task was to follow Beej's Guide on Network Programming (https://beej.us/guide/bgnet/).

1. Clone the repository
   ```bash
   git clone https://github.com/Kolja-05/Webserver-RNVS.git

2. Compile the programm
   ```bash
   cmake -B build
   make -C build

3. Start the webserver
   ```bash
   ./build/webserver 127.0.0.1 1234
4. Make a HTTP-request from another terminal
   ```bash
   echo -e "GET /static/foo HTTP/1.0\r\n\r\n" | nc localhost 1234
5. URIs for PUT/DELETE  have to start with /dynamic/
  ```bash
   echo -e "PUT /dynamic/foo HTTP/1.0\r\nContent-Length: 3\r\n\r\nfoo" | nc localhost 1234


