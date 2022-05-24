#include <MQTT.h>
#include <WiFi.h>
#include <Smartcar.h>
#include <stdlib.h>

WiFiClient net;
MQTTClient mqtt;

/*-------------------------------------- CONFIGURATIONS --------------------------------------*/

ArduinoRuntime arduinoRuntime;
BrushedMotor leftMotor(arduinoRuntime, smartcarlib::pins::v2::leftMotorPins);
BrushedMotor rightMotor(arduinoRuntime, smartcarlib::pins::v2::rightMotorPins);
DifferentialControl control(leftMotor, rightMotor);

GY50 gyroscope(arduinoRuntime, 37);
const auto pulsesPerMeter = 600;
const unsigned long LEFT_PULSES_PER_METER = 600;
DirectionalOdometer leftOdometer{ arduinoRuntime,
                                 smartcarlib::pins::v2::leftOdometerPins,
                                 []() { leftOdometer.update(); },
                                 LEFT_PULSES_PER_METER }; DirectionlessOdometer rightOdometer{ arduinoRuntime,smartcarlib::pins::v2::rightOdometerPin,[]() { rightOdometer.update(); },pulsesPerMeter };

SmartCar car(arduinoRuntime, control, gyroscope, leftOdometer,rightOdometer);

// Front Ultrasonic Sensor
const int triggerPin = 12;  //D6
const int echoPin = 7;  //D7
const unsigned int maxDistance = 100;
SR04 front{arduinoRuntime, triggerPin, echoPin, maxDistance};

//Infrared Sensors
const int backRightIRPin = 0;
const int leftIRPin = 1;
const int rightIRPin = 2;
const int backIRPin = 3;        
const int frontRightIRPin = 5;
const int frontLeftIRPin = 4;
const int backLeftIRPin = 6;

typedef GP2Y0A21 Infrared; //Basically a 'rename'
  Infrared rightIR(arduinoRuntime, rightIRPin);
  Infrared leftIR(arduinoRuntime, leftIRPin);
  Infrared back(arduinoRuntime, backIRPin);
  Infrared frontLeft(arduinoRuntime, frontLeftIRPin);
  Infrared frontRight(arduinoRuntime, frontRightIRPin);
  Infrared backRight(arduinoRuntime, backRightIRPin);
  Infrared backLeft(arduinoRuntime, backLeftIRPin);
  

  // Car Info
  int speed = 0;
  int turningAngle = 0;

/*-------------------------------------- CONSTANTS --------------------------------------*/
                                        
const int SPEED_INCREMENT = 5;
const int TURNING_INCREMENT = 10;
const int FORWARD_SPEED_LIMIT = 150;
const int BACKWARD_SPEED_LIMIT = -50;
const int MAX_STEERING_ANGLE = 90;
const auto ONE_SECOND = 1000UL;

const int PARKING_SPEED = 15;
const int BOX_WIDTH = 70;
const int BOX_HEIGHT = 35;
const int PARKING_ROWS = 5;
const int PARKING_COLS = 3;
const int ENTRANCE_R = 4;
const int ENTRANCE_C = 1;

/*-------------------------------------- SETUP AND LOOP --------------------------------------*/

void setup(){

  // Move the car with 50% of its full speed
  Serial.begin(9600);
  #ifdef __SMCE__
    mqtt.begin("127.0.0.1", 1883, net); // Will connect to localhost
  #else
    mqtt.begin(net);
  #endif
    if (mqtt.connect("arduino", "public", "public")) {
      mqtt.subscribe("/smartcar/control/#", 1);
      mqtt.onMessage([](String topic, String message) { handleMQTTMessage(topic, message); });
    }
    Serial.println(getAngle());
}

void loop() {

if (mqtt.connected()) {
    rightOdometer.update();
    mqtt.loop();
    const auto currentTime = millis();
    static auto previousTransmission = 0UL;
    if (currentTime - previousTransmission >= ONE_SECOND) {
      previousTransmission = currentTime;
      const auto distance = String(front.getDistance());
    }
  }
#ifdef __SMCE__
  // Avoid over-using the CPU if we are running in the emulator
  delay(1);
#endif
  checkObstacles();
  handleInput();
  #ifdef __SMCE__
    // Avoid over-using the CPU if we are running in the emulator
    delay(1);
  #endif
}

