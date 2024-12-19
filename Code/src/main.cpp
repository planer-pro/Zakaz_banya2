#include <Arduino.h>
#include <SoftwareSerial.h>
#include <iarduino_Modbus.h>
#include <LiquidCrystal_I2C.h>

#define TERM_ADRESS 0 // 4 more accuracy
#define HUM_ADRESS 1  // 5 more accuracy

#define SENS_COUNT 3

#define TEMP_MIN -10
#define TEMP_MAX 80
#define HUM_MIN 5
#define HUM_MAX 95

#define TIME_REQ 5000

#define INTRO_ON true
#define BUZZER_ON true
#define HEATER_ON true
#define DEBUG_SERIAL_ON true

// Graph class-------------------------------------------------------------------
class Graph
{
public:
    Graph(LiquidCrystal_I2C lcd);

    void createGraph(uint8_t posX, uint8_t posY, uint8_t width, int min, int max, int value);

private:
    LiquidCrystal_I2C _lcd;
    uint8_t graphChars[20] = {0};
};

Graph::Graph(LiquidCrystal_I2C lcd)
    : _lcd(lcd)
{
}

void Graph::createGraph(uint8_t posX, uint8_t posY, uint8_t width, int min, int max, int value)
{
    for (int i = width - 1; i > 0; i--)
    {
        graphChars[i] = graphChars[i - 1];
    }

    uint8_t d = map(value, min, max, 0, 7);
    graphChars[0] = d;

    _lcd.setCursor(posX, posY);

    for (int i = width - 1; i >= 0; i--)
        _lcd.write(graphChars[i]);
}
//--------------------------------------------------------------------------------

SoftwareSerial rs485(8, 9);    // RX, TX.
ModbusClient modbus(rs485, 2); // RS485, DE/RE pin
LiquidCrystal_I2C lcd(0x27, 20, 4);
Graph graphT0(lcd);
Graph graphH0(lcd);
Graph graphT1(lcd);
Graph graphH1(lcd);
Graph graphT2(lcd);
Graph graphH2(lcd);

uint8_t sensAdr[SENS_COUNT] = {20, 93, 111};
uint32_t tm = 0;

float t0, h0, t1, h1, t2, h2;

void startIntro();
void updateDisplay();
void initPlot();

void setup()
{
    delay(2000); // for sensor normal start

    if (DEBUG_SERIAL_ON)
    {
        Serial.begin(115200);

        while (!Serial)
            ;
    }

    rs485.begin(9600);

    while (!rs485)
        ;
    modbus.begin();

    // modbus.setTimeout(10);        //   Указываем максимальное время ожидания ответа по протоколу Modbus.
    modbus.setDelay(4);           //   Указываем минимальный интервал между отправляемыми сообщениями по протоколу Modbus.
    modbus.setTypeMB(MODBUS_RTU); //   Указываем тип протокола Modbus: MODBUS_RTU (по умолчанию), или MODBUS_ASCII.

    // Init LCD
    lcd.begin();
    initPlot();

    // Termo/Humidity heater on
    if (HEATER_ON)
    {
        for (size_t i = 0; i < SENS_COUNT; i++)
        {
            if (modbus.coilWrite(sensAdr[i], 2, true))
            {
                if (DEBUG_SERIAL_ON)
                    Serial.println("Heater " + String(i) + " ON");

                if (BUZZER_ON)
                {
                    // Buzzer on
                    modbus.coilWrite(sensAdr[i], 0, true);
                    delay(50);
                    modbus.coilWrite(sensAdr[i], 0, false);
                    delay(50);
                }
            }
            else
            {
                if (DEBUG_SERIAL_ON)
                    Serial.println("Heater " + String(i) + " ERROR");
            }
        }
    }

    // modbus.coilWrite(sensAdr[1], 0, true);
    // delay(50);
    // modbus.coilWrite(sensAdr[1], 0, false);
    // delay(50);

    if (DEBUG_SERIAL_ON)
        Serial.println();

    if (INTRO_ON)
        startIntro();

    tm = millis() + TIME_REQ;

    // // Termo/Humidity heater on/off
    // if (modbus.coilWrite(adrSens[0], 2, true)) // ON
    //     Serial.println("Heater ON");
    // else
    //     Serial.println("Heater ERROR");

    // if (modbus.coilWrite(adrSens[0], 2, false)) // OFF
    //     Serial.println("Heater OFF");
    // else
    //     Serial.println("Heater ERROR");

    // // Buzzer on/off
    // modbus.coilWrite(adrSens[0], 0, true);
    // delay(50);
    // modbus.coilWrite(adrSens[0], 0, false);
}

