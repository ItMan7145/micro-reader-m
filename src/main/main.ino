/*
    Скетч к проекту "Электронная шпаргалка с Wi-Fi" ESP8266

    MODIFIED BY ITMAN7145
    https://github.com/ItMan7145/micro-reader

    Ссылка на ядро для IDE: https://arduino.esp8266.com/stable/package_esp8266com_index.json
    Рекомендуемая версия ядра 2.7.4
    Рекомендуемые настройки:
      ESP8266 Boards (2.7.4)
        Generic ESP8266 Module
          Upload speed: 921600
          Flash size: 4M (FS:2MB OTA:~1019KB)
          Остальные параметры по умолчанию
*/

// TODO:
// 1. настройка шрифта
// 2. настройка режима аккумулятора

/* ================ Настройки ================ */

#define AP_DEFAULT_SSID "MicroReader(M)" // Стандартное имя точки доступа ESP (До 20-ти символов)
#define AP_DEFAULT_PASS "12345687"       // Стандартный пароль точки доступа ESP (До 20-ти символов)
#define STA_DEFAULT_SSID ""              // Стандартное имя точки доступа роутера (До 20-ти символов)
#define STA_DEFAULT_PASS ""              // Стандартный пароль точки доступа роутера (До 20-ти символов)
#define STA_CONNECT_EN 0                 // 1/0 - вкл./выкл. подключение к роутеру
#define WIFI_TIMEOUT_S 300               // Таймаут на отключение Wi-Fi (секунды)

#define IIC_SDA_PIN D1    // Номер GPIO SDA дисплея
#define IIC_SCL_PIN D2    // Номер GPIO SCL дисплея
#define OLED_CONTRAST 100 // Яркость дисплея по умолчанию (%)
#define FLIP 0            // перевернуть экран
#define DEFAULT_SCALE 1   // дефолтный размер шрифта

#define UP_BTN_PIN D6  // Номер GPIO для кнопки ВВЕРХ
#define OK_BTN_PIN D5  // Номер GPIO для кнопки ОК
#define DWN_BTN_PIN D7 // Номер GPIO для кнопки ВНИЗ
#define _EB_DEB 25     // Дебаунс кнопок (мс)

#define BATTERY 0
#define VBAT_FULL_MV 3600  // Напряжение питания при заряженном аккуме в (мВ)
#define VBAT_EMPTY_MV 2600 // Напряжение питания при севшем аккуме в (мВ)

#define EE_KEY 0x10 // Ключ EEPROM (1 байт) - измени, чтобы сбросить настройки

/* =========================================== */
/* ============ Список библиотек ============= */

#include <Wire.h>        // Либа I2C
#include <EEPROM.h>      // Либа EEPROM
#include <LittleFS.h>    // Либа файловой системы
#include <GyverPortal.h> // Либа веб морды
#include <GyverOLED.h>   // Либа олед-дисплея
#include <EncButton2.h>  // Либа кнопок

/* =========================================== */
/* ============ Список объектов ============== */

GyverPortal ui(&LittleFS);                          // Портал
GyverOLED<SSD1306_128x64> oled;                     // Олед
EncButton2<EB_BTN> up(INPUT_PULLUP, UP_BTN_PIN);    // Кнопка вверх
EncButton2<EB_BTN> ok(INPUT_PULLUP, OK_BTN_PIN);    // Кнопка ОК
EncButton2<EB_BTN> down(INPUT_PULLUP, DWN_BTN_PIN); // Кнопка вниз

/* =========================================== */
/* ========= Глобальные переменные =========== */

struct
{                                      // Структура со всеми настройками
  char apSsid[21] = AP_DEFAULT_SSID;   // Имя сети для AP режима по умолчанию
  char apPass[21] = AP_DEFAULT_PASS;   // Пароль сети для AP режима по умолчанию
  char staSsid[21] = STA_DEFAULT_SSID; // Имя сети для STA режима по умолчанию
  char staPass[21] = STA_DEFAULT_PASS; // Пароль сети для STA режима по умолчанию
  bool staModeEn = STA_CONNECT_EN;     // Подключаться роутеру по умолчанию?
  bool flip = FLIP;                    // перевернуть экран?
  bool useBattery = BATTERY;           // использовать аккумулятор?
  int dispContrast = OLED_CONTRAST;    // Яркость оледа
  byte scale = DEFAULT_SCALE;
} sets;

