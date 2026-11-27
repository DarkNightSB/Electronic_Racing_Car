# Dự án xe điều khiển ESP32

Dự án gồm hai ESP32 giao tiếp trực tiếp bằng ESP-NOW: tay cầm gửi lệnh tốc độ và góc lái, xe nhận lệnh để điều khiển BTS7960 và servo. Thư mục `GET_MAC` là chương trình phụ để xem địa chỉ MAC Wi-Fi STA của từng board.

## Cấu trúc thư mục

| Thư mục | Chức năng |
|---|---|
| `tay_cam/` | Đọc hai trục joystick, chuyển thành lệnh tốc độ/góc lái và gửi đến xe. |
| `xe_dieu_khien/` | Nhận gói ESP-NOW, điều khiển BTS7960, servo, xi nhan và dừng khi mất tín hiệu. |
| `GET_MAC/` | In MAC Wi-Fi STA của ESP32 ra Serial Monitor. |

Mỗi thư mục là một project PlatformIO riêng. Mở đúng thư mục project trong VS Code để build và nạp lên board tương ứng.

## Gói dữ liệu ESP-NOW

Hai project khai báo cùng cấu trúc và thứ tự trường:

```cpp
struct DuLieuDieuKhien {
  int16_t toc_do;
  int16_t goc_lai;
};
```

- `toc_do`: tay cầm ánh xạ khoảng `-1000` đến `1000`; xe dùng dấu để chọn chiều và độ lớn để đặt PWM.
- `goc_lai`: tay cầm ánh xạ khoảng `-90` đến `90`; xe chuyển thành xung servo khoảng `1000` đến `2000` µs.
- Tay cầm gửi gói mỗi khoảng 10 ms. Xe chỉ chấp nhận gói đúng kích thước cấu trúc.
- ESP-NOW được cấu hình ở Wi-Fi STA, không mã hóa, dùng kênh hiện tại (`channel = 0`). Hai board cần cùng kênh Wi-Fi.

## Địa chỉ MAC hiện cấu hình

- MAC xe: `F8:B3:B7:33:05:78`
- MAC tay cầm: `B0:CB:D8:CD:88:18`

Trong code tay cầm, `mac_xe` là MAC đích của xe. Trong code xe, `mac` là MAC peer của tay cầm. Nếu thay board, nạp `GET_MAC/` lên từng board và cập nhật địa chỉ tương ứng ở hai project.

## Chân tay cầm

| ESP32 GPIO | Tín hiệu |
|---|---|
| 33 | Joystick trái/phải, góc lái |
| 34 | Joystick lên/xuống, tốc độ |

Khi tay cầm khởi động, `hieuChinhJoystick()` lấy trung bình ADC 50 lần để xác định điểm giữa. Giữ cả hai joystick ở giữa trong khoảng nửa giây lúc khởi động.

## Chân xe

| ESP32 GPIO | Tín hiệu |
|---|---|
| 26 | BTS7960 `RPWM` |
| 27 | BTS7960 `LPWM` |
| 14 | BTS7960 `R_EN` |
| 12 | BTS7960 `L_EN` |
| 21 | Signal servo lái |
| 33 | Xi nhan trái |
| 32 | Xi nhan phải |
| 22 / 23 | TRIG / ECHO cảm biến siêu âm trái |
| 4 / 5 | TRIG / ECHO cảm biến siêu âm giữa trái |
| 17 / 16 | TRIG / ECHO cảm biến siêu âm giữa phải |
| 18 / 19 | TRIG / ECHO cảm biến siêu âm phải |

Trong code xe hiện tại, BTS7960, servo và xi nhan được cấu hình và điều khiển. Các GPIO siêu âm mới chỉ được khai báo; code hiện chưa cấu hình hoặc đọc các cảm biến này.

## Serial Monitor

Cả tay cầm và xe dùng `Serial.begin(9600)`. Đặt Serial Monitor ở **9600 baud**.

Tay cầm log ADC thô cùng giá trị `toc_do` và `goc_lai` sau chuyển đổi. Xe log tốc độ/góc lái nhận được và duty PWM BTS7960. Tay cầm gửi 100 gói/giây nhưng log mỗi vòng lặp ở 9600 baud có thể làm nghẽn Serial; khi debug, theo dõi độ trễ nếu thấy gửi hoặc điều khiển không ổn định.

## Failsafe và trạng thái hiện tại

- Xe dùng watchdog tín hiệu: sau hơn 1 giây không nhận gói hợp lệ, đặt tốc độ và góc lái về 0.
- Xi nhan trái/phải dựa trên dấu của góc lái và đổi trạng thái mỗi 300 ms.
- Chưa có phản hồi ACK từ xe về tay cầm; kết quả `esp_now_send()` ở tay cầm hiện chưa được log.
- Chân siêu âm đã dành sẵn nhưng chức năng tránh vật cản chưa được tích hợp vào code hiện tại.

## Nạp chương trình

1. Nạp `GET_MAC/` lên từng ESP32 nếu cần xác nhận MAC.
2. Cập nhật MAC của xe trong `tay_cam/src/main.cpp` và MAC tay cầm trong `xe_dieu_khien/src/main.cpp`.
3. Mở và nạp riêng hai project `tay_cam/` và `xe_dieu_khien/` lên đúng board.
4. Mở Serial Monitor ở 9600 baud để quan sát giá trị ADC và gói nhận.
5. Thử lần đầu khi xe được kê bánh khỏi mặt đất; xác nhận chiều motor và hành trình servo trước khi chạy trên mặt đất.

Hai project hiện dùng API LEDC và callback nhận ESP-NOW kiểu Arduino-ESP32 core 2.x (`ledcSetup`, `ledcAttachPin`, callback nhận MAC trực tiếp). Nếu đổi phiên bản framework, cần kiểm tra tương thích API trước khi build.
