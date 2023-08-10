void setup() {
    Serial.begin(9600);
}

int oldv1 = -1;
int oldv2 = -1;

void loop() {
  int v1 = analogRead(A0);

  int v2 = analogRead(A1);

  if (abs(v1 - oldv1) > 10) {
    Serial.print("P:");
    Serial.println(v1);
    oldv1 = v1;
  }

  if (abs(v2 - oldv2) > 10) {
    Serial.print("R:");
    Serial.println(v2);
    oldv2 = v2;
  }

  delay(10);
}
