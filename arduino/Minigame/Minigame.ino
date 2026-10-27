#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ขาปุ่มกด 1 ปุ่ม
const int btnPin = 2;

// สถานะของระบบ (0 = หน้าเมนู, 1 = เล่นเกม Cave, 2 = เล่นเกม Star)
int gameState = 0;
int selectedGame = 1; 

// ตัวแปรสำหรับระบบเมนู
unsigned long menuTimer = 0;
bool lastButtonState = HIGH;

// --- ตัวแปรสำหรับเกมที่ 1 (Cave Escape) ---
int playerRow1 = 1;
int obstacleX = 15;
int obstacleRow = 1;
int score1 = 0;
unsigned long lastTick1 = 0;

int oldPlayerRow1 = 1;
int oldObstacleX = 15;
int oldObstacleRow = 1;

// --- ตัวแปรสำหรับเกมที่ 2 (Star Catcher) ---
int playerX2 = 7;
int itemX2 = 0;
int itemRow2 = 0;
int itemType2 = 0; 
int score2 = 0;
int highScore2 = 0;
unsigned long dropTimer2 = 0;
unsigned long moveTimer2 = 0;

int oldPlayerX2 = 7;
int oldItemX2 = 0;
int oldItemRow2 = 0;
int oldScore2 = -1;

// ประกาศฟังก์ชันล่วงหน้า ป้องกัน Error หาฟังก์ชันไม่เจอ
void runMenu();
void runCaveEscape();
void runStarCatcher();
void resetGame(int gameId);

void setup() {
  Serial.begin(9600);
  pinMode(btnPin, INPUT_PULLUP);
  
  lcd.init();
  lcd.backlight();
  
  // โหลดคะแนนสูงสุดของเกม Star Catcher จาก EEPROM
  highScore2 = EEPROM.read(100);

  // หน้าจอเริ่มต้น
  lcd.setCursor(0, 0);
  lcd.print("  FocusBox  ");
  lcd.setCursor(0, 1);
  lcd.print("   Offline  ");
  delay(2000);
  lcd.clear();
  
  menuTimer = millis(); // เริ่มจับเวลาเมนู
}

void loop() {
  // เลือกว่าจะทำอะไรตามสถานะปัจจุบัน
  if (gameState == 0) {
    runMenu();
  } else if (gameState == 1) {
    runCaveEscape();
  } else if (gameState == 2) {
    runStarCatcher();
  }
}

// ==========================================
// ระบบเมนูหลัก (Main Menu)
// ==========================================
void runMenu() {
  bool currentBtn = digitalRead(btnPin);
  
  // ตรวจจับการกดปุ่ม (สลับเมนู)
  if (currentBtn == LOW && lastButtonState == HIGH) {
    selectedGame = (selectedGame == 1) ? 2 : 1; // สลับ 1 กับ 2
    menuTimer = millis(); // รีเซ็ตเวลาหน่วง
    lcd.clear(); // ล้างจอเพื่อวาดข้อความใหม่
  }
  lastButtonState = currentBtn;

  // วาดข้อความเมนู
  lcd.setCursor(0, 0);
  lcd.print("Select Game:");
  lcd.setCursor(0, 1);
  if (selectedGame == 1) {
    lcd.print("> 1.Cave Escape ");
  } else {
    lcd.print("> 2.Star Catcher");
  }

  // ถ้าไม่มีการกดปุ่มเกิน 3 วินาที ให้เริ่มเกมที่เลือก
  if (millis() - menuTimer > 3000) {
    gameState = selectedGame;
    resetGame(gameState);
  }
}

// ==========================================
// ฟังก์ชันเตรียมตัวแปรก่อนเริ่มเกม
// ==========================================
void resetGame(int gameId) {
  lcd.clear();
  if (gameId == 1) {
    lcd.print("Starting Cave...");
    playerRow1 = 1;
    obstacleX = 15;
    score1 = 0;
    
    oldPlayerRow1 = 1;
    oldObstacleX = 15;
  } else {
    lcd.print("Star Catcher");
    lcd.setCursor(0, 1);
    lcd.print("High: ");
    lcd.print(highScore2);
    delay(1500);
    playerX2 = 7;
    score2 = 0;
    itemX2 = random(0, 13);
    itemRow2 = 0;
    
    oldPlayerX2 = 7;
    oldItemX2 = itemX2;
    oldItemRow2 = 0;
    oldScore2 = -1;
  }
  delay(1000);
  lcd.clear();
}

