import json
import sys
import requests
import socket
import time

payload =   'Art-Net\x00'+\
            '\x00\x50' + '\x00\x14' +\
            '\x00\x00' + '\x00\x00' +\
            '\x00\x30' + \
            '\xFF'+'\xFF'+'\xFF' + \
            '\xFF'+'\x00'+'\x00' + \
            '\x00'+'\xFF'+'\x00' + \
            '\x00'+'\x00'+'\xFF' + \
            '\xFF'+'\x00'+'\x00' + \
            '\x00'+'\xFF'+'\x00' + \
            '\x00'+'\x00'+'\xFF' + \
            '\xFF'+'\xFF'+'\xFF' + \
            '\xFF'+'\xFF'+'\xFF' + \
            '\xFF'+'\x00'+'\x00' + \
            '\x00'+'\xFF'+'\x00' + \
            '\x00'+'\x00'+'\xFF' + \
            '\xFF'+'\x00'+'\x00' + \
            '\x00'+'\xFF'+'\x00' + \
            '\x00'+'\x00'+'\xFF' + \
            '\xFF'+'\xFF'+'\xFF' 

try:
    s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
except socket.error as msg:
    print ("Failed to create socket. Error code: " + str(msg[0]) + " , Error message : " + msg[1])
    sys.exit();

print ("Socket Created")
print ("trying to connect to: ...")

try:
    s.connect(('192.168.0.165',6454))
except socket.error as msg:
    print(msg)
    sys.exit();

print ("Socket Connected!")

try:
    message = payload
    s.sendall(message.encode('ISO-8859-1'))
    #s.sendall(bytes(message))
    
except socket.error as msg:
    print ("Send failed")
    s.close()
    sys.exit()

print("Message send successfully: "+message+"\r\n")

#feedback = s.recv(1024)
#print(feedback)
s.close()
print("Socket closed.....")

#j = json.loads(r.text)
