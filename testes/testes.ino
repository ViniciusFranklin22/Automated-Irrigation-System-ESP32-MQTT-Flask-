/**
 * DHT11 Sensor Reader
 * This sketch reads temperature and humidity data from the DHT11 sensor and prints the values to the serial port.
 * It also handles potential error states that might occur during reading.
 *
 * Author: Dhruba Saha
 * Version: 2.1.0
 * License: MIT
 */

// Include the DHT11 library for interfacing with the sensor.
#include <DHT11.h>
#define LIGHT_SENSOR_PIN 36 // ESP32 pin GIOP36 (ADC0)
#define SOIL_SENSOR_PIN 39
#define RelePin  4 // pino ao qual o Módulo Relé está conectado


// Create an instance of the DHT11 class.
// - For Arduino: Connect the sensor to Digital I/O Pin 2.
// - For ESP32: Connect the sensor to pin GPIO2 or P2.
// - For ESP8266: Connect the sensor to GPIO2 or D4.
DHT11 dht11(2);

void setup() {
    // Initialize serial communication to allow debugging and data readout.
    // Using a baud rate of 9600 bps.
    Serial.begin(9600);
    pinMode(RelePin, OUTPUT); // seta o pino como saída
    digitalWrite(RelePin, LOW); // seta o pino com nivel logico baixo

    // Uncomment the line below to set a custom delay between sensor readings (in milliseconds).
    analogSetAttenuation(ADC_11db);

    dht11.setDelay(1000); // Set this to the desired delay. Default is 500ms.
}

void loop() {
    int temperature = 0;
    int humidity = 0;

    // Attempt to read the temperature and humidity values from the DHT11 sensor.
    int result = dht11.readTemperatureHumidity(temperature, humidity);

    // Check the results of the readings.
    // If the reading is successful, print the temperature and humidity values.
    // If there are errors, print the appropriate error messages.

    int analogValue = analogRead(LIGHT_SENSOR_PIN);
    Serial.print("Luminosidade: \t");
    Serial.print(analogValue);

    int analogValue_2 = analogRead(SOIL_SENSOR_PIN);
    Serial.print("| Solo Humidade: \t");
    Serial.print(analogValue_2);

    if (analogValue < 600){
      digitalWrite(RelePin, HIGH);
    }
    else{
      digitalWrite(RelePin, LOW);
    }
    if (result == 0) {
        Serial.print("| Temperature: ");
        Serial.print(temperature);
        Serial.print(" °C\t| Humidity: ");
        Serial.print(humidity);
        Serial.println(" %");
    } else {
        // Print error message based on the error code.
        Serial.println(DHT11::getErrorString(result));
    }
}