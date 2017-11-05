#
#
# server.py
# Read data from Arduino USB port, process and send to database
# Version 1.0 Nov 2017
# Use Python 2.7
#

import serial
import time
import io
import sys
import pymysql.cursors
import re
import datetime


class colors:
    '''Colors class:
    reset all colors with colors.reset
    two subclasses fg for foreground and bg for background.
    use as colors.subclass.colorname.
    i.e. colors.fg.red or colors.bg.green
    also, the generic bold, disable, underline, reverse, strikethrough,
    and invisible work with the main class
    i.e. colors.bold
    '''
    reset='\033[0m'
    bold='\033[01m'
    disable='\033[02m'
    underline='\033[04m'
    reverse='\033[07m'
    strikethrough='\033[09m'
    invisible='\033[08m'
    class fg:
        black='\033[30m'
        red='\033[31m'
        green='\033[32m'
        orange='\033[33m'
        blue='\033[34m'
        purple='\033[35m'
        cyan='\033[36m'
        lightgrey='\033[37m'
        darkgrey='\033[90m'
        lightred='\033[91m'
        lightgreen='\033[92m'
        yellow='\033[93m'
        lightblue='\033[94m'
        pink='\033[95m'
        lightcyan='\033[96m'
    class bg:
        black='\033[40m'
        red='\033[41m'
        green='\033[42m'
        orange='\033[43m'
        blue='\033[44m'
        purple='\033[45m'
        cyan='\033[46m'
        lightgrey='\033[47m'

print(colors.fg.green + colors.bold + "#" + colors.reset + colors.fg.cyan + " Starting..." + colors.reset)

connection = pymysql.connect(	host='localhost',
				user='USER',
				password='PASSWORD',
				db='temperatuur',
				charset='utf8mb4',
				cursorclass=pymysql.cursors.DictCursor	)

ser 			= serial.Serial()
ser.port 		= "/dev/ttyUSB0"
ser.baudrate 		= 9600
ser.bytesize 		= serial.EIGHTBITS		#number of bits per bytes
ser.parity 		= serial.PARITY_NONE		#set parity check: no parity
ser.stopbits 		= serial.STOPBITS_ONE		#number of stop bits
ser.timeout 		= 1 				#non-block read
ser.xonxoff 		= False     			#disable software flow control
ser.rtscts 		= False     			#disable hardware (RTS/CTS) flow control
ser.dsrdtr 		= False       			#disable hardware (DSR/DTR) flow control
ser.writeTimeout 	= 2     			#timeout for write

try:
	ser.open()
except Exception, e:
	print "*********** error open serial port: " + str(e)
	exit()

try:
	connection.cursor()
except Exception, e:
	print "*********** error connecting mysql: " + str(e)
	exit()

seq = []
count = 1
action = [0, 5, 10, 15, 20, 25, 30 ,35 ,40, 45, 50, 55]
trigger = 1
loopcheck = 0

print(colors.fg.green + colors.bold + "#" + colors.reset + colors.fg.cyan + " Started. Logging data from arduino from USB to database." + colors.reset)

while True:
	now = datetime.datetime.now()

	for c in ser.read():
		seq.append(c)
		joined_seq = ''.join(str(v) for v in seq) #Make a string from array

		if c == '\n':
			timestamp = int(time.time())
			#print("Line " + str(count) + ': ' + joined_seq)
			mylist = [x for x in re.compile('\s*[,|\s+]\s*').split(joined_seq)]

			if mylist[0] == 'T':
				print(colors.fg.purple + colors.bold + "# " + colors.reset + colors.fg.orange + str(count) + " cmd " + mylist[0] + colors.reset)
				print(colors.fg.purple + colors.bold + "# " + colors.reset + colors.fg.orange + str(count) + " tmp " + mylist[1] + colors.reset)
				print(colors.fg.purple + colors.bold + "# " + colors.reset + colors.fg.orange + str(count) + " ldr " + mylist[2] + colors.reset)
				try:
					with connection.cursor() as cursor:

						sql = "UPDATE temperatuur.current SET value='" +  mylist[1] + "', logtime='" +  str(timestamp) + "' WHERE log_id='4'"
						cursor.execute(sql)
						connection.commit()

						print colors.fg.purple + colors.bold + "# " + colors.reset + colors.fg.pink + "Wrote Temperature to current DB: " + mylist[1] + colors.reset

						for item in action:
							if now.minute == item and trigger == 1:
								loopcheck = item

								sql = "INSERT INTO temperatuur.solar (value, date) VALUES ('" + mylist[2] + "', '" + str(timestamp) + "')"
								cursor.execute(sql)
								connection.commit()
								print colors.fg.purple + colors.bold + "# " + colors.reset + colors.fg.yellow + "Wrote Solar to DB: " + str(item) + " " + str(now.second) + colors.reset

								sql = "INSERT INTO temperatuur.readings (logtime, log_id, value) VALUES ('" + str(timestamp) + "', '4', '" + mylist[1] + "')"
								cursor.execute(sql)
								connection.commit()
								print colors.fg.purple + colors.bold + "# " + colors.reset + colors.fg.yellow + "Wrote Temperature to readings DB: " + str(item) + " " + str(now.second) + colors.reset

								time.sleep(1)
								trigger = 0

						if now.minute == (loopcheck + 1) and trigger == 0:
							trigger = 1

				except Exception, e:
					print "************ error connecting mysql: " + str(e)
					exit()
			seq = []
			count += 1
			break


ser.close()
connection.close()
