#!/bin/bash

# This script will un-register all services that are registered with
# the SDP server in bluez.

for svc in `sdptool browse local | grep RecHandle | cut -d " " -f 3`; do
    echo "Unregistering service $svc"
    sdptool del $svc
done

# Now, make sure the adapter is visible:

hciconfig hci0 piscan

# Now, configure automatic pairing:
# (NO!) http://stackoverflow.com/questions/12888589/linux-command-line-howto-accept-pairing-for-bluetooth-device-without-pin
# http://www.linuxquestions.org/questions/linux-wireless-networking-41/setting-up-bluez-with-a-passkey-pin-to-be-used-as-headset-for-iphone-816003/
# Need to set bluetooth class. 0x000900 is Health major device class; minor device class unknown
# Class calculator: http://www.ampedrftech.com/cod.htm

hciconfig hci0 class 0x000900


