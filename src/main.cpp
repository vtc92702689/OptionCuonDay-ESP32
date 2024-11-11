#include <Arduino.h>
#include <U8g2lib.h>
#include <ArduinoJson.h>
#include <OneButton.h>
#include <LittleFS.h>
#include "func.h"  // Bao gồm file header func.h để sử dụng các hàm từ func.cpp

//U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); // Khởi tạo đối tượng màn hình OLED U8G2
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); // Khởi tạo đối tượng màn hình OLED U8G2


StaticJsonDocument<200> jsonDoc;

const char* jsonString = R"()";

// Khai báo các nút

int btnSetDebounceMill = 20;  // thời gian chống nhiễu phím
int btnSetPressMill = 1000;  // thời gian nhấn giữ phím
int pIndex = 1;
int menuIndex = 1;
int totalChildren; //Tổng số tệp con trong Func loadJsonSettings
int maxValue = 0;
int minValue = 0;
int maxLength = 0; //Số kí tự hiển thị trên func showSetup
int columnIndex = 0; // Biến theo dõi hàng hiện tại (0 = đơn vị, 1 = chục, ...)
int currentValue;

byte trangThaiHoatDong = 0;
byte testModeStep = 0;
byte maxTestModeStep = 0;
byte mainStep = 0;

byte maxTestOutputStep = 0;
byte testOutputStep = 0;

OneButton btnMenu(34, false,false);
OneButton btnSet(35, false,false);
OneButton btnUp(36, false,false);
OneButton btnDown(39, false,false);
OneButton btnRun(32,false,false);
OneButton btnEstop(33,false,false);

bool explanationMode; //logic chức năng diễn giải
bool editAllowed; //logic chức năng chỉnh sửa
bool hienThiTestOutput = false;
bool chayTestMode = false;
bool daoTinHieuOutput = false;


const char* menu1;
const char* menu2;
const char* menu3;
const char* configFile = "/config.json";

String displayScreen = "index";
String setupCodeStr;
String valueStr;
String textExplanationMode;
String textStr;
String keyStr;
String ListExp[10]; // Mảng để chứa các phần chức năng Diễn giải thông số

void reSet();
void xuatXungPWM(unsigned long thoiGianDao);
void loadSetup();

void drawCenteredText(const char* text, int y) {
  int screenWidth = u8g2.getDisplayWidth();
  
  // Đo độ rộng của chuỗi
  int textWidth = u8g2.getStrWidth(text);
  // Tính toán vị trí x để căn giữa
  int x = (screenWidth - textWidth) / 2;
  u8g2.drawStr(x, y, text);           // Vẽ chuỗi căn giữa
}

void wrapText(const char* text, int16_t x, int16_t y, int16_t lineHeight, int16_t maxWidth) {   // Hàm wrapText để hiển thị văn bản xuống dòng nếu dài quá
  int16_t cursorX = x;  // Vị trí x bắt đầu in
  int16_t cursorY = y;  // Vị trí y bắt đầu in
  const char* wordStart = text;  // Vị trí bắt đầu của từ trong chuỗi
  const char* currentChar = text;  // Ký tự hiện tại đang xử lý

  while (*currentChar) {     // Vòng lặp qua từng ký tự trong chuỗi
    if (*currentChar == ' ' || *(currentChar + 1) == '\0') {    
      char word[64];   // Tạo chuỗi tạm để chứa từ hiện tại
      int len = currentChar - wordStart + 1;
      strncpy(word, wordStart, len);
      word[len] = '\0';

      int16_t textWidth = u8g2.getStrWidth(word);  // Kiểm tra nếu từ có vừa với chiều rộng màn hình
      if (cursorX + textWidth > maxWidth) {
        cursorX = x;  // Nếu từ quá dài, xuống dòng
        cursorY += lineHeight;  // Tăng vị trí y để xuống dòng
      }

      u8g2.drawStr(cursorX, cursorY, word);  // Vẽ từ lên màn hình
      cursorX += textWidth;  // Cập nhật vị trí x cho từ tiếp theo
      
      if (*currentChar == ' ') {
        cursorX += u8g2.getStrWidth(" ");  // Thêm khoảng trắng nếu ký tự là ' '
      }

      wordStart = currentChar + 1; // Di chuyển đến từ tiếp theo
    }
    currentChar++;  // Chuyển ký tự hiện tại sang ký tự tiếp theo
  }
}
void log(String mes){
  Serial.println(mes);
}

