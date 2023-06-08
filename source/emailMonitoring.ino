#include <Arduino.h>
#include <WiFi.h>
#include <ESP_Mail_Client.h>

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

Adafruit_BME280 bme; 

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep for (in seconds) */
#define MIN_TIME_BETWEEN_EMAILS 3600 /* The time for which the device will deep sleep after sending an email (in seconds) */
#define preheatTime 30000 //mq2 sensor preheat time
#define digitsAfterDecimalPoint 2
#define numberOfReadingsToAverage 5
#define dataCount 4

//defines that shouldn't be public
#include "credentials.h"

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465


/* The SMTP Session object used for Email sending */
SMTPSession smtp;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);

#include <MQUnifiedsensor.h>

#define         Board                   ("ESP-32") // Wemos ESP-32 or other board, whatever have ESP32 core.

//mq 2 sensor data and creation
#define         Pin                     (36) 
#define         Type                    ("MQ-2") //MQ2 or other MQ Sensor, if change this verify your a and b values.
#define         Voltage_Resolution      (3.3) // 3V3 <- IMPORTANT. Source: https://randomnerdtutorials.com/esp32-adc-analog-read-arduino-ide/
#define         ADC_Bit_Resolution      (12) // ESP-32 bit resolution. Source: https://randomnerdtutorials.com/esp32-adc-analog-read-arduino-ide/
#define         regressionMethod        (1)
#define         A_value                 (36974)
#define         B_value                 (-3.109)
#define         R0_value                (1.28)
MQUnifiedsensor MQ2(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin, Type);

void setup()
{
  // Set time to sleep between measurments
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  
  // Initialize serial communication for debugging
  Serial.begin(115200);
  Serial.println();
  
  // Setting up the temperature /humidity /pressure sensor
  Wire.begin(16,17);
  bme.begin(0x76);
  
  // Start preheating MQ2
  delay(preheatTime);

  // Set math model to calculate the PPM concentration and the value of constants
  MQ2.setRegressionMethod(regressionMethod); //_PPM =  a*ratio^b
  MQ2.setA(A_value); MQ2.setB(B_value); // Configure the equation to to calculate CO concentration
  MQ2.setR0(R0_value);// Obtained after calibration routine
  MQ2.init();
}

void connectWiFi()
{
  Serial.print("Connecting to AP");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(200);
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

void sendMail(String subjectEmailField, String htmlMsg)
{
  /** Enable the debug via Serial port
   * none debug or 0
   * basic debug or 1
  */
  smtp.debug(1);

  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);

  /* Declare the session config data */
  ESP_Mail_Session session;

  /* Set the session config */
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "";

  /* Declare the message class */
  SMTP_Message message;

  /* Set the message headers */
  message.sender.name = "ESP32";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = subjectEmailField;
  message.addRecipient("Me", RECIPIENT_EMAIL);

  /*Send HTML message*/
  message.html.content = htmlMsg.c_str();
  message.text.charSet = "us-ascii";
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  /* Connect to server with the session config */
  if (!smtp.connect(&session))
    return;

  /* Start sending Email and close the session */
  if (!MailClient.sendMail(&smtp, &message))
    Serial.println("Error sending Email, " + smtp.errorReason());
}

void loop()
{
  float toBeAveraged[ numberOfReadingsToAverage ][ dataCount ];
  for(int i = 0; i < numberOfReadingsToAverage ; i++)
  {
    Serial.println("starting");
  
    MQ2.update(); // Update data, the device will read the voltage from the analog pin
    Serial.print(MQ2.readSensor()); // Sensor will read PPM concentration using the model
    Serial.println(" PPM");

    toBeAveraged[i][0] = bme.readTemperature();
    toBeAveraged[i][1] = bme.readHumidity();
    toBeAveraged[i][2] = bme.readPressure()/ 100.0F;
    toBeAveraged[i][3] = MQ2.readSensor();

    Serial.println("finishing");
    printValues();
    esp_light_sleep_start();
  }

  float averageValues[ dataCount ];
  for(int i = 0; i < numberOfReadingsToAverage; i++)
  { 
    for(int j = 0; j < dataCount; j++)
    {
      if(i == 0)
      {
        averageValues[j] = 0;
      }
      averageValues[j] += (toBeAveraged[i][j] / numberOfReadingsToAverage);  
    }
  }

  connectWiFi();
  sendMail("general report", createEmailMessage( averageValues[0], averageValues[1], averageValues[2], averageValues[3]) );
  WiFi.disconnect(true, true);

  esp_sleep_enable_timer_wakeup(MIN_TIME_BETWEEN_EMAILS * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

String createEmailMessage(float temperature, float humidity, float pressure, float COconcentration)
{
  return String( "<div style=\"color:#66CC66;\"><h1>Temperature: " + 
         String(temperature, digitsAfterDecimalPoint) + " °C</h1>\n<h1>Humidity: " + 
         String(humidity, digitsAfterDecimalPoint) + " % </h1>\n<h1>Pressure: " + 
         String(pressure, digitsAfterDecimalPoint) + " hPa</h1>\n<h1>CO reading: " + 
         String(COconcentration, digitsAfterDecimalPoint) + " PPM</h1></div>" );
}

void printValues() 
{
    Serial.print("Temperature = ");
    Serial.print(bme.readTemperature());
    Serial.println(" °C");

    Serial.print("Pressure = ");

    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(" hPa");


    Serial.print("Humidity = ");
    Serial.print(bme.readHumidity());
    Serial.println(" %");

    Serial.println();
}


/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status)
{
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success()){
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++){
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");
  }
}