byte cursor = 0;       // Указатель (курсор) меню
byte files = 0;        // Количество файлов
uint32_t uiTimer = 0;  // Таймер таймаута дисплея
uint32_t batTimer = 0; // Таймер опроса АКБ

/* =========================================== */

/* ======================================================================= */
/* ================= Билд веб-страницы + подсос значений ================= */

void build()
{                                        // Билд страницы
  GP.BUILD_BEGIN(400);                   // Ширина колонок
  GP.THEME(GP_DARK);                     // Темная тема
  GP.PAGE_TITLE("Wi-Fi Reader");         // Обзываем титл
  GP.FORM_BEGIN("/cfg");                 // Начало формы
  GP.GRID_RESPONSIVE(600);               // Отключение респонза при узком экране
  M_BLOCK(                               // Общий блок-колонка для WiFi
      GP.SUBMIT_MINI("SUBMIT SETTINGS"); // Кнопка отправки формы

      M_BLOCK_TAB(   // Конфиг для AP режима -> текстбоксы (логин + пароль)
          "AP-Mode", // Имя + тип DIV
          GP.TEXT("apSsid", "Login", sets.apSsid, "", 20);
          GP.BREAK();
          GP.TEXT("apPass", "Password", sets.apPass, "", 20); GP.BREAK(););

      M_BLOCK_TAB(    // Конфиг для STA режима -> текстбоксы (логин + пароль)
          "STA-Mode", // Имя + тип DIV
          GP.TEXT("staSsid", "Login", sets.staSsid, "", 20);
          GP.BREAK();
          GP.TEXT("staPass", "Password", sets.staPass, "", 20); GP.BREAK();
          M_BOX(GP_CENTER, GP.LABEL("STA Enable"); GP.SWITCH("staEn", sets.staModeEn);););

      M_BLOCK_TAB(
          "Flip",
          GP.LABEL("Перевернуть экран");
          GP.BREAK();
          GP.BREAK();
          GP.SWITCH("flipSwitch", sets.flip););

      M_BLOCK_TAB(
          "Text scale",
          GP.LABEL("Размер текста");
          GP.BREAK();
          GP.BREAK();
          GP.SLIDER("scaleSlider", sets.scale, 1, 4, 1);)

          GP.FORM_END(); // <- Конец формы (костыль)

      M_BLOCK_TAB(           // Блок с OTA-апдейтом
          "ESP UPDATE",      // Имя + тип DIV
          GP.OTA_FIRMWARE(); // Кнопка с OTA начинкой
      );

      M_BLOCK_TAB(                    // Блок с файловым менеджером
          "FILE MANAGER",             // Имя + тип DIV
          GP.FILE_UPLOAD("file_upl"); // Кнопка для загрузки файла
          GP.FILE_MANAGER(&LittleFS); // Файловый менеджер
      ););

  GP.BUILD_END(); // Конец билда страницы
}

void action(GyverPortal &p)
{ // Подсос значений со страницы
  if (p.form("/cfg"))
  { // Если есть сабмит формы - копируем все в переменные
    p.copyStr("apSsid", sets.apSsid);
    p.copyStr("apPass", sets.apPass);
    p.copyStr("staSsid", sets.staSsid);
    p.copyStr("staPass", sets.staPass);
    p.copyBool("staEn", sets.staModeEn);
    p.copyBool("flipSwitch", sets.flip);
    p.copyInt("scaleSlider", sets.scale);

    byte con = map(sets.dispContrast, 10, 100, 1, 255);
    oled.setContrast(con); // Тут же задаем яркость оледа
    EEPROM.put(1, sets);   // Сохраняем все настройки в EEPROM
    EEPROM.commit();       // Записываем
  }
}

/* ======================================================================= */
/* ========================== Работа с файлами =========================== */

String getFilenameByIndex(int idx)
{                                   // Вывод имени файла по номеру
  Dir root = LittleFS.openDir("/"); // Открываем директорию (корень)
  for (int i = 0; root.next(); i++)
  { // Шагаем по директории
    if (i != idx)
      continue;                     // Если не тот файл по номеру - в начало цикла
    return ("/" + root.fileName()); // Нужный файл - возвращаем имя + путь
  }
}

int getFilesCount(void)
{                                   // Вывод количества файлов в системе
  int count = 0;                    // Счетчик
  Dir root = LittleFS.openDir("/"); // Открываем директорию (корень)
  while (root.next())
  {                                 // Шагаем по директории
    File file = root.openFile("r"); // Открываем файл для чтения
    if (file)
      count++;    // Если с файлом все ОК - инкремент
    file.close(); // Закрываем файл
  }
  return count; // Возвращаем количество
}

