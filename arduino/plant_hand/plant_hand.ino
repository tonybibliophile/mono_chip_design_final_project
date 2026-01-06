#include <Servo.h>

// ====== 是否使用右手伺服（只有一顆就設 false）======
static const bool USE_RIGHT = true;

// ====== 腳位設定 ======
static const uint8_t PIN_L = 9;
static const uint8_t PIN_R = 10;

Servo servoL;
Servo servoR;

// ====== 角度校正（你一定要依照實際安裝微調）======
// 建議你先把手臂裝在「伺服 90 度」附近當中立，再調下面三個角度
int L_NEUTRAL = 90;
int L_UP      = 60;   // 上擺（或往前/往上）
int L_DOWN    = 120;  // 下擺（或往後/往下）

// 右手通常鏡像：若方向相反，就把 UP/DOWN 對調或改數值
int R_NEUTRAL = 90;
int R_UP      = 120;
int R_DOWN    = 60;

// ====== 平滑移動參數（會依模式動態調整）=====
int STEP_DEG = 1;          // 每次移動幾度
int STEP_MS  = 10;         // 每步延遲（越小越快）

// ====== 動作辨識度用的額外參數（讓 u/w/h/p 更突出）=====
int BIG_AMP   = 35;        // 大動作幅度（party）
int WAVE_AMP  = 18;        // 揮手幅度（wave）
int SHAKE_AMP = 12;        // 抖動幅度（angry）

// ====== 動作狀態 ======
enum Mode {
  MODE_IDLE,
  MODE_UPDOWN,
  MODE_WAVE,
  MODE_HUG,
  MODE_PARTY,
  MODE_ANGRY,
  MODE_STOP
};

volatile Mode mode = MODE_IDLE;

// ====== 非阻塞排程用 ======
unsigned long tPrev = 0;
int phase = 0;

// ====== 動作停頓控制（端點停一下會更像動作）=====
unsigned long holdUntil = 0;

// 小工具：安全寫角度
int clampAngle(int a) { return (a < 0) ? 0 : (a > 180 ? 180 : a); }

void writeBoth(int aL, int aR) {
  servoL.write(clampAngle(aL));
  if (USE_RIGHT) servoR.write(clampAngle(aR));
}

void centerHands() {
  writeBoth(L_NEUTRAL, R_NEUTRAL);
}

// 以「目前角度」逐步逼近目標角度（平滑）
void stepToward(int targetL, int targetR) {
  int curL = servoL.read();
  int nextL = curL + (targetL > curL ? STEP_DEG : (targetL < curL ? -STEP_DEG : 0));
  servoL.write(clampAngle(nextL));

  if (USE_RIGHT) {
    int curR = servoR.read();
    int nextR = curR + (targetR > curR ? STEP_DEG : (targetR < curR ? -STEP_DEG : 0));
    servoR.write(clampAngle(nextR));
  }
}

// 判斷左手是否到位（夠用）
bool reachedL(int targetL) {
  return abs(servoL.read() - targetL) <= STEP_DEG;
}

