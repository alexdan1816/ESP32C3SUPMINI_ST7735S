    #include <Arduino.h>
    #include <SPI.h>
    #include <LittleFS.h>
    // ============ CHÂN MÀN HÌNH (ESP32-C3) ============
    #define TFT_SCLK   4   // SCL
    #define TFT_MOSI   6   // SDA
    #define TFT_CS     7
    #define TFT_DC     9
    #define TFT_RST   10   // RES
    #define TFT_BL     3   // BLK 8
    // ============ KÍCH THƯỚC MÀN HÌNH ============
    #define TFT_WIDTH  132
    #define TFT_HEIGHT 160

    #define TFT_WIDTH_OFFSET 24
    #define TFT_HEIGHT_OFFSET 2

    // --- Định nghĩa các mã lệnh ST7735S từ Datasheet ---
    #define ST7735_NOP     0x00
    #define ST7735_SWRESET 0x01  // Reset phần mềm
    #define ST7735_SLPOUT  0x11  // Thoát chế độ Sleep Mode
    #define ST7735_INVOFF  0x20  // Tắt đảo màu
    #define ST7735_INVON   0x21  // Bật đảo màu
    #define ST7735_DISPON  0x29  // Bật màn hình
    #define ST7735_CASET   0x2A  // Thiết lập dải Cột (Column)
    #define ST7735_RASET   0x2B  // Thiết lập dải Hàng (Row)
    #define ST7735_RAMWR   0x2C  // Ghi dữ liệu vào RAM
    #define ST7735_MADCTL  0x36  // Điều khiển quét hướng & màu RGB/BGR
    #define ST7735_COLMOD  0x3A  // Cấu hình định dạng màu (Pixel Format)

    #define ST7735_FRMCTR1 0xB1  // Điều khiển tần số quét (Normal Mode)
    #define ST7735_INVCTR  0xB4  // Điều khiển đảo mức quét Display Inversion
    #define ST7735_PWCTR1  0xC0  // Điều khiển nguồn Power Control 1
    #define ST7735_PWCTR2  0xC1  // Power Control 2
    #define ST7735_PWCTR3  0xC2  // Power Control 3
    #define ST7735_VMCTR1  0xC5  // VCOM Control 1
    #define ST7735_GMCTRP1 0xE0  // Gamma Correction (+ Pole)
    #define ST7735_GMCTRN1 0xE1  // Gamma Correction (- Pole)
    #define ST7735_RDDST   0x9H


    #define FRAME_W 160
    #define FRAME_H 80
    #define FRAME_BYTES (FRAME_W * FRAME_H * 2)

    uint8_t frameBuf[FRAME_BYTES];   // 25.6 KB

    
    typedef enum {
        PORTRAIT = 0,
        LANDSCAPE = 1,
        PORTRAIT_FLIPPED = 2,
        LANDSCAPE_FLIPPED = 3
    } TFT_Orientation;

    HardwareSerial SerialPort(1);

    SPISettings mySPISettings(4000000, MSBFIRST, SPI_MODE0);

    // --- Khai báo các hàm giao tiếp phần cứng (Người dùng tự định nghĩa theo MCU) ---
    void ST7735_WriteCommand(uint8_t cmd)
    {
        SPI.beginTransaction(mySPISettings);

        digitalWrite(TFT_CS, LOW);
        digitalWrite(TFT_DC, LOW);

        SPI.write(cmd);

        digitalWrite(TFT_CS, HIGH);

        SPI.endTransaction();
    }

    void ST7735_WriteData(uint8_t data)
    {
        SPI.beginTransaction(mySPISettings);

        digitalWrite(TFT_CS, LOW);
        digitalWrite(TFT_DC, HIGH);

        SPI.write(data);

        digitalWrite(TFT_CS, HIGH);

        SPI.endTransaction();
    }


    void ST7735_Reset_Pin(uint8_t state) // Kéo chân RESX lên 1 hoặc xuống 0
    {
        digitalWrite(TFT_RST, state);
    }
    void Delay_ms(uint32_t ms)
    {
        delay(ms);
        return;
    }

    void ST7735_Init()
    {
    // 1. Reset phần cứng (Hardware Reset)
        ST7735_Reset_Pin(0);
        Delay_ms(10);
        ST7735_Reset_Pin(1);
        Delay_ms(120);

    // 2. Reset phần mềm (Software Reset)
        ST7735_WriteCommand(ST7735_SWRESET); // Lệnh 01h
        Delay_ms(120);                         // Bắt buộc chờ 120ms [1]

    // 3. Thoát chế độ ngủ (SlEEP OUT)
        ST7735_WriteCommand(ST7735_SLPOUT); // Lệnh 11h
        Delay_ms(120);                       // Bắt buộc chờ 120ms [1]

    // 4. Cấu hình tần số quét (Frame Rate Control)
        ST7735_WriteCommand(ST7735_FRMCTR1); // Lệnh B1h
        ST7735_WriteData(0x01);              // Dữ liệu 01h
        ST7735_WriteData(0x2C);              // Dữ liệu 2Ch
        ST7735_WriteData(0x2D);              // Dữ liệu 2Dh

    // 5. Cấu hình điện áp nguồn nội 
        ST7735_WriteCommand(ST7735_PWCTR1);  // Lệnh C0h [3]
        ST7735_WriteData(0xA2);              // Thiết lập AVDD & VRHP [3]
        ST7735_WriteData(0x02);              // VRHN [3]
        ST7735_WriteData(0x84);
        
        ST7735_WriteCommand(ST7735_PWCTR2);  // Lệnh C1h
        ST7735_WriteData(0xC5);              // VGH & VGL

        ST7735_WriteCommand(ST7735_PWCTR3);  // Lệnh C2h
        ST7735_WriteData(0x0A);
        ST7735_WriteData(0x00);

        ST7735_WriteCommand(ST7735_VMCTR1);  // Lệnh C5h [4]
        ST7735_WriteData(0x0A);              // VCOM [4, 5]

    // 6. Cấu hình định dạng màu hiển thị (Pixel Format)
        ST7735_WriteCommand(ST7735_COLMOD);  // Lệnh 3Ah
        ST7735_WriteData(0x05);              // 0x05: Đặt chuẩn 16-bit/pixel (RGB565) [6]

    // // 7. Cấu hình hướng màn hình và thứ tự màu
    //     ST7735_WriteCommand(ST7735_MADCTL);  // Lệnh 36h
    //     ST7735_WriteData(0xC0);              // 0xC0: Chọn chế độ BGR/RGB và hướng quét MX/MY

    // 8. Cấu hình bảng Gamma (Giúp màu sắc chuẩn và sắc nét)
        ST7735_WriteCommand(ST7735_GMCTRP1); // Lệnh E0h
        ST7735_WriteData(0x02); ST7735_WriteData(0x1C); ST7735_WriteData(0x07);
        ST7735_WriteData(0x12); ST7735_WriteData(0x37); ST7735_WriteData(0x32);
        ST7735_WriteData(0x29); ST7735_WriteData(0x2D); ST7735_WriteData(0x29);
        ST7735_WriteData(0x25); ST7735_WriteData(0x2B); ST7735_WriteData(0x39);
        ST7735_WriteData(0x00); ST7735_WriteData(0x01); ST7735_WriteData(0x03); ST7735_WriteData(0x10);

        ST7735_WriteCommand(ST7735_GMCTRN1); // Lệnh E1h
        ST7735_WriteData(0x03); ST7735_WriteData(0x1D); ST7735_WriteData(0x07);
        ST7735_WriteData(0x06); ST7735_WriteData(0x2E); ST7735_WriteData(0x2C);
        ST7735_WriteData(0x29); ST7735_WriteData(0x2D); ST7735_WriteData(0x2E);
        ST7735_WriteData(0x2E); ST7735_WriteData(0x37); ST7735_WriteData(0x3F);
        ST7735_WriteData(0x00); ST7735_WriteData(0x00); ST7735_WriteData(0x02); ST7735_WriteData(0x10);

    // 9. Bật hiển thị (Display On)
        ST7735_WriteCommand(ST7735_DISPON);  // Lệnh 29h
        Delay_ms(100);

    }

    void ST7735_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
    {
        // x0 += TFT_WIDTH_OFFSET;
        // x1 += TFT_WIDTH_OFFSET;

        // y0 += TFT_HEIGHT_OFFSET;
        // y1 += TFT_HEIGHT_OFFSET;

        ST7735_WriteCommand(ST7735_CASET); // Lệnh 2Ah
        ST7735_WriteData(x0 >> 8);
        ST7735_WriteData(x0 & 0xFF);
        ST7735_WriteData(x1 >> 8);
        ST7735_WriteData(x1 & 0xFF);

        ST7735_WriteCommand(ST7735_RASET); // Lệnh 2Bh
        ST7735_WriteData(y0 >> 8);
        ST7735_WriteData(y0 & 0xFF);
        ST7735_WriteData(y1 >> 8);
        ST7735_WriteData(y1 & 0xFF);

        ST7735_WriteCommand(ST7735_RAMWR); // Lệnh 2Ch
    }

    void ST7735_SetHorizontalScroll(TFT_Orientation start) // top left, bottom left, top right, bottom right
    {
        switch (start) {
            case PORTRAIT: // MY = 0, MX = 0, MV = 0, 
                ST7735_WriteCommand(ST7735_MADCTL);
                ST7735_WriteData(0x00);
                ST7735_SetAddressWindow(26, 0, TFT_WIDTH - TFT_WIDTH_OFFSET - 1, TFT_HEIGHT - 1);
                break;
            case LANDSCAPE:
                ST7735_WriteCommand(ST7735_MADCTL);
                ST7735_WriteData(0x60); //0X60 = 0110 0000
                ST7735_SetAddressWindow(0, 26, TFT_HEIGHT - 1, TFT_WIDTH - TFT_WIDTH_OFFSET - 1);
                break;
            case PORTRAIT_FLIPPED:
                ST7735_WriteCommand(ST7735_MADCTL);
                ST7735_WriteData(0xC0); //0XA0 = 1100 0000
                // ST7735_SetAddressWindow(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
                break;
            case LANDSCAPE_FLIPPED:
                ST7735_WriteCommand(ST7735_MADCTL);
                ST7735_WriteData(0xA0); //0XA0 = 1010 000
                // ST7735_SetAddressWindow(0, 0, TFT_HEIGHT - 1, TFT_WIDTH - 1);
                break;
        }
    }

    void ST7735_FillScreen(uint16_t color)
    {
        ST7735_WriteCommand(0x2C); 
        for (int i = 0; i < (TFT_WIDTH - 52) * TFT_HEIGHT; i++) {
            ST7735_WriteData(color >> 8);
            ST7735_WriteData(color & 0xFF);
        }
    }

    void ST7735_DrawImage(const uint16_t *img)
    {
        ST7735_WriteCommand(0x2C); 
        for (uint32_t i = 0; i <(TFT_WIDTH - 52) * TFT_HEIGHT ; i++) {
            uint16_t c = img[i];
            ST7735_WriteData(c >> 8);
            ST7735_WriteData(c & 0xFF);
        }
    }   

