# caltimist - calculates project-/worktime and vacation using iCalendar data

## Features

caltimist
* fetches iCalendar data per user and calculates the worktime
* takes "all day events" as vacation
* can use a public holiday calendar to calculate the _real_ amount of vacation days taken by skipping both weekend and public holidays as well as duplicate/overlapping calendar entries
* supports both common global as well as individual basic auth credentials for HTTP(s) requests
* does both HTTP and HTTPS for getting the iCalendars
* can filter the entries into projects based on the SUMMARY field of the event
* calculates project time being spent "on-site" (if location field is set) or remotely (else)
* can do some price calculation if project has price-tags for remote and onsite work
* supports different output formats (easy to extend)
* can be used on console and as a CGI (supporting both GET and POST requests)

## HowTo

### Build

Caltimist was written using [libowfat](https://www.fefe.de/libowfat/). So you have to have it available on your build system.

On a Debian system it is sufficient to have libowfat-dev and maybe even libowfat-dietlibc-dev installed.

### Configure 

The setup is pretty simple. Run make to compile the program.
Get an iCalendar that is accessible via HTTP(s).
Create a config file and run the binary.

The config file is named .caltimistrc and has the following structure:
```
[General]
user=calusr
password=pAssw0rd
public_holidays=http://localhost/static/pubhol.ics

[User]
{foo}
cal = https://foousr:barPaSs@some.site/path/to/foocal.ics
vacation = 30
monthhours = 168

{jack}
cal = http://localhost/path/to/jackcal.ics
vacation = 20
monthhours = 35

[Projects]
{housekeeping}

{projectX}
onsite = 12.34
remote = 45.67
```

### CGI

If called with suffix _.cgi_, caltimist acts as a CGI. 
In this mode, it only reads the arguments `y` (for year) and `m` (for month)
from the query-string and renders the output as HTML table.
Furthermore, it gets the user via the REMOTE_USER environment variable, so only
authenticated users can view their own data.
All other arguments are ignored in this mode.

## Resource Usage

### Binary 

When compiled with SSL support and as a dynamically linked binary, caltimist is
only 48kB in size.
Skipping the SSL support (e.g. because calendars can be fetched locally) and
compiling it statically using [diet libc](https://www.fefe.de/dietlibc/), it is 72kB *big*.

### Runtime 

In my test setup, I used a public-holiday calendar for my region containing 161
events (3568 lines, 100kB) as well as a user calendar with 2016 entries (34373
lines, 672kB).
Fetching both calendars (via http), parsing them and generating the statistics
took 70ms and had a maximum resident set size of about 600kB.

## License

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.