/* ======================================================================= */
/* ============================ Главное меню ============================= */

void drawMainMenu(void)
{                            // Отрисовка главного меню
  oled.clear();              // Очистка
  oled.home();               // Возврат на 0,0
  oled.line(0, 10, 127, 10); // Линия
  oled.print("FILES FOUND: ");
  oled.print(files); // Выводим кол-во файлов

  for (uint8_t i = 0; i < 6 && i < files; i++)
  {                                                                    // Проходимся от 2й до 8й строки оледа
    oled.setCursor(0, i + 2);                                          // Ставим курсор на нужную строку
    oled.print(getFilenameByIndex(cursor < 6 ? i : i + (cursor - 5))); // Выводим имя файла
  }

  oled.setCursor(0, constrain(cursor, 0, 5) + 2);
  oled.print(" "); // Чистим место под указатель
  oled.setCursor(0, constrain(cursor, 0, 5) + 2);
  oled.print(">"); // Выводим указатель на нужной строке
  oled.update();   // Выводим картинку
}

/* ======================================================================= */
/* ============================= Меню Wi-FI ============================== */

void drawStaMenu(void)
{                            // Рисуем STA меню
  oled.clear();              // Очистка
  oled.home();               // Возврат на 0,0
  oled.line(0, 10, 127, 10); // Линия
  oled.print("STA MODE");    // Выводим режим
  oled.setCursor(0, 2);
  oled.print("Connected: ");
  oled.print(sets.staSsid); // Выводим имя сети
  oled.setCursor(0, 4);
  oled.print("LocalIP:");
  oled.print(WiFi.localIP()); // Выводим IP
  oled.update();              // Выводим картинку
}

void drawApMenu(void)
{                            // Рисуем AP меню
  oled.clear();              // Очистка
  oled.home();               // Возврат на 0,0
  oled.line(0, 10, 127, 10); // Линия
  oled.print("AP MODE");     // Выводим режим
  oled.setCursor(0, 2);
  oled.print("SSID: ");
  oled.print(sets.apSsid); // Выводим имя сети
  oled.setCursor(0, 4);
  oled.print("PASS: ");
  oled.print(sets.apPass); // Выводим пароль
  oled.setCursor(0, 6);
  oled.print("LocalIP:");
  oled.print("192.168.4.1"); // Выводим IP
  oled.update();             // Выводим картинку
}

