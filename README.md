# rpi_ws281x_serv
Lightweight Root server and client lib for rpi_ws281x

Provides a server to run as root utilizing ZeroMQ for messaging, and Protobuf as the message format.
Also provides a client lib that exposes a matching interface to rpi_ws281x, with the aim of allowing userspace apps to use rpi_ws281x with little to no changes.

Still a work-in-progress. 

### Requires
 - ZeroMQ (`sudo apt install libzmq3-dev`)
 - Protobuf (`sudo apt install libprotobuf-dev protobuf-compiler`)
 - cmake (`sudo apt install cmake`)
 - swig (`sudo apt install swig`)

### Building
 - `mkdir build`
 - `cd build`
 - `cmake ..`
 - `make`
 
### Running
 - In one terminal, run `sudo ./server`
 - In another, run `./test [args]`. This is the test app from rpi_ws281x compiled using this lib instead of the original. With the correct CLI args, it'll communincate with the server and start sending out LEDs.
 
### Python
Python is a bit of a work in progress build-wise. For now, do:
 - `cd build/swig/python`
 - `sudo python3 setup.py build`
 - `sudo python3 setup.py sdist`
 - `sudo python3 -m pip uninstall rpi_ws281x`
 - `sudo python3 -m pip install --upgrade --force-reinstall dist/rpi_ws281x-*.tar.gz`

Then run one of the examples in https://github.com/rpi-ws281x/rpi-ws281x-python/tree/master/examples