/*-------------------------------------- MQTT METHODS --------------------------------------*/

void handleMQTTMessage(String topic, String message){
   if (topic == "/smartcar/control/speed") {
          setSpeed(message.toFloat());
    } else if (topic == "/smartcar/control/steering") {
          setAngle(message.toFloat());
    } else {
          Serial.println(topic + " " + message);
    }
}

void setSpeed(float newSpeed){
  if(newSpeed > FORWARD_SPEED_LIMIT || newSpeed < BACKWARD_SPEED_LIMIT){
    newSpeed = newSpeed > 0 ? FORWARD_SPEED_LIMIT : BACKWARD_SPEED_LIMIT;
  }
  speed = newSpeed;
  car.setSpeed(newSpeed);
}

void setAngle(float newAngle){
  if(newAngle > MAX_STEERING_ANGLE){
    newAngle = MAX_STEERING_ANGLE;
  }
  car.setAngle(newAngle);
}

int getAngle(){
    gyroscope.update();
    return gyroscope.getHeading();
}

/*-------------------------------------- OBSTACLE DETECTION --------------------------------------*/

bool isObsAtFront(float obsDistance) {
    const auto frontDist = front.getDistance();
    return (frontDist > 0 && frontDist <= obsDistance);
}

bool isObsAtFront() {
    const auto frontDist = front.getDistance();
    return (frontDist > 0 && frontDist <= 15);
}

bool isObsAtFrontLeft(float obsDistance) {
    const auto frontLeftDist = frontLeft.getDistance();
    return (frontLeftDist > 0 && frontLeftDist <= obsDistance);
}

bool isObsAtFrontLeft() {
    const auto frontLeftDist = frontLeft.getDistance();
    return (frontLeftDist > 0 && frontLeftDist <= 15);
}

bool isObsAtFrontRight(float obsDistance) {
    const auto frontRightDist = frontRight.getDistance();
    return (frontRightDist > 0 && frontRightDist < obsDistance);
}

bool isObsAtFrontRight() {
    const auto frontRightDist = frontRight.getDistance();
    return (frontRightDist > 0 && frontRightDist < 15);
}

bool isObsAtLeft(float obsDistance) {
    const auto lDist = leftIR.getDistance();
    return (lDist > 0 && lDist <= obsDistance);
}

bool isObsAtLeft() {
    const auto lDist = leftIR.getDistance();
    return (lDist > 0 && lDist <= 15);
}

bool isObsAtRight(float obsDistance) {
    const auto rDist = rightIR.getDistance();
    return (rDist > 0 && rDist <= obsDistance);
}

bool isObsAtRight() {
    const auto rDist = rightIR.getDistance();
    return (rDist > 0 && rDist <= 15);
}

bool isObsAtBackRight(float obsDistance) {
    const auto backRightDist = backRight.getDistance();
    return (backRightDist > 0 && backRightDist < obsDistance);
}
bool isObsAtBackRight() {
    const auto backRightDist = backRight.getDistance();
    return (backRightDist > 0 && backRightDist < 15);
}

bool isObsAtBackLeft(float obsDistance) {
    const auto backLeftDist = backLeft.getDistance();
    return (backLeftDist > 0 && backLeftDist < obsDistance);
}

bool isObsAtBackLeft() {
    const auto backLeftDist = backLeft.getDistance();
    return (backLeftDist > 0 && backLeftDist < 15);
}

void checkObstacles(){
  const auto distance = front.getDistance();
  //
  if (isObsAtFront()) {
    stopCar();
  }
}

/*-------------------------------------- INTERNAL MAP SETUP --------------------------------------*/

// the different types types that a parking lot has
enum BoxType{Path, Unoccupied, Occupied, Start, notParking};

class GridBox{
  public:
    int row;
    int col;
    BoxType type;
    GridBox(){}
    GridBox(int r, int c, BoxType boxType){
      row = r;
      col = c;
      type = boxType;
    }
};

boolean isParked = false;
GridBox parkedAt;

