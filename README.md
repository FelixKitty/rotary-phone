# Rotary Phone on Cell Network #

Converts an old rotary telephone to work on the GSM mobile phone network.

Listens to the position of the handset and when it's lifted, plays dialtone and waits for the rotary dial to spin. Decodes rotary dial into numbers and calls the number using the cellphone network.

Uses Adafruit's FONA board for the phone stuff. Triggers a tiny solenoid to bounce back and forwards at the proper frequency to ring the bells of an old-style phone and make it sound realistic. 