void playAnim(const char *path, uint16_t fps)
{
    File f = LittleFS.open(path, "r");
    if (!f) {
        Serial.println("Khong mo duoc file");
        delay(1000);
        return;
    }

    const uint32_t framePeriod = 1000 / fps;   // ms mỗi frame
    uint32_t next = millis();

    while (f.read(frameBuf, FRAME_BYTES) == FRAME_BYTES) {
        ST7735_SetAddressWindow(0, 26, FRAME_W - 1, 26 + FRAME_H - 1);

        SPI.beginTransaction(mySPISettings);
        digitalWrite(TFT_CS, LOW);
        digitalWrite(TFT_DC, HIGH);
        SPI.writeBytes(frameBuf, FRAME_BYTES);
        digitalWrite(TFT_CS, HIGH);
        SPI.endTransaction();

        next += framePeriod;
        int32_t wait = (int32_t)(next - millis());
        if (wait > 0) delay(wait);     // nhanh quá thì đợi, chậm thì không đợi
    }
    f.close();
}
    void setup() {
        Serial.begin(9600);
if (!LittleFS.begin()) {
    Serial.println("LittleFS mount loi (chua uploadfs?)");
}        SerialPort.begin(115200, SERIAL_8N1, 20, 21);

        pinMode(TFT_CS, OUTPUT);
        pinMode(TFT_DC, OUTPUT);
        pinMode(TFT_RST, OUTPUT);
        digitalWrite(TFT_CS, HIGH); // Mặc định thả CS lên cao
        digitalWrite(TFT_RST, HIGH);
        // Bắt đầu Hardware SPI cho C3
        SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);  
        SPISettings mySPISettings(20000000, MSBFIRST, SPI_MODE0);
        pinMode(8, OUTPUT);
        // Khởi tạo màn hình ST7735S
        ST7735_Init();
        ST7735_SetHorizontalScroll(LANDSCAPE);
        // ST7735_FillScreen(0x0000); // Màu trắng
        ST7735_FillScreen(0xFFFF); // Màu trắng
    }

void loop() {
    playAnim("/anim.raw", 20);
}