// This is the representation of the parking lot in terms of code
// The code here works for a parking lot that has infinite rows but only three columns
// i.e a parking lot of 5x3 works just as well as a parking lot of 30x3, as long as
// the middle column remains to be the path.
// c = column, r = row
GridBox parkingLot[PARKING_ROWS][PARKING_COLS] = {
    {GridBox(0, 0, Occupied),  GridBox(0, 1, Path), GridBox(0, 2, Occupied)},
    {GridBox(1, 0, Unoccupied),GridBox(1, 1, Path), GridBox(1, 2, Occupied)},
    {GridBox(2, 0, Occupied),  GridBox(2, 2, Path), GridBox(2, 2, Unoccupied)},
    {GridBox(3, 0, Occupied),  GridBox(3, 2, Path), GridBox(3, 2, Occupied)},
    {GridBox(4, 0, notParking),GridBox(4, 1, Start),GridBox(4, 2, notParking)},
};

void park(){
    for(int i = 0; i < PARKING_ROWS; i++){
        for(int j = 0; j < PARKING_COLS; j++){
            if(parkingLot[i][j].type == Unoccupied){
                move(ENTRANCE_R, ENTRANCE_C, i, j);
                if(j < ENTRANCE_C){
                    autoLeftPark();
                } else{
                    autoRightPark();
                }
                parkingLot[i][j].type = Occupied;
                parkedAt = parkingLot[i][j];
                isParked = true;
                return;
            }
        }
    }
}

void retrieve(){
    int r = parkedAt.row;
    int c = parkedAt.col;
    if(c < ENTRANCE_C){
        autoRightReverse();
    } else{
        autoLeftReverse();
    }
    move(r, c, ENTRANCE_R, ENTRANCE_C);
    isParked = false;
    parkingLot[r][c].type = Unoccupied;

}

// move(parkedAt.row, parkedAt.col, ENTRANCE_R, ENTRANCE_C);
// 1,0,4,1
void move(int r1, int c1, int r2, int c2){
    float distance;
    float diffR;
    int parkingR;
    if(!isParked){
        parkingR = r2+1;
        int parkingC = 1;
        int currentAngle = getAngle();
        // add 1.46 to take the distance of the entrance box into account
        // and the slight movement into the parking space
        diffR = r1-parkingR+1.46;
    }  else {
        diffR = abs(r1-r2-2.0);
    }
    Serial.println(diffR);
    distance = (diffR * BOX_HEIGHT);
    move(distance);
}

void move(float distance){
    leftOdometer.reset();
    while(leftOdometer.getDistance() < distance){
        car.setSpeed(PARKING_SPEED);
    }
    car.setSpeed(0);
}

/*-------------------------------------- AUTO PARKING --------------------------------------*/

void autoRightPark(){ // the car is supposed to park inside a parking spot to its immediate right
    Serial.println("auto right park");
    gyroscope.update();
    // currently using these timers as a way to reduce the amount of times the if-statements are true, to reduce the amount of changes to the cars turning
    int rightTimer = 500;
    int frontRightTimer = 500;
    int leftTimer = 500;
    int frontLeftTimer = 500;
    int backRightTimer = 500;
    int backLeftTimer = 500;
    int frontTimer = 500;

    // these are used so that if isObsAtFrontRight() is true, the car does not stop turning while reversing
    int distanceTraveled = 0;
    int newDistanceTraveled = 0;


    int targetAngle = 0;
    int currentAngle = gyroscope.getHeading();
    if (currentAngle - 90 < 0){
        targetAngle = 360 - abs(currentAngle - 90);
    } else {
        targetAngle = currentAngle - 90;
    }
    car.setAngle(85);
    car.setSpeed(15);
    Serial.println(currentAngle);
    Serial.println(targetAngle);
    while (targetAngle <= currentAngle){
        gyroscope.update();
        newDistanceTraveled = leftOdometer.getDistance();
        currentAngle = gyroscope.getHeading();
        // newDistanceTraveled > distanceTraveled is there to check that this does not trigger while the car is reversing
        if(isObsAtFrontRight() && frontRightTimer > 500 && newDistanceTraveled > distanceTraveled) { // reduce turning angle
            frontRightTimer = 0;
            turningAngle = 5;
            car.setAngle(turningAngle);
            car.setSpeed(PARKING_SPEED);
        }
        if(isObsAtFrontLeft() && frontLeftTimer > 500) { // reverses car and changes the turning angle to the opposite direction
            frontLeftTimer = 0;
            if (car.getSpeed() > 0){
              car.setSpeed(-PARKING_SPEED);
            }
            delay(200); // delay is included to make sure newDistanceTraveled > distanceTraveled is not true before the car starts reversing
            turningAngle = -85;
            car.setAngle(turningAngle);

        }
        if(isObsAtLeft() && leftTimer > 500) { // reverses car and changes the turning angle to the opposite direction
            leftTimer = 0;
            turningAngle = -85;
            car.setAngle(turningAngle);
            if (car.getSpeed() > 0){
              car.setSpeed(-PARKING_SPEED);
            }
        }

        if(isObsAtFront() && frontTimer > 500){ // reverses car and changes the turning angle to the opposite direction
            frontTimer = 0;
            turningAngle = -85;
            car.setAngle(turningAngle);
            if (car.getSpeed() > 0){
              car.setSpeed(-PARKING_SPEED);
            }
        }
        rightTimer++;
        frontRightTimer++;
        leftTimer++;
        frontLeftTimer++;
        frontTimer++;
        distanceTraveled = newDistanceTraveled;
    }
    car.setAngle(0);
    car.setSpeed(10);
    // here the car will have the correct angle, car will drive forward and park
}