void enterToWifiMenu(void)
{                            // Переход в режим WIFI
  oled.clear();              // Очистка
  oled.home();               // Возврат на 0,0
  oled.line(0, 10, 127, 10); // Линия
  oled.print("WI-FI MENU");  // Выводим режим
  oled.update();             // Выводим картинку

  if (sets.staModeEn)
  {                            // Если нужно подключиться к роутеру
    oled.clear();              // Очистка
    oled.home();               // Возврат на 0,0
    oled.line(0, 10, 127, 10); // Линия
    oled.print("WI-FI MENU");  // Выводим режим
    oled.setCursor(0, 2);
    oled.print("Connecting"); // Выводим надпись
    oled.update();            // Выводим картинку

    WiFi.mode(WIFI_STA);                    // Включаем wifi
    WiFi.begin(sets.staSsid, sets.staPass); // Подключаемся к сети
    oled.setCursor(60, 2);                  // Ставим курсор

    for (byte i = 0; i < 10; i++)
    { // Цикл на 10 секунд
      if (WiFi.status() != WL_CONNECTED)
      {                  // Если к сети не подключились
        oled.print("."); // Рисуем точку
        oled.update();   // Выводим картинку
        delay(1000);     // Ждем секунду
      }
      else
      {                        // Как только подключились
        ui.attachBuild(build); // Подключаем билд веб морды
        ui.attach(action);     // Подключаем обработчик действий
        ui.start();            // Стартуем!
        ui.enableOTA();        // Включаем ОТА для прошивки по воздуху
        while (1)
        {              // Бесконечный цикл
          ok.tick();   // Опрос кнопки ОК
          up.tick();   // Опрос кнопки UP
          down.tick(); // Опрос кнопки DOWN

          if (up.isClick() || up.isHold())
          {                     // Поднять яркость
            uiTimer = millis(); // Сброс таймера дисплея
            sets.dispContrast = constrain(sets.dispContrast + 10, 10, 100);
            byte con = map(sets.dispContrast, 10, 100, 1, 255);
            oled.setContrast(con); // Тут же задаем яркость оледа
          }

          if (down.isClick() || down.isHold())
          {                     // Понизить яркость
            uiTimer = millis(); // Сброс таймера дисплея
            sets.dispContrast = constrain(sets.dispContrast - 10, 10, 100);
            byte con = map(sets.dispContrast, 10, 100, 1, 255);
            oled.setContrast(con); // Тут же задаем яркость оледа
          }

          if (ok.isClick() || ((millis() - uiTimer) >= WIFI_TIMEOUT_S * 1000))
          {                          // Если нажата кнопка или таймаут WiFi
            uiTimer = millis();      // Сброс таймера дисплея
            files = getFilesCount(); // Получаем количество файлов
            drawMainMenu();          // Рисуем главное меню
            EEPROM.put(1, sets);     // Сохраняем все настройки в EEPROM
            EEPROM.commit();         // Записываем
            WiFi.mode(WIFI_OFF);     // Вырубаем wifi
            return;                  // Валим из функции
          }

          if (millis() - batTimer >= 5000)
          {                      // Таймер дисплея
            batTimer = millis(); // Сбрасываем таймер
            drawStaMenu();       // Рисуем меню (только ради индикации заряда)
          }

          ui.tick(); // Тикер портала
          yield();   // Внутренний поллинг ESP
        }
      }
    }
  }

  WiFi.mode(WIFI_AP);                    // Если STA режим пропущен врубаем AP
  WiFi.softAP(sets.apSsid, sets.apPass); // Создаем сеть
  ui.attachBuild(build);                 // Подключаем билд веб морды
  ui.attach(action);                     // Подключаем обработчик действий
  ui.start();                            // Стартуем!
  ui.enableOTA();                        // Включаем ОТА для прошивки по воздуху

  while (1)
  {              // Бесконечный цикл
    ok.tick();   // Опрос кнопки ОК
    up.tick();   // Опрос кнопки UP
    down.tick(); // Опрос кнопки DOWN

    if (up.isClick() || up.isHold())
    {                     // Поднять яркость
      uiTimer = millis(); // Сброс таймера дисплея
      sets.dispContrast = constrain(sets.dispContrast + 10, 10, 100);
      byte con = map(sets.dispContrast, 10, 100, 1, 255);
      oled.setContrast(con); // Тут же задаем яркость оледа
    }

    if (down.isClick() || down.isHold())
    {                     // Понизить яркость
      uiTimer = millis(); // Сброс таймера дисплея
      sets.dispContrast = constrain(sets.dispContrast - 10, 10, 100);
      byte con = map(sets.dispContrast, 10, 100, 1, 255);
      oled.setContrast(con); // Тут же задаем яркость оледа
    }

    if (ok.isClick() || ((millis() - uiTimer) / 1000 >= WIFI_TIMEOUT_S))
    {                          // Если нажата кнопка или таймаут WiFi
      uiTimer = millis();      // Сброс таймера дисплея
      files = getFilesCount(); // Получаем количество файлов
      drawMainMenu();          // Рисуем главное меню
      EEPROM.put(1, sets);     // Сохраняем все настройки в EEPROM
      EEPROM.commit();         // Записываем
      WiFi.softAPdisconnect(); // Отключаем точку доступа
      WiFi.mode(WIFI_OFF);     // Вырубаем wifi
      return;                  // Валим из функции
    }

    if (millis() - batTimer >= 5000)
    {                      // Таймер дисплея
      batTimer = millis(); // Сбрасываем таймер
      drawApMenu();        // Рисуем меню (только ради индикации заряда)
    }

    ui.tick(); // Тикер портала
    yield();   // Внутренний поллинг ESP
  }
}

/* ======================================================================= */
/* ============================ Чтение файла ============================= */

void drawPage(File file)
{ // Отрисовка страницы на олед
  if (!file.available())
    return; // Если файл кончился - не выводим
  oled.clear();
  oled.home(); // Очистка и установка в начало дисплея
  while (!oled.isEnd() && file.available())
  {                          // Выводим символы пока не кончился дисплей или файл
    oled.write(file.read()); // Транслируем символы в дисплей
  }
  oled.update(); // Выводим картинку
}