void loop()
{
    if (millis() - tm > TIME_REQ)
    {
        for (size_t i = 0; i < SENS_COUNT; i++)
        {
            if (DEBUG_SERIAL_ON)
            {
                Serial.print(i);
                Serial.print(")\tAdr: ");
                Serial.print(sensAdr[i]);

                Serial.print(sensAdr[i] >= 100 ? "\tTemp: " : "\t\tTemp: ");
            }

            int32_t temp = modbus.inputRegisterRead(sensAdr[i], TERM_ADRESS);
            float tf = temp / 10.0;

            if (DEBUG_SERIAL_ON)
                temp < 0 ? Serial.print("Err") : Serial.print(tf, 1);

            if (DEBUG_SERIAL_ON)
                Serial.print("\tHum: ");

            int32_t hum = modbus.inputRegisterRead(sensAdr[i], HUM_ADRESS);
            float hf = hum / 10.0;

            if (DEBUG_SERIAL_ON)
                hum < 0 ? Serial.println("Err") : Serial.println(hf, 1);

            switch (i)
            {
            case 0:
                t0 = tf;
                h0 = hf;

                break;
            case 1:
                t1 = tf;
                h1 = hf;

                break;
            case 2:
                t2 = tf;
                h2 = hf;

                break;
            }
        }

        updateDisplay();

        if (DEBUG_SERIAL_ON)
            Serial.println();

        tm = millis();
    }
}

void updateDisplay()
{
    // Temperatures -40...80
    // Humidity 5...95

    String s = "";

    lcd.setCursor(2, 0);
    lcd.print("<Control Param>");

    lcd.setCursor(0, 3);
    lcd.print("T:");
    lcd.print(t2, 1);

    s = String(t2, 1);

    if (s.length() == 3)
        graphT2.createGraph(5, 3, 6, TEMP_MIN, TEMP_MAX, round(t2));
    else if (s.length() == 4)
        graphT2.createGraph(6, 3, 5, TEMP_MIN, TEMP_MAX, round(t2));
    else
        graphT2.createGraph(7, 3, 4, TEMP_MIN, TEMP_MAX, round(t2));

    lcd.setCursor(11, 3);
    lcd.print("W:");
    lcd.print(round(h2));

    s = String(h2, 1);

    if (s.length() == 3)
        graphH2.createGraph(14, 3, 6, HUM_MIN, HUM_MAX, round(h2));
    else
        graphH2.createGraph(15, 3, 5, HUM_MIN, HUM_MAX, round(h2));

    lcd.setCursor(0, 2);
    lcd.print("T:");
    lcd.print(t1, 1);

    s = String(t1, 1);

    if (s.length() == 3)
        graphT1.createGraph(5, 2, 6, TEMP_MIN, TEMP_MAX, round(t1));
    else if (s.length() == 4)
        graphT1.createGraph(6, 2, 5, TEMP_MIN, TEMP_MAX, round(t1));
    else
        graphT1.createGraph(7, 2, 4, TEMP_MIN, TEMP_MAX, round(t1));

    lcd.setCursor(11, 2);
    lcd.print("W:");
    lcd.print(round(h1));

    s = String(h1, 1);

    if (s.length() == 3)
        graphH1.createGraph(14, 2, 6, HUM_MIN, HUM_MAX, round(h1));
    else
        graphH1.createGraph(15, 2, 5, HUM_MIN, HUM_MAX, round(h1));

    lcd.setCursor(0, 1);
    lcd.print("T:");
    lcd.print(t0, 1);

    s = String(t0, 1);

    if (s.length() == 3)
        graphT0.createGraph(5, 1, 6, TEMP_MIN, TEMP_MAX, round(t0));
    else if (s.length() == 4)
        graphT0.createGraph(6, 1, 5, TEMP_MIN, TEMP_MAX, round(t0));
    else
        graphT0.createGraph(7, 1, 4, TEMP_MIN, TEMP_MAX, round(t0));

    lcd.setCursor(11, 1);
    lcd.print("W:");
    lcd.print(round(h0));

    s = String(h0, 1);

    if (s.length() == 3)
        graphH0.createGraph(14, 1, 6, HUM_MIN, HUM_MAX, round(h0));
    else
        graphH0.createGraph(15, 1, 5, HUM_MIN, HUM_MAX, round(h0));
}

