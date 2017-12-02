import socket

# This is a simple of a UDP client
# It will freeze if there is no server

# Create the UDP socket
UDPSock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)

# Simply set up a target address
# eg: for a known machine
# IP = ("192.168.0.3")

# Or empt for the local machine
IP = ("")

# ... and send data out to it!
port = int(raw_input("Port to connect (decimal) ?"))
addr = (IP, port)
data = "x" # dummy value
while data != "":
    data = raw_input(" Command (RET to exit) ?")
    if data != "":
        UDPSock.sendto(data,addr)
        data, rep = UDPSock.recvfrom(512)
        print data.strip()#,rep