void enterToReadFile(void)
{                                                             // Режим чтения файла
  File file = LittleFS.open(getFilenameByIndex(cursor), "r"); // Чтение имени файла по положению курсора и открытие файла
  if (!file)
  { // Если сам файл не порядке
    files = getFilesCount();
    drawMainMenu(); // Смотрим количество файлов и рисуем главное меню
    file.close();
    return; // Закрываем файл и выходим
  }

  drawPage(file); // Если с файлом все ок - рисуем первую страницу

  while (1)
  {            // Бесконечный цикл
    up.tick(); // Опрос кнопок
    ok.tick();
    down.tick();

    if (ok.isClick())
    {                     // Если ок нажат
      uiTimer = millis(); // Сбрасываем таймер дисплея
      files = getFilesCount();
      drawMainMenu(); // Смотрим количество файлов и рисуем главное меню
      file.close();
      return; // Закрываем файл и выходим
    }
    else if (up.isClick() or up.isHold())
    {                                   // Если нажата или удержана вверх
      uiTimer = millis();               // Сбрасываем таймер дисплея
      long pos = file.position() - 500; // Смещаем положение файла вверх
      if (pos < 0)
        pos = 0;      // Если достигли нуля - не идем дальше
      file.seek(pos); // Устанавливаем указатель файла
      drawPage(file); // Рисуем страницу
    }
    else if (down.isClick() or down.isHold())
    {                     // Если нажата или удержана вниз
      uiTimer = millis(); // Сбрасываем таймер дисплея
      drawPage(file);     // Рисуем страницу
    }

    yield(); // Внутренний поллинг ESP
  }
}

/* ======================================================================= */
/* ============================ setup + loop ============================= */

void setup()
{
  pinMode(UP_BTN_PIN, INPUT_PULLUP);
  pinMode(OK_BTN_PIN, INPUT_PULLUP);
  pinMode(DWN_BTN_PIN, INPUT_PULLUP); // Все пины кнопок в режиме входа с подтяжкой
  ok.setHoldTimeout(1500);            // Длинное удержание кнопки ОК - 1.5 секунды

  Wire.begin(IIC_SDA_PIN, IIC_SCL_PIN); // меняем i2c пины местами для подключения дисплея

  LittleFS.begin();  // Инициализация файловой системы
  EEPROM.begin(100); // Инициализация EEPROM
  if (EEPROM.read(0) != EE_KEY)
  {                          // Если ключ еепром не совпадает
    EEPROM.write(0, EE_KEY); // Пишем ключ
    EEPROM.put(1, sets);     // Пишем дефолтные настройки
    EEPROM.commit();         // Запись
  }
  else
  {                      // Если ключ совпадает
    EEPROM.get(1, sets); // Читаем настройки
  }

  oled.init(IIC_SDA_PIN, IIC_SCL_PIN); // Инициализация оледа
  oled.clear();                        // Очистка оледа
  oled.update();                       // Вывод пустой картинки
  oled.autoPrintln(true);              // Включаем автоперенос строки
  oled.setScale(sets.scale);           // изменение размера текста

  if (sets.flip)
  {
    oled.flipH(1); // переворот дисплея
    oled.flipV(1);
    up.setPins(INPUT_PULLUP, DWN_BTN_PIN);
    down.setPins(INPUT_PULLUP, UP_BTN_PIN);
  }

  byte con = map(sets.dispContrast, 10, 100, 1, 255);
  oled.setContrast(con);   // Тут же задаем яркость оледа
  files = getFilesCount(); // Читаем количество файлов
  uiTimer = millis();      // Сбрасываем таймер дисплея
  drawMainMenu();          // Рисуем главное меню
  // enterToWifiMenu();
}

void loop()
{
  up.tick();
  ok.tick();
  down.tick(); // Опрос кнопок

  if (up.isClick() or up.isHold())
  {                                               // Если нажата или удержана кнопка вверх
    uiTimer = millis();                           // Сбрасываем таймер дисплея
    cursor = constrain(cursor - 1, 0, files - 1); // Двигаем курсор
    drawMainMenu();                               // Обновляем главное меню
  }
  else if (down.isClick() or up.isHold())
  {                                               // Если нажата или удержана кнопка вниз
    uiTimer = millis();                           // Сбрасываем таймер дисплея
    cursor = constrain(cursor + 1, 0, files - 1); // Двигаем курсор
    drawMainMenu();                               // Обновляем главное меню
  }
  else if (ok.isHold())
  {                     // Если удержана ОК
    uiTimer = millis(); // Сбрасываем таймер дисплея
    enterToWifiMenu();  // Переходим в меню wifi
  }
  else if (ok.isClick())
  {                     // Если нажата ОК
    uiTimer = millis(); // Сбрасываем таймер дисплея
    enterToReadFile();  // Переходим к чтению файла
  }
}
