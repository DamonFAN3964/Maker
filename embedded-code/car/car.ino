#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

int AIN1 = 8;  // 左轮IN1引脚
int AIN2 = 7;  // 左轮IN2引脚
int BIN1 = 11; // 右轮IN1引脚
int BIN2 = 12; // 右轮IN2引脚
int PWMA = 9;  // 左轮PWM引脚
int PWMB = 10;  // 右轮PWM引脚
int STBY = 13; // 使能引脚

int TrigPin1 = 26;  // 超声波传感器1的Trig引脚，左
int EchoPin1 = 27;  // 超声波传感器1的Echo引脚
int TrigPin2 = 24;  // 超声波传感器2的Trig引脚，中间
int EchoPin2 = 25;  // 超声波传感器2的Echo引脚
int TrigPin3 = 22;  // 超声波传感器3的Trig引脚，右
int EchoPin3 = 23;  // 超声波传感器3的Echo引脚

int sensor1 = 16; // 红外传感器1引脚(左)
int sensor2 = 15; // 红外传感器2引脚(中)
int sensor3 = 14; // 红外传感器3引脚(右)

int pos = 0;

// 计算超声波距离
float distance(int TrigPin, int EchoPin) {
  digitalWrite(TrigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(TrigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(TrigPin, LOW);
  float distance = pulseIn(EchoPin, HIGH) / 58.00;
  return distance;
}

// 获取超声波传感器1的距离，左
float getDistance1() {
  Serial.print("Distance1: ");
  float dist = distance(TrigPin1, EchoPin1);
  Serial.print(dist);
  Serial.println(" cm");
  return dist;
}

// 获取超声波传感器2的距离，中间
float getDistance2() {
  Serial.print("Distance2: ");
  float dist = distance(TrigPin2, EchoPin2);
  Serial.print(dist);
  Serial.println(" cm");
  return dist;
}

// 获取超声波传感器3的距离,右边
float getDistance3() {
  Serial.print("Distance3: ");
  float dist = distance(TrigPin3, EchoPin3);
  Serial.print(dist);
  Serial.println(" cm");
  return dist;
}
int flag;
// 初始化设置
void setup() {
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH);

  pinMode(TrigPin1, OUTPUT);
  pinMode(EchoPin1, INPUT);
  pinMode(TrigPin2, OUTPUT);
  pinMode(EchoPin2, INPUT);
  pinMode(TrigPin3, OUTPUT);
  pinMode(EchoPin3, INPUT);

  pinMode(sensor1, INPUT);
  pinMode(sensor2, INPUT);
  pinMode(sensor3, INPUT);

  Serial.begin(9600);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Hello, World!");
  display.display();


  // left();
  // delay(200);
  // stop();
  delay(2000);
}

// 向前移动
void go() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  analogWrite(PWMA, 85);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMB, 90);
}

void gogo() {
  analogWrite(PWMA, 140);
  digitalWrite(AIN1,HIGH);
  digitalWrite(AIN2,LOW);
  analogWrite(PWMB, 140);
  digitalWrite(BIN1,HIGH);
  digitalWrite(BIN2,LOW);
}

void xunleft() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  analogWrite(PWMA, 100);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMB, 100);
}

void xunright() {
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  analogWrite(PWMA, 100);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMB, 100);
}

//右转
void right() {
  analogWrite(PWMA, 160);
  digitalWrite(AIN1,HIGH);
  digitalWrite(9,LOW);
  analogWrite(PWMB, 59);
  digitalWrite(BIN2,HIGH);
  digitalWrite(BIN1,LOW);
}

//左转
void left() {
  analogWrite(PWMA, 62);
  digitalWrite(AIN1,HIGH);
  digitalWrite(9,LOW);
  analogWrite(PWMB, 160);
  digitalWrite(BIN2,HIGH);
  digitalWrite(BIN1,LOW);
}

//超声波避障
void soundtracking() {
  float R = getDistance3();
  float M = getDistance2();
  float L = getDistance1();

  // gogo();
  static int i = 0;
  if (M <= 20 && L <= 20) {
    flag = 3;
    right();
    delay(900);
    stop();
  }
  else if (M <= 20 && R <= 20) {
    // while(i == 0) {
    //   analogWrite(PWMA, 75);
    //   digitalWrite(AIN1,HIGH);
    //   digitalWrite(9,LOW);
    //   analogWrite(PWMB, 175);
    //   digitalWrite(BIN2,HIGH);
    //   digitalWrite(BIN1,LOW);
    //   delay(350);
    //   i++;
    // }
    flag = 1;
    left();
    delay(900);
    stop();
  }
  else{
    flag = 2;
    // go();
    gogo();
    delay(500);
    stop();
  }
}

void linefollow() { //寻迹
  int s1, s2, s3 = 0;
  s2 = digitalRead(sensor2);
  delay(1);
  s3 = digitalRead(sensor3); //R   
  delay(1);
  s1 = digitalRead(sensor1); //L
  delay(1); 
  
  if (s3 == HIGH && s1 == LOW) {
    xunright();
    delay(20);
  } else if (s1 == HIGH && s3 == LOW) {
    xunleft();
    delay(20);
  } else if (s1 == HIGH && s3 == HIGH) {
    go();
    delay(200);
  }
}

void stop() {
  digitalWrite(AIN2, HIGH);
  digitalWrite(AIN1, HIGH);
  analogWrite(PWMA, 0);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMB, 0);
  delay(200);
}

int loop_cnt;


void display_info() {
  float L;  // 使用英文半角分号
  float M;  // 修正：将中文全角分号改为英文半角分号
  float R;
  int s1;
  int s2;
  int s3;
  
  L = getDistance1();
  M = getDistance2();
  R = getDistance3();
  s1 = digitalRead(sensor1);
  s2 = digitalRead(sensor2);
  s3 = digitalRead(sensor3);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("L csb :  ");
  display.println(L);
  display.print("M csb :  ");
  display.println(M);
  display.print("R csb :  ");
  display.println(R);
  display.print("L hw :  ");
  display.println(s1);
  display.print("M hw :  ");
  display.println(s2);
  display.print("R hw :  ");
  display.println(s3);
  display.print("flag :  ");
  display.println(flag);
  display.print("loop_cnt :  ");
  display.println(loop_cnt);
  display.display();
}

void loop() {
  // Serial.println("================================");
  
  // 在loop函数中声明变量
  // float L = getDistance1();
  // float M = getDistance2();
  // float R = getDistance3();
  // int s1 = digitalRead(sensor1);
  // int s2 = digitalRead(sensor2);
  // int s3 = digitalRead(sensor3);

  // if(pos == 0)
  //   linefollow();
  // flag = 1;
  // display.display();
  
  // if(M < 80 && L <= 45 && s1 == 0 && s3 == 0) {
  //   pos++;
  // }
  
  // if(pos >= 1)
  // soundtracking();
  gogo();
  // left();
  // flag = 2;
  loop_cnt++;
 
  // Serial.print("Sensor1: ");
  // Serial.println(s1);
  // Serial.print("Sensor2: ");
  // Serial.println(s2);
  // Serial.print("Sensor3: ");
  // Serial.println(s3);
  
  display_info();  // 调用显示函数
  // display.display();
  delay(200);
}
