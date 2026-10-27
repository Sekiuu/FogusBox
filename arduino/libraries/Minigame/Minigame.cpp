#include "Minigame.h"

Minigame::Minigame(LiquidCrystal_I2C &lcd, uint8_t buttonPin, int eepromAddr)
  : _lcd(lcd),
    _btnPin(buttonPin),
    _eepromAddr(eepromAddr),
    _menuTimeoutMs(3000),
    _autoReturnToMenu(true),
    _gameState(GAME_STATE_MENU),
    _selectedGame(1),
    _menuTimer(0),
    _lastButtonState(HIGH),
    // Cave Escape defaults
    _playerRow1(1),
    _obstacleX(15),
    _obstacleRow(1),
    _score1(0),
    _lastTick1(0),
    _oldPlayerRow1(1),
    _oldObstacleX(15),
    _oldObstacleRow(1),
    // Star Catcher defaults
    _playerX2(7),
    _itemX2(0),
    _itemRow2(0),
    _itemType2(0),
    _score2(0),
    _highScore2(0),
    _dropTimer2(0),
    _moveTimer2(0),
    _oldPlayerX2(7),
    _oldItemX2(0),
    _oldItemRow2(0),
    _oldScore2(-1) {
}

void Minigame::begin(bool showSplash) {
  pinMode(_btnPin, INPUT_PULLUP);

  // อ่าน High Score จาก EEPROM
  _highScore2 = EEPROM.read(_eepromAddr);
  if (_highScore2 == 255) {
    _highScore2 = 0; // ป้องกันกรณี EEPROM ยังว่างเปล่า (ค่าเริ่มต้นของ flash เป็น 0xFF)
  }

  if (showSplash) {
    showSplashScreen();
    openMenu();
  } else {
    _gameState = GAME_STATE_IDLE;
  }
}

void Minigame::showSplashScreen() {
  _lcd.clear();
  _lcd.setCursor(0, 0);
  _lcd.print("  FocusBox  ");
  _lcd.setCursor(0, 1);
  _lcd.print("   Offline  ");
  delay(2000);
  _lcd.clear();
}

void Minigame::openMenu() {
  _gameState = GAME_STATE_MENU;
  _menuTimer = millis();
  _lastButtonState = digitalRead(_btnPin);
  _lcd.clear();
}

void Minigame::startGame(GameState game) {
  if (game == GAME_STATE_CAVE_ESCAPE || game == GAME_STATE_STAR_CATCHER) {
    _gameState = game;
    resetGame((int)game);
  }
}

void Minigame::startCaveEscape() {
  startGame(GAME_STATE_CAVE_ESCAPE);
}

void Minigame::startStarCatcher() {
  startGame(GAME_STATE_STAR_CATCHER);
}

void Minigame::stop() {
  _gameState = GAME_STATE_IDLE;
  _lcd.clear();
}

GameState Minigame::getState() const {
  return _gameState;
}

bool Minigame::isActive() const {
  return (_gameState != GAME_STATE_IDLE);
}

bool Minigame::isPlaying() const {
  return (_gameState == GAME_STATE_CAVE_ESCAPE || _gameState == GAME_STATE_STAR_CATCHER);
}

int Minigame::getScore() const {
  if (_gameState == GAME_STATE_CAVE_ESCAPE) return _score1;
  if (_gameState == GAME_STATE_STAR_CATCHER) return _score2;
  return 0;
}

int Minigame::getHighScore() const {
  return _highScore2;
}

void Minigame::resetHighScore() {
  _highScore2 = 0;
  EEPROM.update(_eepromAddr, 0);
}

void Minigame::setEEPROMAddress(int addr) {
  _eepromAddr = addr;
}

void Minigame::setMenuTimeout(unsigned long ms) {
  _menuTimeoutMs = ms;
}

void Minigame::setAutoReturnToMenu(bool autoReturn) {
  _autoReturnToMenu = autoReturn;
}

