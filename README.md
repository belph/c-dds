DDS C Rewrite
===================
*Note: This document is subject to change. Check periodically for updates*
Contents
---------

1. <a href="#intro">Introduction</a>
2. <a href="#clientOutline">Client-Side Process Outline</a>
3. <a href="#clientDiagram">Client-Side Process Diagram and Flowcharts</a>
  1. <a href="#processLayout">Client Process Layout</a>
  2. <a href="#pluginLoading">Client Python Plugin Loading</a>
  3. <a href="#pluginMsg">Client Python Plugin Message Handling</a>

<a id="intro" name="intro"></a>Introduction
------------
Why? Four reasons:

1. Maximum efficiency
2. Learn some about Unix IPC and C Socket I/O
3. Learn some about C-Python Intercommunication
4. We can say we wrote production-grade software in pure C (Dennis Ritchie would be proud)

Reasoning aside, this document is more or less an outline of how this is all going to (as of the planning stage) going to work. 

*Note: If you're in Crew and wanting to contribute, I __strongly__ recommend reading [Beej's Guide to Unix IPC](http://beej.us/guide/bgipc/output/html/multipage/index.html "Beej's Guide to Unix IPC Index") (pay special attention to his sections on [pipes](http://beej.us/guide/bgipc/output/html/multipage/pipes.html "Beej's Guide to Unix IPC: Pipes"), [FIFOs](http://beej.us/guide/bgipc/output/html/multipage/fifos.html "Beej's Guide to Unix IPC: FIFOs"), and [message queues](http://beej.us/guide/bgipc/output/html/multipage/mq.html "Beej's Guide to Unix IPC: Message Queues")) and then his [Guide to Network Programming](http://beej.us/guide/bgnet/output/html/multipage/index.html "Beej's Guide to Network Programming Index"). You may want to take a look at this [guide on POSIX Threading](https://computing.llnl.gov/tutorials/pthreads/) as well (we aren't implementing this last one yet, but it's worth knowing).* 

*Seem overwhelming? That's alright. Philip and Neil know[citation needed] enough about this to help anyone wanting to assist get up to speed. Don't stress too much about it; this is supposed to be a learning experience for everyone.*

The plan is to re-implement the Python dds-client [repo](https://github.com/crew/dds-client)'s functionality in C. We will be doing this in two phases, creatively titled Phase 1 and Phase 2. Phase 1 simply aims to get something that works, and Phase 2 will both explore subtle performance improvements and re-implement the DDS Python Plugin API.

<a id="clientOutline" name="clientOutline"></a>Client-Side Process Outline
--------------------------------

Alright, so if we're going for efficiency anywhere, it's on the client end. Why? [Reasons](http://en.wikipedia.org/wiki/Raspberry_Pi#Specifications). So, here's our working model of what I (i.e. Philip, who is writing this) think will be the best approach. There is a nonzero probability of me being wrong; that is, if there is any performance improvement to this model, or in the even more likely event that something I've written just plain will not work, *please* let me know.

Now, on to the actual outline.

The current Python Client has (I may have forgotten one or two, but these are the important ones) the following three threads, which will also be present in the C rewrite, more or less functionally the same:

- GTK Display Thread (`gtk.main()`) *Note: This functions as our main thread in the Python version*
- Slide Show Logic (Handles the list of all slides and tells the GTK thread when to show them)
- Socket I/O Handling Thread

Now, one thing that will be different from the Python behavior of these threads is that, instead of being *threads*, they will actually be (in theory) separate processes forked from the main binary which communicate using the message queue functionality provided by `<sys/msg.h>`. Once we have a working implementation of that, we may look into any performance gains from using FIFOs instead during Phase 2.

Then, during Phase 2, we will be writing a Python Plugin API. Fair warning: this probably won't be easy, since none of us have ever done anything with this. On the other hand, however, it will be really cool once it's done. Essentially, the goal is to create a Python Plugin Class which can interface with the C rewrite.

<a id="clientDiagram" name="clientDiagram"></a>Client-Side Process Diagram and Flowcharts
-----------------------------------------------

###<a id="processLayout" name="processLayout"></a>Process Layout
![Process layout diagram](http://i.imgur.com/ECU6H1V.png)

###<a id="pluginLoading" name="pluginLoading"></a>Plugin Loading
![Plugin loading flowchart](http://i.imgur.com/RDdSQOV.png)

###<a id="pluginMsg" name="pluginMsg"></a>Plugin Message Handling
![Plugin message handling flowchart](http://i.imgur.com/rIHJ7RZ.png)
