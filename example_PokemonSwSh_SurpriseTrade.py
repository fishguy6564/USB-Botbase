#Simple example on how to connect to and send commands to the sys module.
#The example is for Pokemon Sword/Shield, it reads a .ek8 file from a certain file path, injects it into box1slot1
#and starts a surprise trade with the given pokemon. It waits a certain amount of time (hoping the trade has completed)
#before retrieving the new pokemon. Finally it extracts the pokemons .ek8 data from the game and saves it to the hard drive.
#The script assumes the game is set up in a way that the character is not currently in any menus and that the cursor of the
#pokebox is on box1slot1.

#The script isn't exactly robust, there are many ways to make it better (for example one could compare the box1slot1 data in
#RAM with that of the pokemon sent to see if a trade has been found and if not back out of the menu to search for another 10 
#seconds or so instead of waiting a fixed 45 seconds), but it is rather meant as a showcase of the functionalites of the 
#sysmodule anyway.

#Commands:
#make sure to append \r\n to the end of the command string or the switch args parser might not work
#responses end with a \n (only poke has a response atm)

#click A/B/X/Y/LSTICK/RSTICK/L/R/ZL/ZR/PLUS/MINUS/DLEFT/DUP/DDOWN/DRIGHT/HOME/CAPTURE
#press A/B/X/Y/LSTICK/RSTICK/L/R/ZL/ZR/PLUS/MINUS/DLEFT/DUP/DDOWN/DRIGHT/HOME/CAPTURE
#release A/B/X/Y/LSTICK/RSTICK/L/R/ZL/ZR/PLUS/MINUS/DLEFT/DUP/DDOWN/DRIGHT/HOME/CAPTURE

#peek <address in hex, prefaced by 0x> <amount of bytes, dec or hex with 0x>
#poke <address in hex, prefaced by 0x> <data, if in hex prefaced with 0x>

#setStick LEFT/RIGHT <xVal from -0x8000 to 0x7FFF> <yVal from -0x8000 to 0x7FFF
#sys-botbase developed by olliz0r
#usb support added by fishguy6564
#This script is an adaptation of the example script originally distributed with the original sys-botbase
#Noteable differences are that a new function to read data was created to make it simpler for the end user called readData()
#sendCommand now only needs one string argument instead of the socket and string command.
#A buffer between reading data and executing the next command isn't needed as the usb interface handles that itself
#The necessary escape characters needed for the parser are handled by the sys-module instead of the script now
#Dependencies are listed on github

import binascii
import struct
import time
import usb.core
import usb.util

global_dev = None
global_out = None
global_in = None

#Sends string commands to the switch
def sendCommand(content):
    global_out.write(struct.pack("<I", (len(content)+2)))
    global_out.write(content)

def readData():
	size = int(struct.unpack("<L", global_in.read(4, timeout=0).tobytes())[0])

	#Converts received data to integer array
	data = [0] * size
	x = global_in.read(size, timeout=0).tobytes()
	for i in range(size):
		data[i] = int(x[i])
	return data

#Using method from Goldleaf
def connect_switch():
    global global_dev
    global global_out
    global global_in
    global_dev = usb.core.find(idVendor=0x057E, idProduct=0x3000)
    if global_dev is not None:
        try:
            global_dev.set_configuration()
            intf = global_dev.get_active_configuration()[(0,0)]
            global_out = usb.util.find_descriptor(intf,custom_match=lambda e:usb.util.endpoint_direction(e.bEndpointAddress)==usb.util.ENDPOINT_OUT)
            global_in = usb.util.find_descriptor(intf,custom_match=lambda e:usb.util.endpoint_direction(e.bEndpointAddress)==usb.util.ENDPOINT_IN)
            return True
        except:
            return False
            pass
    else:
        return False

#To communicate with the user
def attemptConnection():
	isConnected = False
	while not isConnected:
		if connect_switch():
			print("Connected to Switch Successfully!")
			isConnected = True
		else:
			print("Failed to Connect to Switch!")
			print("Attempting to Reconnect in 5 Seconds...")
			time.sleep(5)


def main():
	attemptConnection()

	fileIn = open("toInject.ek8", "rb")
	pokemonToInject = fileIn.read(344)
	pokemonToInject = str(binascii.hexlify(pokemonToInject), "utf-8")
	sendCommand(f"poke 0x4293D8B0 0x{pokemonToInject}") #read pokemon from file and inject it into box1slot1 for trade

	#Button commands to send
	sendCommand("click Y")
	time.sleep(1)
	sendCommand("click DDOWN")
	time.sleep(0.1)
	sendCommand("click A")
	time.sleep(2)
	sendCommand("click A")
	time.sleep(0.5)
	sendCommand("click A")
	time.sleep(4)

	sendCommand("click A")
	time.sleep(0.7)
	sendCommand("click A")
	time.sleep(0.7)
	sendCommand("click A")
	time.sleep(0.7)

	#Check last traded pokemon and dump ek8
	sendCommand("peek 0x4293D8B0 344") #get pokemon from box1slot1
	pokemonBytes = readData()

	fileOut = open("lastReceived.ek8", "wb")
	fileOut.write(bytes(pokemonBytes))
	fileOut.close()

main()