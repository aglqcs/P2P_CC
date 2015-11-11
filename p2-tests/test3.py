#!/usr/bin/env python
import errno
import sys
import os
import time
import sys
import random
from subprocess import Popen, PIPE, STDOUT
import os.path
import resource
import subprocess
import re
import signal
from datetime import datetime

BIN = 'peer'
CHECKER = 'python cp1_checker.py'
StartTimeMs = -1

def download_concurrent():
    print '['+getElapsedTime()+'ms]: Test to check if the peer can download chunks concurrently from single peers'
    if not os.path.isfile(BIN):
        print '['+getElapsedTime()+'ms]: %s is not found' % (BIN)
    print '['+getElapsedTime()+'ms]: Trying to start hupsim.pl'
    cmd = 'perl hupsim.pl -m %s -n %s -p %s -v %s' % \
          ('topo_concurrent.map', 'nodes_concurrent.map', '15441', '0')
    print cmd
    pHupsim = Popen(cmd.split(' '))
    print '['+getElapsedTime()+'ms]: Wait 1 seconds.'
    time.sleep(1)
    print '['+getElapsedTime()+'ms]: Trying to start peer'
    cmd = './peer -p %s -c %s -f %s -m 4 -i 3 -d %s' % \
          ('nodes_concurrent.map', 'B.chunks', 'C.chunks', '0')
    print cmd
    pPeer = Popen(cmd.split(' '), stdin=PIPE)
    print '['+getElapsedTime()+'ms]: Wait 1 seconds.'
    time.sleep(1)
    print '['+getElapsedTime()+'ms]: Trying to start ref_peer_concurrent_1'
    cmd = './peer -p %s -c %s -f %s -m 4 -i 1 -d %s' % \
          ('nodes_concurrent.map', 'A.chunks', 'C.chunks', '0')
    print cmd
    pRefPeer1 = Popen(cmd.split(' '))
    print '['+getElapsedTime()+'ms]: Wait 1 seconds.'
    time.sleep(1)
    print '['+getElapsedTime()+'ms]: Trying to start ref_peer_concurrent_2'
    cmd = './peer -p %s -c %s -f %s -m 4 -i 2 -d %s' % \
          ('nodes_concurrent.map', 'A.chunks', 'C.chunks', '0')
    print cmd
    pRefPeer2 = Popen(cmd.split(' '))
    print '['+getElapsedTime()+'ms]: Wait 1 seconds.'
    time.sleep(1)
    pPeer.stdin.write('GET test1.chunks test1.tar\n')
    rc1 = pRefPeer1.wait()
    rc2 = pRefPeer2.wait()
    time.sleep(1)
    concurrentfile = 'concurrent-peer-jjc.txt'
    if not os.path.exists(concurrentfile):
        print('Concurrent file does not exist!')
    else:
        time1 = -1
        time2 = -1
        with open(concurrentfile) as f:
            for line in f:
                if int(line.split()[1]) == 1:
                    time1 = float(line.split()[2])
                if int(line.split()[1]) == 2:
                    time2 = float(line.split()[2])
        print('time1='+str(time1)+' time2='+str(time2))
        if abs(time1-time2) < 2:
            print "Concurrent Start: OK"
    pDiff = Popen(['diff', 'A.tar', 'test1.tar'])
    rc = pDiff.wait()
    if rc == 0:
        print '['+getElapsedTime()+'ms]: success: files are the same'
        print "Concurrent complete: OK"
    else:
        print '['+getElapsedTime()+'ms]: fail: files are different'
        print "Concurrent complete: FAILED"
    pPeer.kill()
    pHupsim.kill()

def init_stuff():
    os.environ['SPIFFY_ROUTER'] = '127.0.0.1:15441'
    print '['+getElapsedTime()+'ms]: SPIFFY_ROUTER is set to %s' % \
        (os.environ['SPIFFY_ROUTER'])
    killall = Popen(['sh','killall.sh'])
    rc = killall.wait()
    print '['+getElapsedTime()+'ms]: killall returns %d' % (rc)

def getElapsedTime():
    return str(int(round(time.time()*1000))-StartTimeMs)

if __name__ == '__main__':
    init_stuff()
    print '['+getElapsedTime()+'ms]: setting timeout to 280 sec'
    signal.alarm(280)
    StartTimeMs = int(round(time.time()*1000))
    download_concurrent()
