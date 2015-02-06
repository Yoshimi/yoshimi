For yoshimi version 1.2.1 or later

You need version 1.3.2 or later for root changes to work

RootBankProgChangeTest


For versions before 1.3.2
    Expects 'Bank Change' set to LSB and 'Enable On Load' to be checked.
    Also you need the standard 'yoshimi/banks' supplied with yoshimi as your default bank root dir

For versions 1.3.2 or later
    Expects 'Bank Root Change' set to 0, 'Bank Change' set to LSB and 'Enable On Load' to be checked.
    Also a root dir with ID 9 containing the standard 'yoshimi/banks' supplied with yoshimi.

Otherwise you would need to change the root, bank and program numbers. 


Mastersynth High is chosen for the slow pads to really stress test the changes.

There are a couple of deliberately invalid changes.

Rosegarden version lets you see exactly where the changes take place.
