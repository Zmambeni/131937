boolean insert = false;

const int COIN_PIN = 13;
const int COIN_OUT = 15;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(COIN_OUT, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(COIN_PIN), coinInterrupt, RISING);
  delay(1000);
}

void loop() {
  Serial.println("starting lloop " + insert);
  // put your main code here, to run repeatedly:
  
  if (insert) {
    insert = false;
    Serial.println("coin detected!");
    digitalWrite(COIN_OUT, HIGH);
    delay(1000);
    digitalWrite(COIN_OUT, LOW);
  }
}

//interrupt
void coinInterrupt() {
  insert = true;
}