void autoLeftPark(){ // the car is supposed to park inside a parking spot to its immediate left
    Serial.println("auto left park");
    gyroscope.update();
    // currently using these timers as a way to reduce the amount of times the if-statements are true, to reduce the amount of changes to the cars turning
    int rightTimer = 500;
    int frontRightTimer = 500;
    int leftTimer = 500;
    int frontLeftTimer = 500;
    int backRightTimer = 500;
    int backLeftTimer = 500;
    int frontTimer = 500;

    // these are used so that if isObsAtFrontLeft() is true, the car does not stop turning while reversing
    int distanceTraveled = 0;
    int newDistanceTraveled = 0;

    int targetAngle = 0;
    int currentAngle = gyroscope.getHeading();
    if (currentAngle + 90 > 360){
        targetAngle = 0 + abs(currentAngle - 360);
    } else {
        targetAngle = currentAngle + 90;
    }
    car.setAngle(-85);
    car.setSpeed(PARKING_SPEED);
    while (targetAngle-3 >= currentAngle){
        gyroscope.update();
        newDistanceTraveled = leftOdometer.getDistance();
        currentAngle = gyroscope.getHeading();

        if(isObsAtFrontLeft() && frontRightTimer > 500 && newDistanceTraveled > distanceTraveled) { // reduce turning angle
            frontRightTimer = 0;
            turningAngle = -85;
            car.setAngle(turningAngle);
            car.setSpeed(PARKING_SPEED);
        }
        if(isObsAtFrontRight() && frontLeftTimer > 500) { // reverses car and changes the turning angle to the opposite direction
            frontLeftTimer = 0;
            if (car.getSpeed() > 0){
                car.setSpeed(-PARKING_SPEED);
            }
            delay(200); // delay is included to make sure newDistanceTraveled > distanceTraveled is not true before the car starts reversing
            turningAngle = 85;
            car.setAngle(turningAngle);

        }
        if(isObsAtRight() && leftTimer > 500) { // reverses car and changes the turning angle to the opposite direction
            rightTimer = 0;
            turningAngle = 85;
            car.setAngle(turningAngle);
            if (car.getSpeed() > 0){
                car.setSpeed(-PARKING_SPEED);
            }
        }

        if(isObsAtFront() && frontTimer > 500){ // reverses car and changes the turning angle to the opposite direction
            frontTimer = 0;
            turningAngle = 85;
            car.setAngle(turningAngle);
            if (car.getSpeed() > 0){
                car.setSpeed(-PARKING_SPEED);
            }
        }
        rightTimer++;
        frontRightTimer++;
        leftTimer++;
        frontLeftTimer++;
        frontTimer++;
        distanceTraveled = newDistanceTraveled;
    }
    Serial.println("done!");
    car.setAngle(0);
    car.setSpeed(PARKING_SPEED);
    // here the car will have the correct angle, car will drive foward and park
}

