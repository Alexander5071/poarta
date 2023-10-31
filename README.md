# Automatic Gate Control through GSM calling and SMS commands

This is a basic implementation of an automatic control module, composed of an Arduino that communicates with an SIM800L GSM module via the AT commands language, that is responsible for opening an access gate for people and/or vehicles.

The authentication of users is based on the number that is calling/sending SMS messages to the GSM module, whitelisted numbers being stored on the installed SIM card. For faster access to this data, it is also loaded in memory on the Arduino upon bootup, SIM storage being used only for persistence.

Due to the unreliabile nature of the SIM800L module being used when not powered from a Li-Ion battery and rather from a DC power source, the program implements a plethora of safeguards and routines that are destined to help recover the module from a variety of lock-ups and/or errors and, if these preventionary steps fail, completely powers off and reinitializes the GSM module.

The program was written with Romanian formatted cellphone numbers in mind, but, by using the appropriate country code when storing the whitelist on the SIM card, it should function with international numbers.

The SoftwareSerial library used in this project is obtained from the official Arduino site: \
https://docs.arduino.cc/learn/built-in-libraries/software-serial
