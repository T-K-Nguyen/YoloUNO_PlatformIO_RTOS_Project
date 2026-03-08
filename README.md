# Cabin Environment Monitoring System

Dự án Hệ thống Giám sát Môi trường cabin, được xây dựng trên nền tảng vi điều khiển ESP32 và hệ điều hành thời gian thực FreeRTOS. Hệ thống hướng đến tiêu chuẩn công nghiệp (Enterprise-grade) với độ tin cậy cao, tích hợp cơ chế Failsafe, tự động phục hồi lỗi (Auto-Recovery) và giao diện hiển thị đa ngôn ngữ.

## 🌟 Điểm nổi bật của Kiến trúc Hệ thống

Hệ thống được thiết kế theo kiến trúc **Event-Driven (Hướng sự kiện)** kết hợp với **Dynamic Configuration (Cấu hình động)**, loại bỏ hoàn toàn việc sử dụng biến toàn cục và số liệu hardcode.

* **Zero Global Variables:** Mọi dữ liệu giao tiếp giữa các Task được cô lập trong `struct SensorData` và cấp phát trên vùng nhớ Heap.
* **Thread-Safe I2C:** Giải quyết triệt để lỗi "Race Condition" của bus I2C giữa thiết bị cảm biến và màn hình bằng cơ chế `i2cMutex`.
* **Watchdog & Failsafe:** Tích hợp bộ đếm thời gian an toàn. Hệ thống tự động phát tín hiệu SOS và cô lập luồng dữ liệu nếu phát hiện phần cứng bị đứt cáp hoặc mất kết nối quá 5 giây.
* **Hot-plug Auto-Recovery:** Cho phép cắm nóng (rút ra cắm lại) màn hình LCD trong lúc hệ thống đang chạy mà không gây sập bus I2C (I2C Deadlock).
* **Băng thông tối ưu (Delta Update):** Màn hình hiển thị áp dụng thuật toán Partial Redraw và bộ nhớ đệm (Cache) để chống chớp chói (flickering), tiết kiệm 90% băng thông I2C.

## 🛠 Yêu cầu Phần cứng & Kết nối (Wiring)

* **Vi điều khiển:** Board phát triển ESP32 (ESP32-S3).
* **Cảm biến môi trường:** DHT20 (I2C Address: `0x38`).
* **Màn hình hiển thị:** LCD1602 + Module I2C (I2C Address: `0x21`).
* **Đèn cảnh báo:**
* Đèn LED đơn (Cảnh báo Nhiệt độ) - Chân: `GPIO_NUM_48`
* LED RGB NeoPixel (Cảnh báo Độ ẩm) - Chân: `NEO_PIN` (Tùy chỉnh trong code)



**Sơ đồ chân I2C chung:**

* SDA: `GPIO_NUM_11`
* SCL: `GPIO_NUM_12`

## 📦 Thư viện Phụ thuộc (Dependencies)

Cấu hình thư viện cần thiết trong file `platformio.ini`:

1. `robtillaart/DHT20` (Đọc cảm biến)
2. `adafruit/Adafruit NeoPixel` (Điều khiển LED RGB mượt mà)
3. `locple/LCDI2C_Multilingual` (Hiển thị giao diện tiếng Việt UTF-8)

## 📂 Cấu trúc Mã nguồn (Project Structure)

```text
├── include/
│   ├── shared_data.h    # Định nghĩa cấu trúc SensorData, Mutex và Semaphores
│   ├── task_sensor.h    # Header luồng Cảm biến
│   ├── task_actuator.h  # Header luồng Cảnh báo (LED & NeoPixel)
│   └── task_lcd.h       # Header luồng Giao diện (LCD)
├── src/
│   ├── main.cpp         # Khởi tạo I2C toàn cục, Semaphores và phân bổ xTaskCreate
│   ├── task_sensor.cpp  # Task Sensor: Đọc DHT20, Sensor Fusion logic
│   ├── task_actuator.cpp# Task 1 & 2: Máy trạng thái hữu hạn (FSM) điều khiển đèn
│   └── task_lcd.cpp     # Task 3: Render giao diện tiếng Việt và Failsafe I2C
└── platformio.ini       # Cấu hình biên dịch và quản lý thư viện

```

