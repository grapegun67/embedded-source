import time

while True:
    f = open("/dev/sonic_device",'r')
    t = f.readline()    #microseconds
    f.close()

    if (long(t) == -1):
        t = -1   #N/A
    else:
        t = (float(t)/58)  

    print("result: %.6f" % t)

    time.sleep(1)
