
#include <Servo.h>

// MOTOR DRIVER - L298N 

const int ENA = 5;
const int IN1 = 8;
const int IN2 = 9;

const int ENB = 6;
const int IN3 = 10;
const int IN4 = 11;

// Motor direction correction
const bool LEFT_REVERSED = true;
const bool RIGHT_REVERSED = true;

// FRONT ULTRASONIC SENSOR 

const int TRIG = 7;
const int ECHO = 12;

// SERVO 

Servo scanner;
const int SERVO_PIN = 3;

// SETTINGS
const int MOTOR_SPEED = 150;
const int TURN_SPEED = 160;

const int SAFE_DISTANCE = 25;

const int REVERSE_MS = 500;
const int TURN_MS = 1200;

// Servo positions
const int CENTRE = 90;
const int LEFT = 150;
const int RIGHT = 30;


// DISTANCE MEASUREMENT

long getDistance() {

  digitalWrite(TRIG, LOW);
  delayMicroseconds(3);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH, 25000);

  if (duration == 0) {
    return 999;
  }

  return duration * 0.0343 / 2;
}


// MOTOR CONTROL 

void setLeft(int dir) {

  if (LEFT_REVERSED) {
    dir = -dir;
  }

  digitalWrite(IN1, dir > 0);
  digitalWrite(IN2, dir < 0);
}

void setRight(int dir) {

  if (RIGHT_REVERSED) {
    dir = -dir;
  }

  digitalWrite(IN3, dir > 0);
  digitalWrite(IN4, dir < 0);
}

void stopMotors() {

  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

void forward() {

  setLeft(1);
  setRight(1);

  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}

void reverse() {

  setLeft(-1);
  setRight(-1);

  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}

void turnLeft() {

  setLeft(-1);
  setRight(1);

  analogWrite(ENA, TURN_SPEED);
  analogWrite(ENB, TURN_SPEED);
}


//  OBSTACLE AVOIDANCE 

void avoidObstacle() {

  // Stop
  stopMotors();
  delay(300);

  // Scan left
  scanner.write(LEFT);
  delay(500);

  // Scan right
  scanner.write(RIGHT);
  delay(500);

  // Return to centre
  scanner.write(CENTRE);
  delay(400);

  // Reverse
  reverse();
  delay(REVERSE_MS);

  stopMotors();
  delay(200);

  // Always turn LEFT
  turnLeft();
  delay(TURN_MS);

  // Stop after turning
  stopMotors();
  delay(300);
}


//  SETUP

void setup() {

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  scanner.attach(SERVO_PIN);
  scanner.write(CENTRE);

  Serial.begin(9600);

  stopMotors();

  delay(1000);
}


// MAIN PROGRAM 
void loop() {

  scanner.write(CENTRE);
  delay(100);

  long distance = getDistance();

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  if (distance > SAFE_DISTANCE) {

    forward();

  } else {

    avoidObstacle();

  }

  delay(50);
}