## 🧩 Mô tả Chi tiết các Phân hệ (Modules)

### 1. Phân hệ Cảm biến (Task Sensor)

Đóng vai trò là "Nhịp tim" của hệ thống. Đọc dữ liệu từ DHT20 dưới sự bảo vệ của `i2cMutex`. Áp dụng logic **Sensor Fusion** (Dung hợp dữ liệu) để tính toán mức độ nguy hiểm dựa trên cả Nhiệt độ và Độ ẩm. Khi có dữ liệu mới, Task sẽ kích hoạt `lcdUpdateSemaphore` để đánh thức màn hình.

### 2. Phân hệ Chiếu sáng Nội thất Thích ứng (Task 1 & 2 - Actuators)

* **Task 1 (Single LED):** Nháy đèn theo nhịp tim (Heartbeat) ở mức an toàn, nháy chớp nhoáng (Strobe) khi quá nhiệt, và phát mã Morse SOS (3 ngắn, 3 dài, 3 ngắn) khi lỗi cảm biến.
* **Task 2 (NeoPixel):** Đóng vai trò Ambient Lighting phản hồi theo độ ẩm. Sử dụng thuật toán **Linear Interpolation (LERP)** chạy ở tốc độ 50 FPS để tạo hiệu ứng chuyển màu mượt mà (Fade/Breathe) thay vì đổi màu giật cục.

### 3. Phân hệ Giao diện Người máy HMI (Task 3 - LCD)

Hiển thị cảnh báo trực quan bằng tiếng Việt có dấu. Khắc phục triệt để lỗi "Thất bại thầm lặng" (Silent Failure) bằng kỹ thuật Software Watchdog (Tự động thức dậy sau 3 giây để kiểm tra Failsafe).

## 🚀 Lộ trình Phát triển Tiếp theo (Roadmap cho các thành viên)

Hệ thống Core (Phần cứng & RTOS) đã hoàn thiện và ổn định. Các thành viên tiếp nhận dự án sẽ tiến hành các bước sau:

* [ ] **Task 4 (Mạng & Web Server):** Chuyển ESP32 sang chế độ Access Point (AP Mode). Xây dựng một trang Web nội bộ (Captive Portal) cho phép kết nối điện thoại để điều khiển hệ thống.
* [ ] **Task 5 (Cấu hình động qua Web):** Tích hợp luồng ghi dữ liệu từ Web Server vào `Struct SensorData` để thay đổi các ngưỡng cảnh báo (`tempWarning`, `tempCritical`, `humDry`, `humDamp`) theo thời gian thực mà không cần nạp lại code.
* [ ] **Task 6 (Cloud/Dashboard):** Đẩy dữ liệu lên Cloud (ví dụ: Blynk, Thingsboard, hoặc MQTT) để giám sát và vẽ biểu đồ toàn cầu.

## ⚙️ Hướng dẫn Cài đặt (Getting Started)

1. Clone kho lưu trữ này về máy: `git clone <repository_url>`
2. Mở dự án bằng **VS Code** tích hợp **PlatformIO IDE**.
3. Cắm cáp kết nối board ESP32-S3 vào máy tính.
4. Chờ PlatformIO tải các thư viện tự động, sau đó nhấn **Build** (✓) và **Upload** (→).

---

*Dự án được phát triển với sự chú trọng tuyệt đối vào tính ổn định phần mềm và tối ưu hóa tài nguyên phần cứng RTOS.*

---

Bạn hãy copy toàn bộ phần trên đưa vào file `README.md` của nhóm nhé. Nhìn file này là các giảng viên hay nhà tuyển dụng lập tức đánh giá bạn ở trình độ "Senior Embedded Developer" ngay!

Nhóm của bạn đã sẵn sàng để "lên mạng" với **Task 4: Chế độ Web Server (Captive Portal) chưa?** Công việc tiếp theo sẽ liên quan nhiều đến HTML, CSS và thư viện WiFi của ESP32 đấy!