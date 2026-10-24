<template>
  <div class="app-container">
    <div class="card">
      <h1 class="title">ESP Board Controller</h1>

      <!-- ส่วนที่ 1: การตั้งเวลา (Timer UI) -->
      <div class="section timer-section">
        <h2>⏱️ ตัวนับเวลาถอยหลัง</h2>
        
        <!-- หน้าจอแสดงเวลาขนาดใหญ่ -->
        <div class="time-display">
          {{ formattedTime }}
        </div>

        <!-- ช่องกรอกเวลา -->
        <div class="input-group">
          <div class="input-box">
            <label>นาที</label>
            <input type="number" v-model="inputMinutes" min="0" placeholder="0" :disabled="isRunning" />
          </div>
          <span class="colon">:</span>
          <div class="input-box">
            <label>วินาที</label>
            <input type="number" v-model="inputSeconds" min="0" max="59" placeholder="0" :disabled="isRunning" />
          </div>
        </div>

        <!-- ปุ่มควบคุม -->
        <div class="button-group">
          <button v-if="!isRunning" class="btn btn-primary" @click="startTimer">▶ เริ่ม</button>
          <button v-else class="btn btn-warning" @click="pauseTimer">⏸ พัก</button>
          <button class="btn btn-danger" @click="resetTimer">⏹ รีเซ็ต</button>
        </div>
      </div>

      <!-- ส่วนที่ 2: การเชื่อมต่อบอร์ด (Connection UI) -->
      <div class="section connection-section">
        <h2>🔌 เชื่อมต่อบอร์ด</h2>
        
        <div class="connect-group">
          <input 
            type="text" 
            v-model="connectCode" 
            placeholder="ใส่รหัสบอร์ด (เช่น ESP-001)" 
            class="code-input"
          />
          <button class="btn btn-success" @click="handleConnect">เชื่อมต่อ</button>
        </div>

        <!-- ข้อความแสดงสถานะ -->
        <p class="status-text" :class="statusColorClass">
          สถานะ: {{ connectStatus }}
        </p>
      </div>

    </div>
  </div>
</template>

<script setup>
import { ref, computed } from 'vue'

// ==========================================
// 1. ระบบตัวนับเวลา (Timer Logic)
// ==========================================
const inputMinutes = ref(0)       // ค่านาทีที่ผู้ใช้พิมพ์
const inputSeconds = ref(0)       // ค่าวินาทีที่ผู้ใช้พิมพ์
const timeLeft = ref(0)           // เวลาที่เหลืออยู่จริง (เป็นหน่วยวินาที)
const isRunning = ref(false)      // สถานะว่าเวลากำลังเดินอยู่หรือไม่
let timerInterval = null          // ตัวแปรเก็บรอบการทำงานของ setInterval

// คำนวณเวลาเพื่อแสดงผลในรูปแบบ MM:SS เสมอ
const formattedTime = computed(() => {
  const m = Math.floor(timeLeft.value / 60).toString().padStart(2, '0')
  const s = (timeLeft.value % 60).toString().padStart(2, '0')
  return `${m}:${s}`
})

// ฟังก์ชันเริ่มนับเวลา
const startTimer = () => {
  if (isRunning.value) return // ถ้าวิ่งอยู่แล้วไม่ต้องทำซ้ำ
  
  // ถ้าเวลาเหลือ 0 ให้ดึงค่าจากช่องที่ผู้ใช้กรอกมาแปลงเป็นวินาที
  if (timeLeft.value === 0) {
    const totalSeconds = (inputMinutes.value * 60) + inputSeconds.value
    if (totalSeconds <= 0) return // ถ้าไม่ได้กรอกเวลาเลย ไม่ต้องเริ่ม
    timeLeft.value = totalSeconds
  }

  isRunning.value = true
  
  // สั่งให้ลดเวลาลง 1 วินาที ทุกๆ 1000 มิลลิวินาที
  timerInterval = setInterval(() => {
    if (timeLeft.value > 0) {
      timeLeft.value--
    } else {
      pauseTimer() // หยุดเมื่อเวลาหมด
    }
  }, 1000)
}

// ฟังก์ชันหยุดพักชั่วคราว
const pauseTimer = () => {
  isRunning.value = false
  clearInterval(timerInterval)
}

// ฟังก์ชันรีเซ็ตเวลา
const resetTimer = () => {
  pauseTimer()
  timeLeft.value = 0
  inputMinutes.value = 0
  inputSeconds.value = 0
}