void Minigame::update() {
  if (_gameState == GAME_STATE_MENU) {
    runMenu();
  } else if (_gameState == GAME_STATE_CAVE_ESCAPE) {
    runCaveEscape();
  } else if (_gameState == GAME_STATE_STAR_CATCHER) {
    runStarCatcher();
  }
}

// ==========================================
// เมนูหลัก (Main Menu)
// ==========================================
void Minigame::runMenu() {
  bool currentBtn = digitalRead(_btnPin);

  // ตรวจจับการกดปุ่ม (สลับเกมที่เลือก)
  if (currentBtn == LOW && _lastButtonState == HIGH) {
    _selectedGame = (_selectedGame == 1) ? 2 : 1;
    _menuTimer = millis();
    _lcd.clear();
  }
  _lastButtonState = currentBtn;

  // แสดงผลข้อความบนจอ LCD
  _lcd.setCursor(0, 0);
  _lcd.print("Select Game:");
  _lcd.setCursor(0, 1);
  if (_selectedGame == 1) {
    _lcd.print("> 1.Cave Escape ");
  } else {
    _lcd.print("> 2.Star Catcher");
  }

  // หากไม่มีการกดปุ่มเกินเวลา timeout ให้เริ่มเกมที่เลือก
  if (millis() - _menuTimer > _menuTimeoutMs) {
    _gameState = (GameState)_selectedGame;
    resetGame(_selectedGame);
  }
}

// ==========================================
// เตรียมตัวแปรก่อนเริ่มเกม
// ==========================================
void Minigame::resetGame(int gameId) {
  _lcd.clear();
  if (gameId == 1) {
    _lcd.setCursor(0, 0);
    _lcd.print("Starting Cave...");
    _playerRow1 = 1;
    _obstacleX = 15;
    _obstacleRow = 1;
    _score1 = 0;

    _oldPlayerRow1 = 1;
    _oldObstacleX = 15;
    _oldObstacleRow = 1;
    _lastTick1 = millis();
    delay(1000);
  } else {
    _lcd.setCursor(0, 0);
    _lcd.print("Star Catcher");
    _lcd.setCursor(0, 1);
    _lcd.print("High: ");
    _lcd.print(_highScore2);
    delay(1500);

    _playerX2 = 7;
    _score2 = 0;
    _itemX2 = random(0, 13);
    _itemRow2 = 0;
    _itemType2 = (random(0, 4) == 0) ? 1 : 0;

    _oldPlayerX2 = 7;
    _oldItemX2 = _itemX2;
    _oldItemRow2 = 0;
    _oldScore2 = -1;
    _dropTimer2 = millis();
    _moveTimer2 = millis();
    delay(1000);
  }
  _lcd.clear();
}

// ==========================================
// เกมที่ 1: Cave Escape
// ==========================================
void Minigame::runCaveEscape() {
  if (digitalRead(_btnPin) == LOW) {
    _playerRow1 = 0; // กดปุ่ม = ขึ้นบน
  } else {
    _playerRow1 = 1; // ปล่อยปุ่ม = ลงล่าง
  }

  // วาดตัวผู้เล่นทันทีเมื่อเปลี่ยนแถว เพื่อความลื่นไหลในการควบคุม
  if (_oldPlayerRow1 != _playerRow1) {
    _lcd.setCursor(1, _oldPlayerRow1);
    _lcd.print(" ");
    _lcd.setCursor(1, _playerRow1);
    _lcd.print("P");
    _oldPlayerRow1 = _playerRow1;
  }

  if (millis() - _lastTick1 > 300) {
    _lastTick1 = millis();
    _obstacleX--;

    if (_obstacleX < 0) {
      _lcd.setCursor(0, _obstacleRow);
      _lcd.print(" "); // ลบเศษสิ่งกีดขวางที่ตกขอบจอ

      _obstacleX = 15;
      _obstacleRow = random(0, 2);
      _score1++;
    }

    // ตรวจสอบการชน (GAME OVER)
    if (_obstacleX == 1 && _playerRow1 == _obstacleRow) {
      showGameOver("GAME OVER", _score1);
      if (_autoReturnToMenu) {
        openMenu();
      } else {
        _gameState = GAME_STATE_IDLE;
      }
      return;
    }

    // วาดสิ่งกีดขวาง
    if (_oldObstacleX != _obstacleX || _oldObstacleRow != _obstacleRow) {
      _lcd.setCursor(_oldObstacleX, _oldObstacleRow);
      _lcd.print(" ");
    }
    _lcd.setCursor(_obstacleX, _obstacleRow);
    _lcd.print("#");

    // วาดตัวผู้เล่นทับตำแหน่ง
    _lcd.setCursor(1, _playerRow1);
    _lcd.print("P");

    _oldPlayerRow1 = _playerRow1;
    _oldObstacleX = _obstacleX;
    _oldObstacleRow = _obstacleRow;
  }
}