// ====== 各模式的「目標角度序列」======
void runMode() {
  unsigned long now = millis();
  if (now - tPrev < (unsigned long)STEP_MS) return;
  tPrev = now;

  // 若正在停頓，先不更新 phase（讓節奏出來）
  if (now < holdUntil) return;

  switch (mode) {
    case MODE_IDLE: {
      // 小幅呼吸/擺動：慢、小、穩
      int lA = L_NEUTRAL - 8;
      int lB = L_NEUTRAL + 8;
      int rA = R_NEUTRAL + 8;
      int rB = R_NEUTRAL - 8;

      if (phase == 0) stepToward(lA, rA);
      if (phase == 1) stepToward(lB, rB);

      if (reachedL(phase == 0 ? lA : lB)) {
        phase = (phase + 1) % 2;
      }
    } break;

    case MODE_UPDOWN: {
      // 上下擺動：幅度最大 + 端點停頓（更像“點頭/擺手”）
      int targetL = (phase == 0) ? L_UP : L_DOWN;
      int targetR = (phase == 0) ? R_UP : R_DOWN;

      stepToward(targetL, targetR);

      if (reachedL(targetL)) {
        holdUntil = now + 250;          // 端點停一下，辨識度大幅提升
        phase = (phase + 1) % 2;
      }
    } break;

    case MODE_WAVE: {
      // 揮手：小幅 + 快速三連 + 回中立停一下（和 u 明顯不同）
      int baseL = L_NEUTRAL;
      int baseR = R_NEUTRAL;

      int targetL;
      if      (phase == 0) targetL = baseL + WAVE_AMP;
      else if (phase == 1) targetL = baseL - WAVE_AMP;
      else if (phase == 2) targetL = baseL + WAVE_AMP;
      else                 targetL = baseL;          // 回中立（停一下）

      int targetR = baseR; // 右手固定中立

      stepToward(targetL, targetR);

      if (reachedL(targetL)) {
        if (phase == 3) holdUntil = now + 300;       // 揮完停一下
        phase = (phase + 1) % 4;
      }
    } break;

    case MODE_HUG: {
      // 擁抱：雙手慢慢合起來 -> “抱住停久一點” -> 打開
      // 假設：L_DOWN/R_DOWN 是“往內合”，L_UP/R_UP 是“打開”
      int targetL, targetR;

      if (phase == 0) { targetL = L_DOWN; targetR = R_DOWN; }  // 合起來
      else            { targetL = L_UP;   targetR = R_UP;   }  // 打開

      stepToward(targetL, targetR);

      if (reachedL(targetL)) {
        holdUntil = now + (phase == 0 ? 800 : 250);  // 抱住停久一點＝辨識度
        phase = (phase + 1) % 2;
      }
    } break;

    case MODE_PARTY: {
      // PARTY：更誇張、更快、不規則（跳舞感）
      // 4 拍大動作 + 2 拍小抖 + 回中立停
      int lBigA = L_NEUTRAL + BIG_AMP;
      int lBigB = L_NEUTRAL - BIG_AMP;
      int rBigA = R_NEUTRAL - BIG_AMP;
      int rBigB = R_NEUTRAL + BIG_AMP;

      int lJitA = L_NEUTRAL + 12;
      int lJitB = L_NEUTRAL - 12;
      int rJitA = R_NEUTRAL - 12;
      int rJitB = R_NEUTRAL + 12;

      int targetL, targetR;
      if      (phase == 0) { targetL = lBigA; targetR = rBigA; }
      else if (phase == 1) { targetL = lBigB; targetR = rBigB; }
      else if (phase == 2) { targetL = lBigA; targetR = rBigB; } // 反向
      else if (phase == 3) { targetL = lBigB; targetR = rBigA; } // 反向
      else if (phase == 4) { targetL = lJitA; targetR = rJitA; } // 小抖
      else if (phase == 5) { targetL = lJitB; targetR = rJitB; } // 小抖
      else                 { targetL = L_NEUTRAL; targetR = R_NEUTRAL; }

      stepToward(targetL, targetR);

      if (reachedL(targetL)) {
        if (phase == 6) holdUntil = now + 250;
        phase = (phase + 1) % 7;
      }
    } break;

    case MODE_ANGRY: {
      // 生氣抖動：幅度小但超快（和 wave/party 區分）
      int lTarget = (phase == 0) ? (L_NEUTRAL + SHAKE_AMP) : (L_NEUTRAL - SHAKE_AMP);
      int rTarget = (phase == 0) ? (R_NEUTRAL - SHAKE_AMP) : (R_NEUTRAL + SHAKE_AMP);

      stepToward(lTarget, rTarget);

      if (reachedL(lTarget)) {
        phase = (phase + 1) % 2;
      }
    } break;

    case MODE_STOP:
    default:
      centerHands();
      break;
  }
}

void printHelp() {
  Serial.println(F("\nCommands:"));
  Serial.println(F(" i = IDLE (small breathe)"));
  Serial.println(F(" u = UP/DOWN swing (big + hold)"));
  Serial.println(F(" w = WAVE (fast triple wave)"));
  Serial.println(F(" h = HUG (slow + long hold)"));
  Serial.println(F(" p = PARTY dance (irregular)"));
  Serial.println(F(" c = ANGRY shake (tiny + very fast)"));
  Serial.println(F(" s = STOP & center"));
  Serial.println(F(" ? = help\n"));
}

void setMode(Mode m) {
  mode = m;
  phase = 0;
  holdUntil = 0;

  // 依模式調速度（讓每個動作節奏差很多）
  switch (mode) {
    case MODE_IDLE:   STEP_MS = 16; STEP_DEG = 1; break; // 慢
    case MODE_UPDOWN: STEP_MS = 14; STEP_DEG = 1; break; // 慢大擺
    case MODE_WAVE:   STEP_MS = 6;  STEP_DEG = 2; break; // 快小幅三連
    case MODE_HUG:    STEP_MS = 16; STEP_DEG = 1; break; // 慢+停久
    case MODE_PARTY:  STEP_MS = 5;  STEP_DEG = 3; break; // 快誇張
    case MODE_ANGRY:  STEP_MS = 3;  STEP_DEG = 4; break; // 超快抖
    case MODE_STOP:   STEP_MS = 10; STEP_DEG = 1; break;
    default:          STEP_MS = 10; STEP_DEG = 1; break;
  }

  Serial.print(F("Mode = "));
  Serial.println((int)mode);
}

void setup() {
  Serial.begin(115200);

  servoL.attach(PIN_L);
  if (USE_RIGHT) servoR.attach(PIN_R);

  centerHands();
  delay(800);

  printHelp();
}

void loop() {
  // 讀序列指令（方便你 demo）
  if (Serial.available()) {
    char c = (char)Serial.read();
    if      (c == 'i') setMode(MODE_IDLE);
    else if (c == 'u') setMode(MODE_UPDOWN);
    else if (c == 'w') setMode(MODE_WAVE);
    else if (c == 'h') setMode(MODE_HUG);
    else if (c == 'p') setMode(MODE_PARTY);
    else if (c == 'c') setMode(MODE_ANGRY);
    else if (c == 's') setMode(MODE_STOP);
    else if (c == '?') printHelp();
  }

  runMode();
}