// ==========================================
// โค้ดเกมที่ 1: Cave Escape
// ==========================================
void runCaveEscape() {
  if (digitalRead(btnPin) == LOW) {
    playerRow1 = 0; // กดปุ่ม = ขึ้นบน
  } else {
    playerRow1 = 1; // ปล่อยปุ่ม = ลงล่าง
  }

  if (millis() - lastTick1 > 300) {
    lastTick1 = millis();
    obstacleX--;

    if (obstacleX < 0) {
      lcd.setCursor(0, obstacleRow);
      lcd.print(" "); // ลบเศษสิ่งกีดขวาง
      
      obstacleX = 15;
      obstacleRow = random(0, 2);
      score1++;
    }

    // ตรวจสอบการชน (GAME OVER)
    if (obstacleX == 1 && playerRow1 == obstacleRow) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("GAME OVER");
      lcd.setCursor(0, 1);
      lcd.print("Score: ");
      lcd.print(score1);
      delay(2000); // โชว์คะแนน 2 วินาที
      
      // กลับไปหน้าเมนู
      gameState = 0;
      menuTimer = millis(); 
      lcd.clear();
      return;
    }

    // Render 
    if (oldPlayerRow1 != playerRow1) {
      lcd.setCursor(1, oldPlayerRow1);
      lcd.print(" ");
    }
    lcd.setCursor(1, playerRow1);
    lcd.print("P");

    if (oldObstacleX != obstacleX || oldObstacleRow != obstacleRow) {
      lcd.setCursor(oldObstacleX, oldObstacleRow);
      lcd.print(" ");
    }
    lcd.setCursor(obstacleX, obstacleRow);
    lcd.print("#");

    oldPlayerRow1 = playerRow1;
    oldObstacleX = obstacleX;
    oldObstacleRow = obstacleRow;
  }
}

// ==========================================
// โค้ดเกมที่ 2: Star Catcher
// ==========================================
void runStarCatcher() {
  if (millis() - moveTimer2 > 150) {
    moveTimer2 = millis();
    if (digitalRead(btnPin) == LOW) {
      if (playerX2 < 15) playerX2++;
    } else {
      if (playerX2 > 0) playerX2--;
    }
  }

  if (millis() - dropTimer2 > 400) {
    dropTimer2 = millis();
    itemRow2++;
    
    if (itemRow2 > 1) {
      if (itemX2 == playerX2) { 
        if (itemType2 == 0) { // เก็บดาวได้
          score2++;
          if (score2 > highScore2) {
            highScore2 = score2;
            EEPROM.write(100, highScore2);
          }
        } else { // โดนระเบิด GAME OVER
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("BOOM! GAME OVER");
          lcd.setCursor(0, 1);
          lcd.print("Score: ");
          lcd.print(score2);
          delay(2000);
          
          // กลับไปหน้าเมนู
          gameState = 0;
          menuTimer = millis(); 
          lcd.clear();
          return;
        }
      }
      
      lcd.setCursor(itemX2, 1);
      lcd.print(" ");
      
      itemX2 = random(0, 13);
      itemType2 = (random(0, 4) == 0) ? 1 : 0;
      itemRow2 = 0;
    }
  }

  // Render 
  if (oldItemX2 != itemX2 || oldItemRow2 != itemRow2) {
    lcd.setCursor(oldItemX2, oldItemRow2);
    lcd.print(" ");
  }
  lcd.setCursor(itemX2, itemRow2);
  lcd.print(itemType2 == 0 ? "*" : "X");

  if (oldPlayerX2 != playerX2) {
    lcd.setCursor(oldPlayerX2, 1);
    lcd.print(" ");
  }
  lcd.setCursor(playerX2, 1);
  lcd.print("O");

  if (oldScore2 != score2) {
    lcd.setCursor(13, 0);
    if (score2 < 10) lcd.print("  "); 
    lcd.setCursor(13, 0);
    lcd.print(score2);
  }

  oldItemX2 = itemX2;
  oldItemRow2 = itemRow2;
  oldPlayerX2 = playerX2;
  oldScore2 = score2;
  
  delay(10);
}