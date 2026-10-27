#ifndef MINIGAME_H
#define MINIGAME_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

enum GameState {
  GAME_STATE_IDLE = -1,
  GAME_STATE_MENU = 0,
  GAME_STATE_CAVE_ESCAPE = 1,
  GAME_STATE_STAR_CATCHER = 2
};

class Minigame {
public:
  /**
   * @brief คอนสตรัคเตอร์ของ Minigame
   * @param lcd Reference ของ LiquidCrystal_I2C
   * @param buttonPin ขา Digital Pin ของปุ่มกด (default = 2, ใช้ INPUT_PULLUP)
   * @param eepromAddr ตำแหน่ง EEPROM ที่ใช้เก็บ High Score (default = 100)
   */
  Minigame(LiquidCrystal_I2C &lcd, uint8_t buttonPin = 2, int eepromAddr = 100);

  /**
   * @brief เริ่มต้นการทำงานของเกม โหลด High Score และแสดง Splash Screen
   * @param showSplash กำหนดว่าจะแสดงหน้าจอเริ่มต้น FocusBox หรือไม่
   */
  void begin(bool showSplash = true);

  /**
   * @brief ฟังก์ชันหลักที่ต้องเรียกใน loop() ของ Arduino
   */
  void update();

  /**
   * @brief เปิดหน้าเมนูเลือกเกม
   */
  void openMenu();

  /**
   * @brief เริ่มเกมตาม GameState (GAME_STATE_CAVE_ESCAPE หรือ GAME_STATE_STAR_CATCHER)
   */
  void startGame(GameState game);

  /**
   * @brief เริ่มเกม Cave Escape ทันที
   */
  void startCaveEscape();

  /**
   * @brief เริ่มเกม Star Catcher ทันที
   */
  void startStarCatcher();

  /**
   * @brief หยุดการทำงานของเกมและเคลียร์จอ
   */
  void stop();

  /**
   * @brief ดึงสถานะปัจจุบันของเกม
   */
  GameState getState() const;

  /**
   * @brief ตรวจสอบว่ากำลังเล่นเกมอยู่หรือไม่ (ไม่ใช่ IDLE หรือ MENU)
   */
  bool isPlaying() const;

  /**
   * @brief ดึงคะแนนปัจจุบัน
   */
  int getScore() const;

  /**
   * @brief ดึงคะแนนสูงสุด (High Score) ของเกม Star Catcher
   */
  int getHighScore() const;

  /**
   * @brief รีเซ็ตคะแนนสูงสุดใน EEPROM เป็น 0
   */
  void resetHighScore();

  /**
   * @brief กำหนดตำแหน่ง Address ของ EEPROM
   */
  void setEEPROMAddress(int addr);

  /**
   * @brief กำหนดเวลา Timeout ของเมนู (ค่าเริ่มต้น 3000 ms)
   */
  void setMenuTimeout(unsigned long ms);

  /**
   * @brief กำหนดว่าจะให้กลับสู่หน้าเมนูอัตโนมัติเมื่อ Game Over หรือไม่ (default = true)
   */
  void setAutoReturnToMenu(bool autoReturn);

private:
  LiquidCrystal_I2C &_lcd;
  uint8_t _btnPin;
  int _eepromAddr;
  unsigned long _menuTimeoutMs;
  bool _autoReturnToMenu;

  GameState _gameState;
  int _selectedGame;
  unsigned long _menuTimer;
  bool _lastButtonState;

  // --- ตัวแปรเกม Cave Escape ---
  int _playerRow1;
  int _obstacleX;
  int _obstacleRow;
  int _score1;
  unsigned long _lastTick1;
  int _oldPlayerRow1;
  int _oldObstacleX;
  int _oldObstacleRow;

  // --- ตัวแปรเกม Star Catcher ---
  int _playerX2;
  int _itemX2;
  int _itemRow2;
  int _itemType2;
  int _score2;
  int _highScore2;
  unsigned long _dropTimer2;
  unsigned long _moveTimer2;
  int _oldPlayerX2;
  int _oldItemX2;
  int _oldItemRow2;
  int _oldScore2;

  // Private helper methods
  void runMenu();
  void runCaveEscape();
  void runStarCatcher();
  void resetGame(int gameId);
  void showGameOver(const char *title, int score);
  void showSplashScreen();
};

#endif // MINIGAME_H
