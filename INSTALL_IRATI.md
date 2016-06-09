Make sure IRATI is installed, userspace should be built with the C
bindings enabled (they are by default).

For the game files, you will need to either install openarena or
extract the baseoa folder to a known location.

to build:
```
>STANDALONE=1 make
```
the game binaries will be in the build/release-linux-x86_64 folder (or
similar if on 32 bit arch).

to run the server:
```
>LD_LIBRARY_PATH=/usr/local/irati/lib ./ioq3ded.x86_64 +set com_basegame baseoa +set com_homepath PATH +map MAP
```

the run the client

```
LD_LIBRARY_PATH=/usr/local/irati/lib ./ioquake3.x86_64 +set com_basegame baseoa +set com_homepath PATH
```

the PATH variable should lead to the directory that contains the
baseoa folder from openarena. If you place it in your homefolder this
can be ommitted.

to connect from the client to the server, open a console using tilde
(~) and type

```
connect -R <ignored>
```

You can type anything for <ignored>, but the interface needs a third
parameter.  You can connect with IP and RINA simultaneously into the
same game, to connect over IPv4, open the console and type:

```
connect -4 <ip.ad.d.r>
```

Happy fragging!

Sander Vrijders   <sander.vrijders@intec.ugent.be>
Dimitri Staessens <dimitri.staessens@intec.ugent.be>
