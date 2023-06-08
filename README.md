# ESP32 project
project for IoT course

MQ-2 sensor calibrated(not in a laboratory setting) for CO concentration and
bme280 temperature/pressure/humidity sensor collect data, which is then averaged
and sent by email.

After getting a number of data points( ligth sleeping in between each reading ),
an email is sent with the averaged values and then the esp enters deep sleep for a longer period.