void autoRightReverse(){ // When the car is supposed to turn right out of a parking spot
    Serial.println("reverse right!");
    gyroscope.update();
    // currently using these timers as a way to reduce the amount of times the if-statements are true, to reduce the amount of changes to the cars turning
    int rightTimer = 500;
    int frontRightTimer = 500;
    int leftTimer = 500;
    int frontLeftTimer = 500;
    int backRightTimer = 500;
    int backLeftTimer = 500;
    int frontTimer = 500;

    int distanceTraveled = 0;
    int targetAngle = 0;
    int currentAngle = getAngle();

    if (currentAngle - 90 < 0){
        targetAngle = 360 - abs(currentAngle - 90);
    } else {
        targetAngle = currentAngle - 90;
    }

    Serial.println(targetAngle);
    car.setSpeed(-PARKING_SPEED);
    leftOdometer.reset();
    while (distanceTraveled > -50){
        distanceTraveled = leftOdometer.getDistance();
    }

    turningAngle = 85;
    car.setAngle(turningAngle);
    while (targetAngle <= getAngle()){
        currentAngle = gyroscope.getHeading();
        // during reversing all obstacle detection causes the car to stop turning for a small distance to get the car away from the obstacle
        if(isObsAtFrontRight() && frontRightTimer > 500) {
            frontRightTimer = 0;
            turningAngle = 0;
            car.setAngle(turningAngle);
            leftOdometer.reset();
            distanceTraveled = 0;
            while (distanceTraveled > -5){
                distanceTraveled = leftOdometer.getDistance();
                Serial.println(distanceTraveled);
            }
            turningAngle = 85;
            car.setAngle(turningAngle);
            car.setSpeed(-PARKING_SPEED);
        }
        if(isObsAtFrontLeft() && frontLeftTimer > 500) {
            frontLeftTimer = 0;
            turningAngle = 0;
            car.setAngle(turningAngle);
            leftOdometer.reset();
            distanceTraveled = 0;
            while (distanceTraveled > -5){
                distanceTraveled = leftOdometer.getDistance();
                Serial.println(distanceTraveled);
            }
            turningAngle = 85;
            car.setAngle(turningAngle);
            car.setSpeed(-PARKING_SPEED);
        }
        if(isObsAtLeft() && leftTimer > 500) {
            leftTimer = 0;
            turningAngle = 0;
            car.setAngle(turningAngle);
            leftOdometer.reset();
            distanceTraveled = 0;
            while (distanceTraveled > -5){
                distanceTraveled = leftOdometer.getDistance();
                Serial.println(distanceTraveled);
            }
            turningAngle = 85;
            car.setAngle(turningAngle);
            car.setSpeed(-PARKING_SPEED);
        }
        if(isObsAtFront() && frontTimer > 500){
            frontTimer = 0;
            turningAngle = 0;
            car.setAngle(turningAngle);
            leftOdometer.reset();
            distanceTraveled = 0;
            while (distanceTraveled > -5){
                distanceTraveled = leftOdometer.getDistance();
                Serial.println(distanceTraveled);
            }
            turningAngle = 85;
            car.setAngle(turningAngle);
            car.setSpeed(-PARKING_SPEED);
        }
        rightTimer++;
        frontRightTimer++;
        leftTimer++;
        frontLeftTimer++;
        frontTimer++;
    }
    car.setAngle(0);
    // here the car will have the correct angle, car will drive foward and park
}

