import socket

# This is a simple of a UDP client
# It will freeze if there is no server

# Create the UDP socket
UDPSock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)

# Simply set up a target address and port
# eg: for a known machine
# addr = ("192.168.0.3",8888)

# Or just the port for the local machine
addr = ("",8888)

# ... and send data out to it!
data = "x" # dummy value
while data != "":
    data = raw_input(" Command (RET to exit) ?")
    if data != "":
        UDPSock.sendto(data,addr)
        data,rep = UDPSock.recvfrom(1024)
        print data.strip()#,rep
