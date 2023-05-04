#!/usr/bin/env python3

# I'm too lazy to do a clicky-clicky GUI for this tool, so just edit the lines below
# to configure what you want to see on the screen.

PLOT_SOURCES = { "helper", "main" }
PLOT_SIGNALS = { "y", "err", "phase_current", "phase_target" }
PLOT_EVENTS = True

import sys
from matplotlib import animation
from matplotlib import pyplot as plt
import threading
import logging

class SPLLTrace:
    def __init__(self):
        self.data = {}
        self.events = []

    def parse( self, tokens ):
        for t in tokens:
            #print(t)
            try:
                (name, value_str) = t.split('=')
            except ValueError:
                continue

            if name == "event":
                self.events.append( ( value_str, self.data["sample_id"][-1] ) )
            if not name in self.data:
                self.data[name] = []

            self.data[name].append( int( value_str ) )

            #print(name, value_str)

def acquisition_thread(args):
    logging.info("ACQ Thread: starting")
    while True:
        l = sys.stdin.readline().rstrip('\n').rstrip('\r').lstrip(' ').lstrip('\t').rstrip(' ')
        t = l.split(' ')
        if(len(t) < 1):
            continue
        src_id = t[0]
        if src_id in traces:
            if traces[src_id] == None:
                traces[src_id] = SPLLTrace()

            traces[src_id].parse( t[1:] )
    logging.info("ACQ Thread: quitting")

traces = {
    "helper" : None,
    "main" : None,
    "ext" : None,
    "aux0" : None,
    "aux1" : None,
    "aux2" : None,
    "aux3" : None
};

def animated_refresh(args):
    ax.clear()

    for src in traces.keys():
        if not src in PLOT_SOURCES:
            continue
        trace = traces[src]
        if trace == None:
            continue
        for signal in trace.data.keys():
            if not signal in PLOT_SIGNALS:
                continue
            #print("Plot ", src, signal )
            print(trace.data["sample"]  )
            ax.plot( trace.data["sample"], trace.data[signal], label="%s:%s" % ( src, signal ) )
            plt.legend()


acq_thread = threading.Thread( target=acquisition_thread )
acq_thread.start()

fig, ax = plt.subplots( 1, 1 )
anim = animation.FuncAnimation(fig, animated_refresh, interval=1000)
plt.title("SoftPLL debug")
plt.xlabel("Time [samples]")
plt.ylabel("Trace value [arbitrary units]")
plt.show()

acq_thread.join()