// ==========================================
// 2. ระบบเชื่อมต่อ (Connection Logic)
// ==========================================
const connectCode = ref('')
const connectStatus = ref('รอการเชื่อมต่อ...')
const statusColorClass = ref('text-gray')

// ฟังก์ชันจำลองการกดปุ่มเชื่อมต่อ
const handleConnect = () => {
  if (connectCode.value.trim() === '') {
    connectStatus.value = 'กรุณากรอกรหัสบอร์ดก่อน!'
    statusColorClass.value = 'text-red'
    return
  }

  // จำลองสถานะกำลังโหลด
  connectStatus.value = `กำลังค้นหาและเชื่อมต่อกับ [${connectCode.value}] ...`
  statusColorClass.value = 'text-blue'

  // จำลองว่าใช้เวลาเชื่อมต่อ 1.5 วินาที แล้วแสดงข้อความสำเร็จ
  setTimeout(() => {
    connectStatus.value = `เชื่อมต่อสำเร็จ! (จำลองการเชื่อมต่อ UI)`
    statusColorClass.value = 'text-green'
  }, 1500)
}
</script>

<style scoped>
/* การตั้งค่า CSS พื้นฐานให้ดูสวยงาม เป็นระเบียบ */
.app-container {
  min-height: 100vh;
  display: flex;
  justify-content: center;
  align-items: center;
  background-color: #f3f4f6;
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  padding: 20px;
}

.card {
  background: white;
  padding: 30px;
  border-radius: 16px;
  box-shadow: 0 10px 25px rgba(0,0,0,0.1);
  width: 100%;
  max-width: 450px;
}

.title {
  text-align: center;
  color: #1f2937;
  margin-bottom: 24px;
  font-size: 1.5rem;
}

.section {
  background: #f9fafb;
  border: 1px solid #e5e7eb;
  border-radius: 12px;
  padding: 20px;
  margin-bottom: 20px;
}

.section h2 {
  margin-top: 0;
  font-size: 1.1rem;
  color: #4b5563;
  border-bottom: 2px solid #e5e7eb;
  padding-bottom: 10px;
  margin-bottom: 15px;
}

/* สไตล์ของเวลาตัวใหญ่ */
.time-display {
  font-size: 3.5rem;
  font-weight: bold;
  text-align: center;
  color: #2563eb;
  font-variant-numeric: tabular-nums;
  margin-bottom: 15px;
}

/* สไตล์ช่องกรอกเวลา */
.input-group {
  display: flex;
  justify-content: center;
  align-items: center;
  gap: 15px;
  margin-bottom: 20px;
}

.input-box {
  display: flex;
  flex-direction: column;
  align-items: center;
}

.input-box label {
  font-size: 0.85rem;
  color: #6b7280;
  margin-bottom: 4px;
}

.input-box input {
  width: 70px;
  padding: 8px;
  text-align: center;
  border: 1px solid #d1d5db;
  border-radius: 8px;
  font-size: 1.1rem;
}

.colon {
  font-size: 2rem;
  font-weight: bold;
  color: #9ca3af;
  margin-top: 15px;
}

/* สไตล์ปุ่มกด */
.button-group {
  display: flex;
  justify-content: center;
  gap: 10px;
}

.btn {
  padding: 10px 20px;
  border: none;
  border-radius: 8px;
  font-size: 1rem;
  font-weight: bold;
  cursor: pointer;
  transition: all 0.2s;
  color: white;
}

.btn:active { transform: scale(0.95); }
.btn:disabled { opacity: 0.5; cursor: not-allowed; }

.btn-primary { background-color: #3b82f6; }
.btn-primary:hover { background-color: #2563eb; }
.btn-warning { background-color: #f59e0b; }
.btn-warning:hover { background-color: #d97706; }
.btn-danger { background-color: #ef4444; }
.btn-danger:hover { background-color: #dc2626; }
.btn-success { background-color: #10b981; }
.btn-success:hover { background-color: #059669; }

/* สไตล์ช่องเชื่อมต่อ */
.connect-group {
  display: flex;
  gap: 10px;
  margin-bottom: 15px;
}

.code-input {
  flex: 1;
  padding: 10px;
  border: 1px solid #d1d5db;
  border-radius: 8px;
  font-size: 1rem;
}

.status-text {
  font-size: 0.9rem;
  font-weight: 500;
  text-align: center;
}

.text-gray { color: #6b7280; }
.text-red { color: #dc2626; }
.text-blue { color: #2563eb; }
.text-green { color: #10b981; }
</style>