void writeFile(JsonDocument& doc, const char* path) {
    File file = LittleFS.open(path, "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }

    // Chuyển đổi JSON thành chuỗi và ghi vào tệp
    serializeJson(doc, file);
    file.close();
    Serial.println("File written successfully!");
}

void showList(int indexNum){
  menu1 = jsonDoc["main"]["main1"]["text"];
  menu2 = jsonDoc["main"]["main2"]["text"];
  menu3 = jsonDoc["main"]["main3"]["text"];

  u8g2.clearBuffer();  // Xóa bộ nhớ đệm của màn hình để vẽ mới
  u8g2.setFont(u8g2_font_crox3h_tf);  // Thiết lập font chữ thường (không đậm)

  
  u8g2.drawStr(12, 16, menu1);  // Hiển thị danh mục 1
  u8g2.drawStr(12, 32, menu2);  // Hiển thị danh mục 2
  u8g2.drawStr(12, 48, menu3);  // Hiển thị danh mục 3

  u8g2.drawStr(0, indexNum * 16, ">");  // Hiển thị mã cài đặt (tại vị trí x=0, y=18)
  u8g2.sendBuffer(); // Gửi nội dung đệm ra màn hình
}

void showText(const char* title, const char* messenger){
  u8g2.clearBuffer();  // Xóa bộ nhớ đệm của màn hình để vẽ mới
  u8g2.setFont(u8g2_font_crox3hb_tf);  // Thiết lập font chữ đậm

  drawCenteredText(title,18);
  
  u8g2.setFont(u8g2_font_crox3h_tf);  // Thiết lập font chữ thường (không đậm)
  wrapText(messenger, 0, 42, 18, 128);  // Bắt đầu tại tọa độ x=0, y=46, mỗi dòng cách nhau 18 điểm, tối đa chiều rộng 128 điểm
  u8g2.sendBuffer(); // Gửi nội dung đệm ra màn hình
}

void showProgress(int parameter1, int parameter2, int parameter3) {
  u8g2.clearBuffer();  // Xóa bộ nhớ đệm của màn hình để vẽ mới
  u8g2.setFont(u8g2_font_crox3h_tf);  // Thiết lập font chữ thường (không đậm)

  // Hiển thị thông số Total Stick
  u8g2.drawStr(0, 18, "Total Stick: ");
  u8g2.drawStr(100, 18, String(parameter1).c_str());  // Chuyển parameter1 thành chuỗi và hiển thị

  // Hiển thị thông số Re.Stick
  u8g2.drawStr(0, 36, "Re.Stick: ");
  u8g2.drawStr(100, 36, String(parameter2).c_str());  // Chuyển parameter2 thành chuỗi và hiển thị

  // Hiển thị thông số Count output
  u8g2.drawStr(0, 54, "Count output: ");
  u8g2.drawStr(100, 54, String(parameter3).c_str());  // Chuyển parameter3 thành chuỗi và hiển thị

  u8g2.sendBuffer();  // Gửi dữ liệu từ bộ đệm lên màn hình
}

void showSetup(const char* setUpCode, const char* value, const char* text) {   // Thêm maxValue vào tham số
  u8g2.clearBuffer();  // Xóa bộ nhớ đệm của màn hình để vẽ mới
  u8g2.setFont(u8g2_font_crox3hb_tf);  // Thiết lập font chữ đậm

  char tempSetUpCode[64];    // Tạo một chuỗi tạm chứa mã cài đặt và dấu ";"
  snprintf(tempSetUpCode, sizeof(tempSetUpCode), "%s:", setUpCode);  // Nối mã cài đặt với dấu ":"
  
  u8g2.drawStr(0, 18, tempSetUpCode);  // Hiển thị mã cài đặt (tại vị trí x=0, y=18)
  
  u8g2.setFont(u8g2_font_crox3h_tf);  // Thiết lập font chữ thường (không đậm)
  
  // Chuyển đổi maxValue sang chuỗi
  char maxValueStr[16]; // Chuỗi chứa giá trị maxValue sau khi chuyển đổi
  snprintf(maxValueStr, sizeof(maxValueStr), "%d", maxValue);  // Chuyển maxValue thành chuỗi
  
  // Tính toán độ dài của value và maxValueStr
  int valueLength = strlen(value);
  if (!isNumeric(value)){
    maxLength = 5;
  } else {
    maxLength = strlen(maxValueStr);
    // Giới hạn độ dài value không vượt quá chiều dài maxValue
    if (valueLength > maxLength) {
        valueLength = maxLength;
    }
  }

  // Vị trí bắt đầu cho ký tự đầu tiên căn lề từ phải
  int startX = 128 - 10; // Bắt đầu từ vị trí rìa phải (x = 118)

  // Vẽ từng ký tự từ value lên màn hình, bắt đầu từ ký tự cuối cùng
  for (int i = 0; i < valueLength; i++) {
      char temp[2] = {value[valueLength - 1 - i], '\0'}; // Lấy ký tự theo thứ tự ngược lại
      u8g2.drawStr(startX - (i * 10), 18, temp); // Vẽ ký tự lùi về bên trái
  }

  u8g2.drawLine(0, 23, 128, 23);  // Vẽ một đường ngang trên màn hình (tọa độ từ x=0 đến x=128)

  wrapText(text, 0, 42, 18, 128);  // Bắt đầu tại tọa độ x=0, y=46, mỗi dòng cách nhau 18 điểm, tối đa chiều rộng 128 điểm
  u8g2.sendBuffer(); // Gửi nội dung đệm ra màn hình
}

void showEdit(int columnIndex) {
  u8g2.clearBuffer();  // Xóa bộ nhớ đệm của màn hình để vẽ mới
  u8g2.setFont(u8g2_font_crox3hb_tf);  // Thiết lập font chữ đậm
  char tempSetUpCode[64];    // Tạo một chuỗi tạm chứa mã cài đặt và dấu ";"
  snprintf(tempSetUpCode, sizeof(tempSetUpCode), "%s:", setupCodeStr.c_str());  // Nối mã cài đặt với dấu ":"
  //log(setupCodeStr);
  u8g2.drawStr(0, 18, tempSetUpCode);  // Hiển thị mã cài đặt (tại vị trí x=0, y=18)

  u8g2.setFont(u8g2_font_crox3h_tf);  // Thiết lập font chữ thường (không đậm)

  char maxValueStr[16]; // Chuỗi chứa giá trị maxValue sau khi chuyển đổi
  snprintf(maxValueStr, sizeof(maxValueStr), "%d", maxValue);  // Chuyển maxValue thành chuỗi

  const char* valueChr = valueStr.c_str();
  int valueLength = strlen(valueChr);
  maxLength = strlen(maxValueStr);

  // Giới hạn độ dài value không vượt quá chiều dài maxValue
  if (valueLength > maxLength) {
      valueLength = maxLength;
  }
  log("Value[" +String(columnIndex)+"]");
  int startX = 128 - 10; // 10 là độ rộng trung bình của một ký tự (nếu sử dụng font có kích thước tiêu chuẩn)

    for (int i = 0; i < maxLength; i++) {
      char temp[2] = {valueChr[valueLength - 1 - i], '\0'}; // Lấy ký tự theo thứ tự ngược lại
      if (i == columnIndex){
        u8g2.setDrawColor(1);  // Màu nền
        u8g2.drawBox(startX - (i * 10) - 1, 5, 10, 18);
        u8g2.setDrawColor(0);  // Màu chữ
        u8g2.drawStr(startX - (i * 10), 18, temp); // Vẽ ký tự lùi về bên trái
        u8g2.setDrawColor(1);
      } else {
        u8g2.drawStr(startX - (i * 10), 18, temp); // Vẽ ký tự lùi về bên trái
      }
    }

  u8g2.drawLine(0, 23, 128, 23);  // Vẽ một đường ngang trên màn hình (tọa độ từ x=0 đến x=128)
  wrapText(textStr.c_str(), 0, 42, 18, 128);  // Bắt đầu tại tọa độ x=0, y=46, mỗi dòng cách nhau 18 điểm, tối đa chiều rộng 128 điểm
  u8g2.sendBuffer(); // Gửi nội dung đệm ra màn hình
}

void loadJsonSettings() {
    try {
        const char* code = jsonDoc["main"]["main" + String(menuIndex)]["key"]; // Truy xuất key của mục menu hiện tại từ JSON
        totalChildren = jsonDoc["main"]["main" + String(menuIndex)]["totalChildren"]; // Truy xuất tổng số lượng phần tử con
        setupCodeStr = String(code) + String(pIndex); // Tạo setupCode dựa trên key và pIndex (số thứ tự)

        // Kiểm tra xem configuredValue có phải là số hay không
        if (jsonDoc["main"]["main" + String(menuIndex)]["children"][setupCodeStr]["configuredValue"].is<int>()) {
            // Nếu là số, chuyển đổi sang String
            int valueInt = jsonDoc["main"]["main" + String(menuIndex)]["children"][setupCodeStr]["configuredValue"];
            valueStr = String(valueInt); // Chuyển đổi value từ int thành String
            currentValue = valueStr.toInt();
        } else if (jsonDoc["main"]["main" + String(menuIndex)]["children"][setupCodeStr]["configuredValue"].is<const char*>()) {
            // Nếu là chuỗi, lấy giá trị trực tiếp
            valueStr = jsonDoc["main"]["main" + String(menuIndex)]["children"][setupCodeStr]["configuredValue"].as<const char*>();
            currentValue = -1;
        } else {
            valueStr = ""; // Gán giá trị mặc định nếu không phải số hoặc chuỗi
        }

        maxValue = jsonDoc["main"]["main" + String(menuIndex)]["children"][setupCodeStr]["maxValue"];
        minValue = jsonDoc["main"]["main" + String(menuIndex)]["children"][setupCodeStr]["minValue"];
        explanationMode = jsonDoc["main"]["main" + String(menuIndex)]["children"][setupCodeStr]["explanationMode"];
        editAllowed = jsonDoc["main"]["main" + String(menuIndex)]["children"][setupCodeStr]["editAllowed"];
        if (explanationMode){
          String listStr = jsonDoc["main"]["main" + String(menuIndex)]["children"][setupCodeStr]["explanationDetails"];
          
          splitString(listStr, ListExp, 10);
          textExplanationMode = jsonDoc["main"]["main" + String(menuIndex)]["children"][setupCodeStr]["text"].as<const char*>();
          textStr = textExplanationMode +": " + ListExp[currentValue-1];  // Hiển thị text từ list
        } else {
          textStr = jsonDoc["main"]["main" + String(menuIndex)]["children"][setupCodeStr]["text"].as<const char*>(); // Truy xuất text từ JSON
        }
        //log(textStr);

        keyStr = String(code);
        log("Truy cập thẻ " + keyStr + "/" + setupCodeStr);
        showSetup(setupCodeStr.c_str(), valueStr.c_str() , textStr.c_str()); // Hiển thị thông tin cấu hình bằng cách gọi hàm showSetup
    } catch (const std::exception& e) { // Bắt lỗi nếu có ngoại lệ
        Serial.println("Error reading JSON data: "); // In thông báo lỗi
        Serial.println(e.what()); // In thông tin chi tiết từ e.what()
    } catch (...) { // Bắt mọi lỗi khác không thuộc std::exception
        Serial.println("An unknown error occurred while reading JSON data."); // In thông báo lỗi chung
    }
}


void editValue(const char* Calculations) {
    int newValue;
    int factor = pow(10, columnIndex); // Tính hàng (đơn vị, chục, trăm, v.v.)

    // Tăng hoặc giảm giá trị tại hàng đang chọn
    if (strcmp(Calculations, "addition") == 0) {
        newValue = currentValue + factor;
    } else if (strcmp(Calculations, "subtraction") == 0) {
        newValue = currentValue - factor;
    }

    // Kiểm tra nếu newValue nằm trong khoảng minValue và maxValue
    if (newValue >= minValue && newValue <= maxValue) {
        currentValue = newValue; // Cập nhật currentValue nếu newValue hợp lệ
    }

    // Chuyển giá trị thành chuỗi để hiển thị
    valueStr = String(currentValue);

    // Kiểm tra chức năng diễn giải có hoạt động hay không, nếu hoạt động thì hiển thị diễn giải
    if (explanationMode){
      textStr = textExplanationMode + ": " +ListExp[currentValue-1];
    }

    // Gọi lại hàm showEdit để cập nhật màn hình
    showEdit(columnIndex);
}

void readConfigFile() {
  // Đọc nội dung file config.json
  File config = LittleFS.open(configFile, "r");  // Mở file ở chế độ đọc
  if (!config) {
    Serial.println("Failed to open config file");
    return;
  }

  // Đọc dữ liệu từ file
  DeserializationError error = deserializeJson(jsonDoc, config);
  if (error) {
    Serial.println("Failed to read config file");
    return;
  }
  config.close();
}


void btnMenuClick() {
  //Serial.println("Button Clicked (nhấn nhả)");
  if (displayScreen == "ScreenCD") {
    if (keyStr == "CD") {
      writeFile(jsonDoc,"/config.json");
    }
    showList(menuIndex);  // Hiển thị danh sách menu hiện tại
    displayScreen = "MENU";
  } else if (displayScreen == "ScreenEdit") {
    loadJsonSettings();
    displayScreen = "ScreenCD";
  } else if (displayScreen == "index" && mainStep == 0) {
    trangThaiHoatDong = 0;
    showList(menuIndex);  // Hiển thị danh sách menu hiện tại
    displayScreen = "MENU";
  } else if (displayScreen == "MENU" && mainStep == 0){
    displayScreen= "index";
    trangThaiHoatDong = 1;
    showText("HELLO", "ESP32-OPTION");
  } else if (displayScreen == "testIO"){
    loadJsonSettings();
    displayScreen = "ScreenCD";
    trangThaiHoatDong = 0;
  } else if (displayScreen == "testOutput"){
    loadJsonSettings();
    displayScreen = "ScreenCD";
    trangThaiHoatDong = 0;
  } else if (displayScreen == "screenTestMode" && testModeStep == 0){
    loadJsonSettings();
    displayScreen = "ScreenCD";
    trangThaiHoatDong = 0;
  }
}

// Hàm callback khi bắt đầu nhấn giữ nút
void btnMenuLongPressStart() {
  //Serial.println("Button Long Press Started (BtnMenu)");
}

// Hàm callback khi nút đang được giữ
void btnMenuDuringLongPress() {
  //Serial.println("Button is being Long Pressed (BtnMenu)");
}

void btnSetClick() {
  if (displayScreen == "MENU") {
    pIndex = 1;
    loadJsonSettings(); // Hiển thị giá trị thiết lập
    displayScreen = "ScreenCD"; // Chuyển màn hình sau khi xử lý dữ liệu thành công
  } else if (displayScreen == "ScreenCD" && editAllowed){
    if (keyStr == "CD"){
      columnIndex = maxLength-1;
      showEdit(columnIndex);
      displayScreen = "ScreenEdit";
    } else if (keyStr == "CN") {
      if (setupCodeStr == "CN1"){
        trangThaiHoatDong = 201;   //Trạng thái hoạt động 201 là trạng thái TestMode
        testModeStep = 0;
        chayTestMode = true;
        showText("TEST MODE", String("Step " + String(testModeStep)).c_str());
        displayScreen = "screenTestMode";
      } else if (setupCodeStr == "CN2"){
        trangThaiHoatDong = 202;   //Trạng thái hoạt động 202 là trạng thái TEST IO INPUT
        showText("TEST I/O", "TEST I/O INPUT");
        displayScreen = "testIO";
      } else if ((setupCodeStr == "CN3")){
        trangThaiHoatDong = 203;  //Trạng thái hoạt động 203 là trạng thái TEST IO OUTPUT
        testOutputStep = 0;
        displayScreen = "testOutput";
        hienThiTestOutput = true;
      }
    }
  } else if (displayScreen == "ScreenEdit")  {
    if (keyStr == "CD"){
      if (columnIndex - 1 < 0){
        columnIndex = maxLength-1;
      } else {
        columnIndex --;
      }
      showEdit(columnIndex);
    }
  } else if (displayScreen == "testOutput"){
    daoTinHieuOutput = true;
  }
}

// Hàm callback khi bắt đầu nhấn giữ nút
void btnSetLongPressStart() {
  if (displayScreen = "ScreenEdit"){
    if (keyStr == "CD"){
      jsonDoc["main"]["main" + String(menuIndex)]["children"][setupCodeStr]["configuredValue"] = currentValue;
      log("Đã lưu giá trị:" + String(currentValue) + " vào thẻ " + keyStr + "/" + setupCodeStr);
      loadJsonSettings();
      loadSetup();
      displayScreen = "ScreenCD";
    } else if (keyStr == "CN"){
      if (setupCodeStr == "CN2"){
        /* code */
      } else if (setupCodeStr == "CN4" && currentValue == 1){
        reSet();
        showText("RESET","Tat May Khoi Dong Lai!");
        trangThaiHoatDong = 200;  //Trạng thái hoạt động 200 là reset, không cho phép thao tác nào
        displayScreen = "RESET";
      }
    }
  }
}

// Hàm callback khi nút đang được giữ
void btnSetDuringLongPress() {
  //showSetup("Setup", "OFF", "Dang giu nut");
}

void btnUpClick() {
  if (displayScreen == "MENU") {
    if (menuIndex + 1 > 3) {
      menuIndex = 1;  // Khi chỉ số vượt quá giới hạn, quay lại đầu danh sách
    } else {
      menuIndex++;    // Tăng menuIndex lên 1
    }
    showList(menuIndex);  // Hiển thị danh sách với chỉ số mới
  } else if (displayScreen == "ScreenCD") {
    if (pIndex + 1 > totalChildren) {
      pIndex = 1;
    } else {
      pIndex ++;
    }
    loadJsonSettings(); // Hiển thị giá trị thiết lập
  } else if (displayScreen == "ScreenEdit") {
    if (keyStr = "CD"){
      editValue("addition");
      log("Value:" + valueStr);
    } else if (keyStr == "CN") {
      editValue("addition");
      log("Value:" + valueStr);
    }
  } else if (displayScreen == "testOutput"){
    if (testOutputStep == maxTestOutputStep){
      testOutputStep = 0;
      hienThiTestOutput = true;
    } else {
      testOutputStep ++;
      hienThiTestOutput = true;
    }
  } else if (displayScreen == "screenTestMode"){
    if (testModeStep < maxTestModeStep){
      testModeStep ++;
      chayTestMode = true;
      showText("TEST MODE", String("Step " + String(testModeStep)).c_str());
    }
  }
}

// Hàm callback khi bắt đầu nhấn giữ nút
void btnUpLongPressStart() { 
  //Serial.println("Button Long Press Started (btnUp)");
}

// Hàm callback khi nút đang được giữ
void btnUpDuringLongPress() {
  //Serial.println("Button is being Long Pressed (btnUp)");
}

void btnDownClick() {
  if (displayScreen == "MENU") {
    if (menuIndex - 1 < 1) {
      menuIndex = 3;  // Khi chỉ số nhỏ hơn giới hạn, quay lại cuối danh sách
    } else {
      menuIndex--;    // Giảm menuIndex đi 1
    }
    showList(menuIndex);  // Hiển thị danh sách với chỉ số mới
  } else if (displayScreen == "ScreenCD"){
    if (pIndex - 1 < 1) {
      pIndex = totalChildren;
    } else {
      pIndex --;
    }
    loadJsonSettings(); // Hiển thị giá trị thiết lập
  } else if (displayScreen == "ScreenEdit"){
    if (keyStr == "CD"){
      editValue("subtraction");
      log("Value:" + valueStr);
    } else if (keyStr == "CN"){
      editValue("subtraction");
      log("Value:" + valueStr);
    }
  } else if (displayScreen == "testOutput"){
    if (testOutputStep == 0){
      testOutputStep = maxTestOutputStep;
      hienThiTestOutput = true;
    } else {
      testOutputStep --;
      hienThiTestOutput = true;
    }
  } else if (displayScreen == "screenTestMode"){
    if (testModeStep > 0){
      testModeStep --;
      chayTestMode = true;
      showText("TEST MODE", String("Step " + String(testModeStep)).c_str());
    }
  }
}

// Hàm callback khi bắt đầu nhấn giữ nút
void btnDownLongPressStart() {
  //Serial.println("Button Long Press Started (btnDown)");
}

// Hàm callback khi nút đang được giữ
void btnDownDuringLongPress() {
  //Serial.println("Button is being Long Pressed (btnDown)");
}

//KHAI BÁO CHÂN IO Ở ĐÂY

const int sensorCilinderXp1 = 0;
const int sensorCilinderXp2 = 16;

const int sensorCilinderYp1 = 2;
const int sensorCilinderYp2 = 4;

const int sensorOrigin = 17;
const int sensorFoot = 15;
const int sensorActive = 33;

const int outRelayX = 25;
const int outRelayY = 26;
const int outRelayFoot = 27;
const int outRelayRun = 14;

const int pinDir = 5;
const int pinPWM = 18;



// Thông số cài đặt

unsigned long thoiDiemCuoiPWM = 0;
bool trangThaiPWM = false;

byte cheDoHoatDong = 1;
byte soVongCuon = 2;
bool chieuQuayDongCo = 1;
int tocDoQuay = 1000 ;
int soXungMotor = 100 ;

int soXungDoGoc = 2000;

// Thông số tính toán

int soXungCanChay = 1000;
int soLanDaoCanDaoTinHieu = 2000;
int thoiGianDaoPWM = 50;

// Biến toàn cục
bool diemGoc = false;

bool trangThaiCuoiCungX1 = false;
bool trangThaiCuoiCungX2 = false;
bool trangThaiCuoiCungY1 = false;
bool trangThaiCuoiCungY2 = true;

bool trangThaiCuoiCungBanDap = false;

bool trangThaiKichChanVit = false;
bool trangThaiKichMayChay = false;

unsigned long thoiDiemCuoiKichChanVit = 0;
unsigned long thoiDiemCuoiKichMayChay = 0;

int soXungDaChay = 0;
int soVongDaChay = 0;


//TRƯƠNG TRÌNH NGƯỜI DÙNG LẬP TRÌNH



void testMode(){
  switch (testModeStep){
    case 0:
      if (chayTestMode){
        maxTestModeStep = 7;
        soXungDaChay = 0;
        digitalWrite(outRelayX,HIGH);
        digitalWrite(outRelayY,HIGH);
        chayTestMode = false;
      }
      break;
    case 1:
      if(chayTestMode){
        switch (cheDoHoatDong){
          case 1:
            if (soXungDaChay < soXungCanChay){
              xuatXungPWM(thoiGianDaoPWM);
            } else if (soXungDaChay == soXungCanChay && digitalRead(sensorOrigin)){
              digitalWrite(outRelayY,LOW);
              chayTestMode = false;
            }
            break;
          case 2:
            if (soXungDaChay < soXungCanChay){
              xuatXungPWM(thoiGianDaoPWM);
            } else if (soXungDaChay == soXungCanChay){
              digitalWrite(outRelayY,LOW);
              chayTestMode = false;
            }
            break;
          case 3:
            if (soVongDaChay > soVongCuon){
              xuatXungPWM(thoiGianDaoPWM);
              if (digitalRead(sensorOrigin)){
                soVongDaChay ++;
              }
            } else {
              digitalWrite(outRelayY,LOW);
              chayTestMode = false;
            }
            break;
          default:
            showText("ERROR","Loi trong trinh");
            break;
        }
      }
      break;
    case 2:
      if (chayTestMode){
        digitalWrite(outRelayY,LOW);
        chayTestMode = false;
      }
      break;
    case 3:
      if (chayTestMode){
        digitalWrite(outRelayFoot,HIGH);
        chayTestMode = false;
      }
      break;
    case 4:
      if (chayTestMode){
        digitalWrite(outRelayFoot,LOW);
        digitalWrite(outRelayX,LOW);
        chayTestMode = false;
      }
      break;
    case 5:
      if (chayTestMode){
        digitalWrite(outRelayRun,HIGH);
        digitalWrite(outRelayY,HIGH);
        chayTestMode = false;
      }
      break;
    case 7:
      if (chayTestMode){
        digitalWrite(outRelayRun,LOW);
        digitalWrite(outRelayX,HIGH);
        chayTestMode = false;
      }
      break;
    default:
      /* code */
      break;
    }
}

void testInput(){
  static bool trangthaiCuoiIO1 = false;
  if (digitalRead(sensorCilinderXp1)!= trangthaiCuoiIO1){
    trangthaiCuoiIO1 = digitalRead(sensorCilinderXp1);
    showText("INPUT: 0" , String(trangthaiCuoiIO1).c_str());
  }
  static bool trangthaiCuoiIO2;
  if (digitalRead(sensorCilinderXp2)!= trangthaiCuoiIO2){
    trangthaiCuoiIO2 = digitalRead(sensorCilinderXp2);
    showText("INPUT: 16" , String(trangthaiCuoiIO2).c_str());
  }
  static bool trangthaiCuoiIO3 = false;
  if (digitalRead(sensorCilinderYp1)!= trangthaiCuoiIO3){
    trangthaiCuoiIO3 = digitalRead(sensorCilinderYp1);
    showText("INPUT: 02" , String(trangthaiCuoiIO3).c_str());
  }
  static bool trangthaiCuoiIO4;
  if (digitalRead(sensorCilinderYp2)!= trangthaiCuoiIO4){
    trangthaiCuoiIO4 = digitalRead(sensorCilinderYp2);
    showText("INPUT: 04" , String(trangthaiCuoiIO4).c_str());
  }
  static bool trangthaiCuoiIO5;
  if (digitalRead(sensorOrigin)!= trangthaiCuoiIO5){
    trangthaiCuoiIO5 = digitalRead(sensorOrigin);
    showText("INPUT: 17" , String(trangthaiCuoiIO5).c_str());
  }
  static bool trangthaiCuoiIO6;
  if (digitalRead(sensorFoot)!= trangthaiCuoiIO6){
    trangthaiCuoiIO6 = digitalRead(sensorFoot);
    showText("INPUT: 15" , String(trangthaiCuoiIO6).c_str());
  }
  static bool trangthaiCuoiIO7;
  if (digitalRead(sensorActive)!= trangthaiCuoiIO7){
    trangthaiCuoiIO7 = digitalRead(sensorActive);
    showText("INPUT: 33" , String(trangthaiCuoiIO7).c_str());
  }
}
void testOutput(){
  switch (testOutputStep){
    case 0:
      if (hienThiTestOutput){
        maxTestOutputStep = 5;
        bool tinHieuHienTai = digitalRead(outRelayX);
        showText("1.OUTPUT: 25", String(tinHieuHienTai).c_str());
        hienThiTestOutput = false;
      } else if (daoTinHieuOutput){
        bool tinHieuHienTai = digitalRead(outRelayX);
        digitalWrite(outRelayX,!tinHieuHienTai);
        hienThiTestOutput = true;
        daoTinHieuOutput = false;
      }
      break;
    case 1:
      if (hienThiTestOutput){
        bool tinHieuHienTai = digitalRead(outRelayY);
        showText("2.OUTPUT: 26", String(tinHieuHienTai).c_str());
        hienThiTestOutput = false;
      } else if (daoTinHieuOutput){
        bool tinHieuHienTai = digitalRead(outRelayY);
        digitalWrite(outRelayY,!tinHieuHienTai);
        hienThiTestOutput = true;
        daoTinHieuOutput = false;
      }
      break;
    case 2:
      if (hienThiTestOutput){
        bool tinHieuHienTai = digitalRead(outRelayFoot);
        showText("3.OUTPUT: 27", String(tinHieuHienTai).c_str());
        hienThiTestOutput = false;
      } else if (daoTinHieuOutput){
        bool tinHieuHienTai = digitalRead(outRelayFoot);
        digitalWrite(outRelayFoot,!tinHieuHienTai);
        hienThiTestOutput = true;
        daoTinHieuOutput = false;
      }
      break;
    case 3:
      if (hienThiTestOutput){
        bool tinHieuHienTai = digitalRead(outRelayRun);
        showText("4.OUTPUT: 14", String(tinHieuHienTai).c_str());
        hienThiTestOutput = false;
      } else if (daoTinHieuOutput){
        bool tinHieuHienTai = digitalRead(outRelayRun);
        digitalWrite(outRelayRun,!tinHieuHienTai);
        hienThiTestOutput = true;
        daoTinHieuOutput = false;
      }
      break;
    case 4:
      if (hienThiTestOutput){
        bool tinHieuHienTai = digitalRead(pinDir);
        showText("5.OUTPUT: 5", String(tinHieuHienTai).c_str());
        hienThiTestOutput = false;
      } else if (daoTinHieuOutput){
        bool tinHieuHienTai = digitalRead(pinDir);
        digitalWrite(pinDir,!tinHieuHienTai);
        hienThiTestOutput = true;
        daoTinHieuOutput = false;
      }
      break;
    case 5:
      if (hienThiTestOutput){
        bool tinHieuHienTai = digitalRead(pinPWM);
        showText("6.OUTPUT: 18", String(tinHieuHienTai).c_str());
        hienThiTestOutput = false;
      } else if (daoTinHieuOutput){
        bool tinHieuHienTai = digitalRead(pinPWM);
        digitalWrite(pinPWM,!tinHieuHienTai);
        hienThiTestOutput = true;
        daoTinHieuOutput = false;
      }
      break;
    default:
      break;
  }
}

void veGoc() {
  if (digitalRead(sensorFoot)){
    showText("HELLO","ESP32 VE GOC");
    for (size_t i = 0; i < soXungDoGoc; i++){
      xuatXungPWM(thoiGianDaoPWM);
      if (digitalRead(sensorOrigin)){
        trangThaiHoatDong = 199;
        return;
      }
    }
    trangThaiHoatDong = 200;
    showSetup("ERROR","008","Khong tim thay goc");
  }
}

void tinhToanCaiDat(){
  soXungCanChay = soXungMotor * soVongCuon;
  thoiGianDaoPWM = (1000000*30)/(tocDoQuay*soXungMotor);
  digitalWrite(pinDir,chieuQuayDongCo);
}


void reSet(){
   
}

void loadSetup(){
  cheDoHoatDong = jsonDoc["main"]["main1"]["children"]["CD1"]["configuredValue"];
  soVongCuon = jsonDoc["main"]["main1"]["children"]["CD2"]["configuredValue"];
  chieuQuayDongCo = jsonDoc["main"]["main1"]["children"]["CD3"]["configuredValue"];
  tocDoQuay = jsonDoc["main"]["main1"]["children"]["CD4"]["configuredValue"] ;
  soXungMotor = jsonDoc["main"]["main1"]["children"]["CD5"]["configuredValue"] ;
  tinhToanCaiDat();

}

void khoiDong(){
  displayScreen = "index";
  showText("HELLO","ESP32-OPTION");
  mainStep = 0;
  trangThaiHoatDong = 0;
  loadSetup();
  veGoc();
}

void xuatXungPWM(unsigned long thoiGianDao){
  if (WaitMicros(thoiDiemCuoiPWM,thoiGianDao) && !trangThaiPWM){
    trangThaiPWM = !trangThaiPWM;
    digitalWrite(pinPWM,trangThaiPWM);
    thoiDiemCuoiPWM = micros();
  } else if (WaitMicros(thoiDiemCuoiPWM,thoiGianDao) && trangThaiPWM){
    trangThaiPWM = !trangThaiPWM;
    digitalWrite(pinPWM,trangThaiPWM);
    thoiDiemCuoiPWM = micros();
    soXungDaChay ++;
  }
}

void mainRun(){
  switch (mainStep){
  case 0:
    
    break;
  case 1:
    switch (cheDoHoatDong){
    case 1:
      if (soXungDaChay < soXungCanChay){
        xuatXungPWM(thoiGianDaoPWM);
      } else if (soXungDaChay == soXungCanChay && digitalRead(sensorOrigin)){
        digitalWrite(outRelayY,LOW);
        mainStep ++;
      }
      break;
    case 2:
      if (soXungDaChay < soXungCanChay){
        xuatXungPWM(thoiGianDaoPWM);
      } else if (soXungDaChay == soXungCanChay){
        digitalWrite(outRelayY,LOW);
        mainStep ++;
      }
      break;
    case 3:
      if (soVongDaChay > soVongCuon){
        xuatXungPWM(thoiGianDaoPWM);
        if (digitalRead(sensorOrigin)){
          soVongDaChay ++;
        }
      } else {
        digitalWrite(outRelayY,LOW);
        mainStep ++;
      }
      break;
    default:
      showText("ERROR","Loi trong trinh");
      break;
    }
    break;
  case 2:
    if (!digitalRead(sensorCilinderYp2)){
      digitalWrite(outRelayFoot,HIGH);
      trangThaiKichChanVit = true;
      mainStep++;
    }
    break;
  case 3:
    if (digitalRead(sensorFoot)){
      digitalWrite(outRelayFoot,LOW);
      trangThaiKichChanVit = false;
      digitalWrite(outRelayX,LOW);
      mainStep ++;
    }
    break;
  case 4:
    if (digitalRead(sensorCilinderXp2)) {
      digitalWrite(outRelayRun,HIGH);
      digitalWrite(outRelayY,HIGH);
      trangThaiKichMayChay = true;
      mainStep ++;
    } 
    break;
  case 5:
    if (!digitalRead(sensorCilinderYp1)){
      digitalWrite(outRelayRun,LOW);
      digitalWrite(outRelayX,HIGH);
      mainStep++;
    }
    break;
  case 6:
    if (digitalRead(!sensorCilinderXp1)){
      soXungDaChay = 0;
      soVongDaChay = 0;
      trangThaiHoatDong = 1;
      mainStep = 0;
    }
    break;
  default:
    break;
  }
}

void setup() {

  Serial.begin(115200);     // Khởi tạo Serial và màn hình

  u8g2.begin();  // Khởi tạo màn hình OLED

  btnMenu.attachClick(btnMenuClick);
  btnMenu.attachLongPressStart(btnMenuLongPressStart);
  btnMenu.attachDuringLongPress(btnMenuDuringLongPress);

  btnSet.attachClick(btnSetClick);
  btnSet.attachLongPressStart(btnSetLongPressStart);
  btnSet.attachDuringLongPress(btnSetDuringLongPress);

  btnUp.attachClick(btnUpClick);
  btnUp.attachLongPressStart(btnUpLongPressStart);
  btnUp.attachDuringLongPress(btnUpDuringLongPress);

  btnDown.attachClick(btnDownClick);  
  btnDown.attachLongPressStart(btnDownLongPressStart);
  btnDown.attachDuringLongPress(btnDownDuringLongPress);

  btnMenu.setDebounceMs(btnSetDebounceMill);
  btnSet.setDebounceMs(btnSetDebounceMill);
  btnUp.setDebounceMs(btnSetDebounceMill);
  btnDown.setDebounceMs(btnSetDebounceMill);

  btnMenu.setPressMs(btnSetPressMill);
  btnSet.setPressMs(btnSetPressMill);
  btnUp.setPressMs(btnSetPressMill);
  btnDown.setPressMs(btnSetPressMill);

  pinMode(sensorCilinderXp1,INPUT);
  pinMode(sensorCilinderXp2,INPUT);
  pinMode(sensorCilinderYp1,INPUT);
  pinMode(sensorCilinderYp2,INPUT);
  pinMode(sensorActive,INPUT);
  pinMode(sensorFoot,INPUT);
  pinMode(sensorOrigin,INPUT);

  pinMode(pinDir,OUTPUT);
  pinMode(pinPWM,OUTPUT);
  pinMode(outRelayX,OUTPUT);
  pinMode(outRelayY,OUTPUT);
  pinMode(outRelayFoot,OUTPUT);
  pinMode(outRelayRun,OUTPUT);


  if (!LittleFS.begin()) {
    showSetup("Error", "E003", "LittleFS Mount Failed");
    Serial.println("LittleFS Mount Failed");
    return;
  }

  

  // Kiểm tra xem file config.json có tồn tại không
  if (!LittleFS.exists(configFile)) {
    DeserializationError error = deserializeJson(jsonDoc, jsonString); // Phân tích chuỗi JSON
    if (error) {
        showSetup("Error", "E005", "JsonString Error");
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }
    showSetup("Error", "E007", "JsonString Load Mode");
    Serial.println("Read Data From JsonString");
    loadSetup();
    Serial.println("File config.json does not exist.");
    return;
  }

  readConfigFile();

  Serial.println("Load toàn bộ dữ liệu thành công");
  khoiDong();
}

void loop() {
  switch (trangThaiHoatDong){
  case 0:
    btnMenu.tick();
    btnSet.tick();
    btnUp.tick();
    btnDown.tick();
    break;
  case 1:{
    btnMenu.tick();
    // Đảm bảo rằng sensorActive đã được định nghĩa đúng
    bool trangThaiBanDap = digitalRead(sensorActive);
    if (trangThaiCuoiCungBanDap != trangThaiBanDap) {
      if (trangThaiBanDap && mainStep == 0) {
        mainStep ++;
        trangThaiHoatDong ++;
      }
      trangThaiCuoiCungBanDap = trangThaiBanDap;
    }
    break;
  }
  case 2:
    mainRun();
    break;
  case 199:
    if (!digitalRead(sensorFoot)){
      digitalWrite(outRelayRun,LOW);
      showText("HELLO","ESP32-READY");
      diemGoc = true;
      trangThaiHoatDong = 1;
    }
    break;
  case 200:
    break;
  case 201:
    btnMenu.tick();
    btnUp.tick();
    btnDown.tick();
    testMode();
    break;
  case 202:
    btnMenu.tick();
    testInput();
    break;
  case 203:
    btnMenu.tick();
    btnSet.tick();
    btnUp.tick();
    btnDown.tick();
    testOutput();
    break;
  default:
    // Xử lý giá trị không mong muốn của trangThaiHoatDong
      trangThaiHoatDong = 0;
    break;
  }
}
