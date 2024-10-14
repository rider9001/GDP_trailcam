#include <minus.h>
#include <add.h>

uint8_t a, b;

void setup() {
  Serial.begin(9600);

  a = 0;
  b = 0;
}

void loop() {
  // Say hi!
  Serial.println("Hello!");
  Serial.println(add(a++,b++));
  Serial.println(add3(a,b,a));
  Serial.println(minus(a, b / 2));
  delay(500);
}