void startIntro()
{
    for (size_t i = 0; i < 4; i++)
    {
        switch (i)
        {
        case 0:
            lcd.setCursor(9, 1);
            lcd.print("T");
            break;
        case 1:
            lcd.setCursor(7, 1);
            lcd.print("B");
            break;
        case 2:
            lcd.setCursor(10, 1);
            lcd.print("H");
            break;
        case 3:
            lcd.setCursor(8, 1);
            lcd.print("A");
            break;
        }
        delay(500);
    }

    String str;

    for (size_t i = 0; i < 6; i++)
    {
        lcd.setCursor(0, 2);
        lcd.print(str);
        lcd.print("Moni");
        str += " ";

        delay(200);
    }

    delay(500);

    for (int i = 19; i > 8; i--)
    {
        lcd.setCursor(i, 2);

        switch (i)
        {
        case 19:
            lcd.print("T");
            break;
        case 18:
            lcd.print("Th");
            break;
        case 17:
            lcd.print("Tho");
            break;
        case 16:
            lcd.print("Thor");
            str = "";
            break;
        default:
            str += " ";
            lcd.print("Thor");
            lcd.print(str);
            break;
        }

        delay(200);
    }

    delay(250);

    for (int i = 2; i > 0; i--)
    {
        lcd.setCursor(4, i);
        lcd.print("|");
    }
    for (size_t i = 5; i < 13; i++)
    {
        lcd.setCursor(i, 0);
        lcd.print("-");
    }
    for (size_t i = 1; i < 3; i++)
    {
        lcd.setCursor(13, i);
        lcd.print("|");
    }
    for (int i = 12; i > 4; i--)
    {
        lcd.setCursor(i, 3);
        lcd.print("-");
    }

    delay(1000);

    for (size_t i = 0; i < 4; i++)
    {
        switch (i)
        {
        case 0:
            lcd.setCursor(16, i);
            lcd.print("v2.0");
            break;
        case 1:
            lcd.setCursor(16, 0);
            lcd.print("    ");
            lcd.setCursor(16, i);
            lcd.print("v2.0");
            break;
        case 2:
            lcd.setCursor(16, 0);
            lcd.print("    ");
            lcd.setCursor(16, 1);
            lcd.print("    ");
            lcd.setCursor(16, i);
            lcd.print("v2.0");
            break;
        case 3:
            lcd.setCursor(16, 0);
            lcd.print("    ");
            lcd.setCursor(16, 1);
            lcd.print("    ");
            lcd.setCursor(16, 2);
            lcd.print("    ");
            lcd.setCursor(16, i);
            lcd.print("v2.0");
            break;
        }
        delay(250);
    }

    delay(1000);

    lcd.clear();
}

void initPlot()
{
    byte row7[8] = {0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111};
    byte row6[8] = {0b00000, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111};
    byte row5[8] = {0b00000, 0b00000, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111};
    byte row4[8] = {0b00000, 0b00000, 0b00000, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111};
    byte row3[8] = {0b00000, 0b00000, 0b00000, 0b00000, 0b11111, 0b11111, 0b11111, 0b11111};
    byte row2[8] = {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111, 0b11111, 0b11111};
    byte row1[8] = {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111, 0b11111};
    byte row0[8] = {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111};

    lcd.createChar(0, row0);
    lcd.createChar(1, row1);
    lcd.createChar(2, row2);
    lcd.createChar(3, row3);
    lcd.createChar(4, row4);
    lcd.createChar(5, row5);
    lcd.createChar(6, row6);
    lcd.createChar(7, row7);
}