// ==========================================
// เกมที่ 2: Star Catcher
// ==========================================
void Minigame::runStarCatcher() {
  // การเคลื่อนที่ของผู้เล่น
  if (millis() - _moveTimer2 > 150) {
    _moveTimer2 = millis();
    if (digitalRead(_btnPin) == LOW) {
      if (_playerX2 < 15) _playerX2++;
    } else {
      if (_playerX2 > 0) _playerX2--;
    }
  }

  // ไอเทมตกลงมา
  if (millis() - _dropTimer2 > 400) {
    _dropTimer2 = millis();
    _itemRow2++;

    if (_itemRow2 > 1) {
      if (_itemX2 == _playerX2) {
        if (_itemType2 == 0) {
          // เก็บดาวสำเร็จ
          _score2++;
          if (_score2 > _highScore2) {
            _highScore2 = _score2;
            EEPROM.update(_eepromAddr, (uint8_t)_highScore2);
          }
        } else {
          // โดนระเบิด GAME OVER
          showGameOver("BOOM! GAME OVER", _score2);
          if (_autoReturnToMenu) {
            openMenu();
          } else {
            _gameState = GAME_STATE_IDLE;
          }
          return;
        }
      }

      // ลบดาว/ระเบิดแถวล่าง
      _lcd.setCursor(_itemX2, 1);
      _lcd.print(" ");

      // สุ่มไอเทมใหม่
      _itemX2 = random(0, 13);
      _itemType2 = (random(0, 4) == 0) ? 1 : 0;
      _itemRow2 = 0;
    }
  }

  // Render ไอเทม
  if (_oldItemX2 != _itemX2 || _oldItemRow2 != _itemRow2) {
    _lcd.setCursor(_oldItemX2, _oldItemRow2);
    _lcd.print(" ");
  }
  _lcd.setCursor(_itemX2, _itemRow2);
  _lcd.print(_itemType2 == 0 ? "*" : "X");

  // Render ผู้เล่น
  if (_oldPlayerX2 != _playerX2) {
    _lcd.setCursor(_oldPlayerX2, 1);
    _lcd.print(" ");
  }
  _lcd.setCursor(_playerX2, 1);
  _lcd.print("O");

  // Render คะแนน
  if (_oldScore2 != _score2) {
    _lcd.setCursor(13, 0);
    if (_score2 < 10) _lcd.print("  ");
    _lcd.setCursor(13, 0);
    _lcd.print(_score2);
  }

  _oldItemX2 = _itemX2;
  _oldItemRow2 = _itemRow2;
  _oldPlayerX2 = _playerX2;
  _oldScore2 = _score2;
}

// ==========================================
// แสดงผลหน้าจอ Game Over
// ==========================================
void Minigame::showGameOver(const char *title, int score) {
  _lcd.clear();
  _lcd.setCursor(0, 0);
  _lcd.print(title);
  _lcd.setCursor(0, 1);
  _lcd.print("Score: ");
  _lcd.print(score);
  delay(2000);
  _lcd.clear();
}
