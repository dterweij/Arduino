import serial
import time
import io
import sys
import pymysql.cursors
import re
import datetime

print("Starting...\n")
connection = pymysql.connect(host='localhost',
							 user='USER',
							 password='PASSWORD',
							 db='temperatuur',
							 charset='utf8mb4',
							 cursorclass=pymysql.cursors.DictCursor)


ser = serial.Serial()
ser.port = "/dev/ttyUSB0"
ser.baudrate = 9600
ser.bytesize = serial.EIGHTBITS #number of bits per bytes
ser.parity = serial.PARITY_NONE #set parity check: no parity
ser.stopbits = serial.STOPBITS_ONE #number of stop bits
ser.timeout = 1            #non-block read
ser.xonxoff = False     #disable software flow control
ser.rtscts = False     #disable hardware (RTS/CTS) flow control
ser.dsrdtr = False       #disable hardware (DSR/DTR) flow control
ser.writeTimeout = 2     #timeout for write

try:
	ser.open()
except Exception, e:
	print "error open serial port: " + str(e)
	exit()

try:
	connection.cursor()
except Exception, e:
	print "error connecting mysql: " + str(e)
	exit()
#finally:
	#connection.close()


seq = []
count = 1
action = [0, 5, 10, 15, 20, 25, 30 ,35 ,40, 45, 50, 55]
trigger = 1
loopcheck = 0

print("Started. Logging data from arduino from USB to database.\n")

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
				print(str(count) + " cmd " + mylist[0])
				print(str(count) + " tmp " + mylist[1])
				print(str(count) + " ldr " + mylist[2])
				try:
					with connection.cursor() as cursor:
						# Create a new record
						sql = "UPDATE temperatuur.current SET value='" +  mylist[1] + "', logtime='" +  str(timestamp) + "' WHERE log_id='4'"
						cursor.execute(sql)
						connection.commit()
						print "Wrote Temperature to DB: " + mylist[1]
						for item in action:
							if now.minute == item and trigger == 1:
								loopcheck = item
								sql = "INSERT INTO temperatuur.solar (value, date) VALUES ('" + mylist[2] + "', '" + str(timestamp) + "')"
								cursor.execute(sql)
								connection.commit()
								print "Wrote Solar to DB: " + str(item) + " " + str(now.second)
								time.sleep(1)
								trigger = 0
						if now.minute == (loopcheck + 1) and trigger == 0:
							trigger = 1
				except Exception, e:
					print "error connecting mysql: " + str(e)
					exit()
			seq = []
			count += 1
			break


ser.close()
connection.close()
