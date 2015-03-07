RFM69 Library extension to implement transmission session keys to prevent replay attacks.

Based off of Felix Rusu's RFM69 library: https://github.com/LowPowerLab/RFM69
Which was modified for extensibility by Thomas Studwell: https://github.com/TomWS1/RFM69/tree/virtualized

Requires Thomas' modifications plus some additional modifications by me available here: https://github.com/dewoodruff/RFM69/tree/virtualized

Session management is enabled transparent to the sketch by running radio.useSessionKey(1) during setup.