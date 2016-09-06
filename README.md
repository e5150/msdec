# Mode S decoder
This is a set of three programs: `msdec`, `msgui` and `rtl-modes`.

# msdec
The primary program of the suite, that tries to decode messages.
It can be invoked either as `msdec -m <message [...]>` or `msdec [options] <file>`.
The preferred input format is:
````
DF##:<unix-time>:<hex-data>:<hex-addr>
##        = The downlink format, as two decimal digits.
unix-time = Number of seconds since midnight 1970-01-01, in decimal.
hex-data  = 56 or 112 bit hexadecimal string representing the message data.
hex-addr  = 24 bit address of the emitting aircraft.
````
Thou it isn’t too picky, any of the fields, except `hex-data`, can be omitted. But
without a time-stamp it can only make sense of real-time data when e.g. decoding
CPR locations. The address will be derived from the `AP` or `PI` message fields if missing.
Likewise, the `DF` field is derived from the first 5 bits of the message, regardless of the input.
It also understands messages as output by dump1090, with asterisk and semi-colon.

`msdec` has the following command line options:
* `-f` follow input file as it grows, akin to `tail -f`
* `-nm` omit message output, useful when using the dump options.
* `-ns` omit statistics output.
* `-s` dump statistics to file (defaults to temporary file).
* `-S file` use given file for stats dump.
* `-h incr` dump histogram of input data, where incr is one
        of `m`, `q`, `h`, `6`, `d` and `w` for increments of
        1 minute, 15 minutes, 1 hour, 6 hours, 1 day and 1 week, respectively.
* `-H file` filename for histogram, otherwise temporary file.
* `-al` dump aircraft logs, one file per aircraft, named `CC3:0xADDR.log`
        (three char iso country code and aircraft hex-ICAO address) in
        a temporary directory under `/tmp`, or dir given by `-A dir`.
        No more than one line per second will be printed, and fields for which
        no data is available from that second will be represented as `-`.
        The log format is as follows:
````
TIME ID ALT VEL HEAD LAT LON
TIME = strftime(%Y-%m-%d %H:%M:%S)
ID   = four digit octal squawk code
ALT  = altitude in feet
VEL  = speed in kt
HEAD = heading, degrees clockwise from north
LAT  = latitude, four decimals, degrees
LON  = longitude, four decimals, degrees
````
* `-am` dump aircraft messages, one file per aircraft named `CC3:0xADDR.msg`, to
        same directory as aircraft logs.
* `-A dir` directory for aircraft dumps
* `-r` output raw messages, sample output:
````
recv:2016-09-04 18:00:07
data:8D47BB8799948BB1880483B9CFB7
hash:B9CFB7:B9CFB7:000000
addr:47BB87:NOR:SAS013
DF17:Extended squitter
Bit(s)  Field                     Binary   Decimal     Hex
  1-  5 DF                         10001        17      11
  6-  8 CA                           101         5       5
  9- 32 AA      010001111011101110000111   4701063  47BB87
 33- 88 Extended squitter
  1-  5 FTC                        10011        19      13
  6-  8 SUB                          001         1       1
  9     ICF                            1         1       1
 10     IFR                            0         0       0
 11- 13 NAC_v                        010         2       2
 14     E/W                            1         1       1
 15- 24 E/W-v                 0010001011       139      8B
 25     N/S                            1         1       1
 26- 35 N/S-v                 0110001100       396     18C
 36     VR/SRC                         0         0       0
 37     VR±                           1         1       1
 38- 46 VR                     000000001         1       1
 49     GNSS±                         1         1       1
 50- 56 GNSS                     0000011         3       3
        End of subfield
 89-112 PI      101110011100111110110111  12177335  B9CFB7
Bit(s)  Field   └──┴┴──┘└──┴┴──┘└──┴┴──┘
---
````
Standard (non-raw) output of the same message would be:
````
recv:2016-09-06 21:10:53
data:8D47BB8799948BB1880483B9CFB7
hash:B9CFB7:B9CFB7:000000
addr:47BB87:NOR
DF17:Extended squitter
CA=5:Capability:Level 2+. Airborne
Address announced:47BB87:Norway
BDS:0,9:Airborne velocity
Velocity over ground
ICF=1:Change in intent:Yes
IFR=0:ADS-B class A1+ capability:No
NAC_v=2:Navigational accuracy:HFOM_R < 3 m/s ∧ VFOM_R < 4.57 m/s
NS=1,018C:Southwards 395 kt
EW=1,008B:Westwards 138 kt
Velocity:418.4 kt (775 km/h):199°15′27.84″ (SSW)
VR=0,1,1:GNSS:Rate of descent = 0 ± 32 ft/min
GBD=1,3:GNSS = 50 ± 12.5 ft below barometric altitude
````


# rtl-modes

`rtl-modes` is a stripped down version of
[dump1090-mutability](https://github.com/mutability/dump1090).
It reads data from an RTL-SDR device at 1090 MHz, sampling at 2.4 MHz,
and print (seemingly valid) messages to stdout, using the same format
as described above as the preferred format for `msdec`.


# msgui

`msgui` is a GTK2 application for plotting the trails of aircrafts. See 
[screenshot.png](https://raw.githubusercontent.com/e5150/msdec/master/screenshot.png)


# Compilation
Copy `config.def.h` to `config.h` and edit it, and possibly `mk.config`, to your needs, and run `make`.


# License
These programs are © Lars Lindqvist and they’re available under the terms of GPLv3.
The mapping part of the gui, `map.c` and `map.h`, is based on
[osm-gps-map](https://nzjrs.github.io/osm-gps-map/) (GPLv2+)
And `rtl-modes.c` is based on
[dump1090-mutability](https://github.com/mutability/dump1090) (GPLv2+), which in turn
is based on MalcomRobb’s dump1090 (BSD-ish license).
See individual files for detailed copyright.


# Bibliography

* [1] ICAO Annex 10 Volume IV Aeronautical Telecommunications,
        Surveillance and Collision Avoidance Systems, 4th ed, 2007-07
* [2] ESC-TR-2006-082, Project Report ATC-334, Guidance Material for
        Mode S-Specific Protocol Application Avionics, Grappel & Wiken,
        Lincoln Laboratory, MIT, Lexington, MA, 2007-06-04
* [3] ICAO Doc 9871 AN/464 Technical Provision for Mode S Services
        and Extended Squitter, 1st ed, 2008
* [4] ICAO Annex 10 Volume III Aeronautical Telecommunications,
        Communication Systems, 2nd ed, 2007-07
* [5] 1090-WP-9-13, RTCA Special Committee 186, Working group 3,
        ADS-B 1090 MOPS, Rev. A, Meeting #9 ‐ 1090 MOPS and DF = 19