void autoLeftReverse(){ // When the car is supposed to turn left out of a parking spot
    Serial.println("reverse right!");
    gyroscope.update();
    // currently using these timers as a way to reduce the amount of times the if-statements are true, to reduce the amount of changes to the cars turning
    int rightTimer = 500;
    int frontRightTimer = 500;
    int leftTimer = 500;
    int frontLeftTimer = 500;
    int backRightTimer = 500;
    int backLeftTimer = 500;
    int frontTimer = 500;

    int distanceTraveled = 0;
    int targetAngle = 0;
    int currentAngle = getAngle();

    if (currentAngle - 90 < 0){
        targetAngle = 360 - abs(currentAngle - 90);
    } else {
        targetAngle = currentAngle - 90;
    }

    Serial.println(targetAngle);
    car.setSpeed(-PARKING_SPEED);
    leftOdometer.reset();
    while (distanceTraveled > -50){
        distanceTraveled = leftOdometer.getDistance();
    }

    turningAngle = -85;
    car.setAngle(turningAngle);
    while (targetAngle >= getAngle()){
        currentAngle = gyroscope.getHeading();
        // during reversing all obstacle detection causes the car to stop turning for a small distance to get the car away from the obstacle
        if(isObsAtFrontRight() && frontRightTimer > 500) {
            frontRightTimer = 0;
            turningAngle = 0;
            car.setAngle(turningAngle);
            leftOdometer.reset();
            distanceTraveled = 0;
            while (distanceTraveled > -5){
                distanceTraveled = leftOdometer.getDistance();
                Serial.println(distanceTraveled);
            }
            turningAngle = -85;
            car.setAngle(turningAngle);
            car.setSpeed(-PARKING_SPEED);
        }
        if(isObsAtFrontLeft() && frontLeftTimer > 500) {
            frontLeftTimer = 0;
            turningAngle = 0;
            car.setAngle(turningAngle);
            leftOdometer.reset();
            distanceTraveled = 0;
            while (distanceTraveled > -5){
                distanceTraveled = leftOdometer.getDistance();
                Serial.println(distanceTraveled);
            }
            turningAngle = -85;
            car.setAngle(turningAngle);
            car.setSpeed(-PARKING_SPEED);
        }
        if(isObsAtLeft() && leftTimer > 500) {
            leftTimer = 0;
            turningAngle = 0;
            car.setAngle(turningAngle);
            leftOdometer.reset();
            distanceTraveled = 0;
            while (distanceTraveled > -5){
                distanceTraveled = leftOdometer.getDistance();
                Serial.println(distanceTraveled);
            }
            turningAngle = -85;
            car.setAngle(turningAngle);
            car.setSpeed(-PARKING_SPEED);
        }
        if(isObsAtFront() && frontTimer > 500){
            frontTimer = 0;
            turningAngle = 0;
            car.setAngle(turningAngle);
            leftOdometer.reset();
            distanceTraveled = 0;
            while (distanceTraveled > -5){
                distanceTraveled = leftOdometer.getDistance();
                Serial.println(distanceTraveled);
            }
            turningAngle = -85;
            car.setAngle(turningAngle);
            car.setSpeed(-PARKING_SPEED);
        }
        rightTimer++;
        frontRightTimer++;
        leftTimer++;
        frontLeftTimer++;
        frontTimer++;
    }
    car.setAngle(0);
    // here the car will have the correct angle, car will drive foward and park
}

/*-------------------------------------- SERIAL METHODS --------------------------------------*/

void handleInput(){
  if(Serial.available()){
    char input = Serial.read();
    switch(input){
      case 'i':
        increaseSpeed();
        break;
      case 'k': 
        decreaseSpeed();
        break;
      case 'j': 
        turnLeft();
        break;
      case 'l': 
        turnRight();
        break;
      case 'u':
        stopCar();
        break;
      case 'p':
        park();
        break;
      case 'r':
        retrieve();
        break;
      default:
        break;
    }
  }
}

void increaseSpeed(){
  //sets max speed to 110
  speed = speed+SPEED_INCREMENT>=110 ? 110 : speed+SPEED_INCREMENT;
  car.setSpeed(speed);
}

void decreaseSpeed(){
  //sets max speed to 110
  speed = speed-SPEED_INCREMENT;
  car.setSpeed(speed);
}

void turnLeft(){ // turns the car 10 degrees counter-clockwise (degrees depend on TURNING_INCREMENT)
  turningAngle = turningAngle-TURNING_INCREMENT;
  car.setAngle(turningAngle);
}

void turnRight(){ // turns the car 10 degrees clockwise (degrees depend on TURNING_INCREMENT)
  turningAngle = turningAngle+TURNING_INCREMENT;
  car.setAngle(turningAngle);
}

void stopCar(){
  while(car.getSpeed() > 0){
    int speed = speed > 0 ? speed-0.1 : speed+0.1;
    car.setSpeed(speed);
  }
}