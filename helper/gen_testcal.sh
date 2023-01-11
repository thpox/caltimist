#!/bin/sh

cat <<EOH
BEGIN:VCALENDAR
VERSION:2.0
X-WR-CALNAME:caltimist
X-WR-CALDESC:calendar for caltimist
EOH

for y in $(seq 2020 2023); do
for m in $(seq -w 1 12); do
for d in $(seq -w 1 20); do
for h in 10 14 ; do
    BE="${y}${m}${d}T${h}30"
    EE="${y}${m}${d}T$((h+3))00"

cat <<EOE
BEGIN:VEVENT
DTSTART:${BE}00Z
DTEND:${EE}00Z
DTSTAMP:${BE}12Z
UID:${BE}@testsystem
CREATED:${BE}34Z
LAST-MODIFIED:${BE}56Z
SUMMARY:projectX
CLASS:PUBLIC
STATUS:CONFIRMED
TRANSP:OPAQUE
BEGIN:VALARM
ACTION:DISPLAY
DESCRIPTION:projectX
TRIGGER;VALUE=DURATION:-PT30M
END:VALARM
END:VEVENT
EOE

done
done

for d in $(seq -w 26 27); do
    BV="${y}${m}${d}"
    EV="${y}${m}$((d+1))"

cat <<EOV
BEGIN:VEVENT
DTSTART;VALUE=DATE:${BV}
DTEND;VALUE=DATE:${EV}
X-FUNAMBOL-ALLDAY:1
DTSTAMP:${BV}T123456Z
UID:${BV}@testsystem
CREATED:${BV}T123456Z
LAST-MODIFIED:${BV}T123456Z
SUMMARY:vacation (caltimist-test)
CLASS:PUBLIC
STATUS:CONFIRMED
TRANSP:OPAQUE
BEGIN:VALARM
ACTION:DISPLAY
DESCRIPTION:vacation (caltimist-test)
TRIGGER;VALUE=DURATION:-PT30M
END:VALARM
END:VEVENT
EOV
done

done
done

echo "END:VCALENDAR"
exit
