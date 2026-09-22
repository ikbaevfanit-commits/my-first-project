#include <WiFi.h>
#include <time.h>
#include <Adafruit_NeoPixel.h>

// Аппаратная конфигурация
#define LED_PIN         10   // Твой контакт управления лентой часов
#define INTERNAL_LED     8   // Встроенный синий светодиод на плате ESP32-C3 Super Mini
#define NUM_LEDS        60   // Лента из 60 светодиодов

// Геометрический сдвиг: 00:00:00 теперь строго на 28-м светодиоде ленты
#define LED_OFFSET      27

// Настройки беспроводной сети
const char* ssid     = "Ufanet_6/1";     
const char* password = "9177549462"; 

// Настройки часового пояса (Уфа: UTC +5 часов, без перехода на летнее время)
const long  gmtOffset_sec = 5 * 3600; 
const int   daylightOffset_sec = 0;   

// Инициализация объекта адресной ленты
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);

  // Принудительно выключаем встроенный светодиод платы
  pinMode(INTERNAL_LED, OUTPUT);
  digitalWrite(INTERNAL_LED, HIGH); 

  // Стабилизация пина управления лентой
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  delay(10);
  
  // Старт работы с адресными светодиодами
  strip.begin();
  strip.setBrightness(255); // Начальное значение (100%)
  strip.show(); 

  // Подключение к Wi-Fi роутеру в режиме клиента
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");

  // Инициализация синхронизации времени по протоколу NTP
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  Serial.println("NTP server synchronized.");
}

void loop() {
  struct tm timeinfo;
  // Если время еще не подтянулось из сети, ждем стабильного подключения
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Waiting for time server response...");
    delay(1000);
    return;
  }

  // Получаем текущие данные времени
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  int currentSecond = timeinfo.tm_sec;

  // Флаг ночного режима: с 21:00 вечера (включая этот час) до 07:00 утра (не включая этот час)
  // То есть: 21, 22, 23, 0, 1, 2, 3, 4, 5, 6 часов — это ночь.
  bool isNight = (currentHour >= 21 || currentHour < 7);

  // Динамически управляем яркостью всей ленты
  if (isNight) {
    strip.setBrightness(25);  // ~10% от максимальной яркости 255
  } else {
    strip.setBrightness(255); // 100% яркости
  }

  // Рассчитываем базовые виртуальные позиции стрелок (0-59)
  int raw_s = currentSecond;
  int raw_m = currentMinute;
  int raw_h = ((currentHour % 12) * 5) + (currentMinute / 12); // Плавное смещение часовой стрелки

  // Применяем круговой сдвиг под физическое положение начала круга корпуса (28-й светодиод)
  int s_pixel = (raw_s + LED_OFFSET) % NUM_LEDS;
  int m_pixel = (raw_m + LED_OFFSET) % NUM_LEDS;
  int h_pixel = (raw_h + LED_OFFSET) % NUM_LEDS;

  // Полностью очищаем предыдущий шаг отображения
  strip.clear();

  // Логика побитового наложения и смешивания цветов при совпадении стрелок
  // Шаг 1: Добавляем Красный цвет для часа (работает всегда)
  uint32_t color = strip.getPixelColor(h_pixel);
  strip.setPixelColor(h_pixel, color | strip.Color(255, 0, 0));

  // Шаг 2: Добавляем Зеленый цвет для минут (работает всегда)
  color = strip.getPixelColor(m_pixel);
  strip.setPixelColor(m_pixel, color | strip.Color(0, 255, 0));

  // Шаг 3: Добавляем Синий цвет для секунд (ТОЛЬКО ДНЕМ)
  if (!isNight) {
    color = strip.getPixelColor(s_pixel);
    strip.setPixelColor(s_pixel, color | strip.Color(0, 0, 255));
  }

  // Передаем обновленный массив цветов на светодиоды
  strip.show();

  // Отправка текущих логов в Монитор порта
  Serial.printf("Time: %02d:%02d:%02d | Mode: %s | Brightness: %d%%\n", 
                currentHour, currentMinute, currentSecond, 
                isNight ? "NIGHT" : "DAY", 
                isNight ? 10 : 100);

  // Тактовая частота обновления основного цикла программы (150 мс)
  delay(150); 
}
