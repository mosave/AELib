# AE' ESP8266 IoT Framework

## Набор базовых модулей для упрощения и ускорения разработки IoT устройств на основе процессоров ESP82XX

Фреймворк "из коробки" реализует базовые функции WiFi IoT устройства:
 * Подключение к WiFi сети и MQTT брокеру с постоянным мониторингом соединения
 * Интеграция устройства в MQTT инфраструктуру: публикация информации об состоянии и heartbeat устройства, 
 обработка основных команд настройки и управления через MQTT
 * Поддержка OTA (On The Air update, прошивка устройства по WiFi)
 * Модуль обработки кнопок: защита от дребезга, 
 различные типы событий (нажатие/отпускание, короткие/длинные нажатия), поддержка виртуальных 
 кнопок и их публикация в MQTT
 * Модуль управления реле с публикацией состояния и управлением через MQTT
 * Менеджер EEPROM с отложенной записью изменений
 * Поддержка модульной архитектуры прошивки устройства
 * Поддержка различных сенсоров и устройств (DHT22, BMP280, BH1750...)

## Примеры использования фреймворка

### Простая базовая прошивка 3х-канального смарт реле

В репозитории можно найти пример проекта для Arduino Studio (файл AELib.ino), 
реализующий прошивку типичного смарт реле. Такое реле, например, можно использовать для управления тремя 
группами освещения в комнате с помощью двухклавишного выключателя.

В прошивке предполагается что ESP управляет тремя реле, подключенными к выходам D5, D6 и D7 (GPIO14, 12 и 13 соответственно) 
и имеет 2 нормально разомкнутые кнопки без фиксации, подключенные к D1 и D2 (GPIO 5 и 4) и GND.

Нажатия кнопок управляют первым и вторым реле, одновременное нажатие двух кнопок - третьим реле.

Кроме того, устройство будет управлять встроенным светодиодом в зависимости от состояния подключения и нажатий кнопок.

### Другие примеры использования
 * [MQTT интерфейс электросчетчика Eastron SDM630](https://github.com/mosave/SDM630Gateway)
 * [Замена модуля Broadlink в термостатах Beok на MQTT контроллер на базе ESP8266](https://github.com/mosave/Beok2MQTT)

## Модули и файлы фреймовка

### Config.h: Конфигурация прошивки

В файле описываются необходимые для используемых модулей константы и сигнатуры общих функций.
При создании нового проекта используйти файл Config.h из этого репозитория в качестве шаблона.
Основные константы, которые должны быть определены в файле конфигурации:
 * **WIFI_SSID**: имя WiFi сети, к которой будет подключаться устройство
 * **WIFI_Password**: пароль для подключения к WiFi 
 * **MQTT_Address**: адрес MQTT брокера
 * **MQTT_Port**: номер порта MQTT брокера
 * **WIFI_HostName**: имя хоста и имя MQTT клиента. "%s" в будет заменено на MAC адрес устройства. Если константа не определена - устройство будет иметь имя "ESP_%s" и в дальнейшем может быть переименовано MQTT командой
 * **MQTT_Root**: корневой топик устройства. "%s" будет заменено на имя хоста. Если константа не определена, то топик по умолчанию будет "new/%s/" и в дальнейшем может быть изменено с помощью MQTT команды

Если константа **MQTT_Address** либо **MQTT_Port** не определена то фреймворк будет искать MQTT брокер, 
анонсированный в [сервисе mDNS](https://github.com/mosave/AELib/blob/main/mDNS) (avahi / Bonjur / ZeroConf / mDNS / etc)

Остальные константы зависят от используемых в проекте модулей и описаны ниже.


### AELib.cpp: Поддержка модульной архитектуры прошивки

Реализация методов registerLoop() и Loop() для сборки модулей в прошивке. Обязательно для включения в каждый проект.
Все модули регистрируют себя в цепочке обработки в том порядке, в котором их инициализация перечислена
в функции setup().

### Storage: Менеджер EEPROM памяти. 

В модуле реализовано периодическое сканирование зарегистрированные блоков памяти и
отложенное сохранение изменений в нестираемую EEPROM память. Обязательно для включения в проект.
Для регистрации блока памяти используется функция storageRegisterBlock(blockId, ...).
При этом необходимо учитывать что объем EEPROM для ESP8266 составляет 4096 байт и часть памяти 
уже зарезервирована:

 * 96 байт используется для хранения настроек WiFi и MQTT. Модуль Comms, идентификатор блока памяти 0x43 ('C').
 * 128 байт используется модулем Relays, идентификатор блока памяти 0x52 ('R').

### Comms: WiFi, MQTT и OTA

Основной модуль фреймворка. Обеспечивает управление WiFi, интеграцию в MQTT, настройку и обновление прошивки устройства по воздуху.
Для взаимодействия с MQTT брокером использована библиотека [PubSubClient by Nick O'Leary](https://github.com/knolleary/pubsubclient)

Модуль автоматически публикует следующие топики в поддереве устройства (см. **MQTT_Root**):
 * **Version**: Номер версии прошивки, если в конфигурации определена константа **VERSION** (Retained)
 * **Address**: IP адрес  устройства (Retained)
 * **Online**: "1" или "0". Значение "1" перепосылается каждые 10 минут (heartbeat), 
 "0" выставляется MQTT брокером при пропадении устройства из сети (т.н. Last Will). (Retained)
 * **Activity**: "1" или "0". Выставляется в "1" при обнаружении нажатия кнопки или при вызове функции **triggerActivity()**. Через некоторое время автоматически сбрасывается в 0.

Модуль получает и исполняет следующие команды, полученные через MQTT:
 * **SetName**, payload = "hostname": переименование устройства. Исполняется только если не определена константа WIFI_HostName
 * **SetRoot**, payload = "MQTT root": изменение корневого топика устройства. Исполняется только если не определена константа MQTT_Root
 * **EnableOTA**: включение режима перепрошивки по воздуху. Требуется введения WiFi пароля. Устройство будет перезагружено, если в течение 15 минут не получит запрос на перепрошивку.
 * **Reset**: перезагрузка устройства.
 * **FactoryReset**: Сброс значений в EEPROM и перезагрузка.

### Buttons: обработка "кнопочных" входов с защитой от дребезга и поддержкой виртуальных кнопок

Модуль Buttons поддерживает два режима обработки кнопок.
В режиме по умолчанию события **ShortPressed()**, **LongPressed()** и **VeryLongPressed()** возникают только после отпускания кнопки.
В "простом" режиме событие **LongPressed()** возвращается при нажатии и удержании кнопки не менее одной секунды, 
а событие **VeryLongPressed()** становится недоступно. Для компиляции в "простом" режиме в Config.h необходимо прописать

**#define ButtonsEasyMode**
                           
Функции, возвращающие состояние и события кнопок:
 * **btnState()**: текущее состояние кнопки, нажата или отжата
 * **btnPressed()**, **btnReleased()**: обнаружено нажатие либо отпускание кнопки. Сбрасывается при считывании
 * **btnShortPressed()**: обнаружено короткое нажатие кнопки. Сбрасывается при считывании
 * **btnLongPressed()**: Обнаружено длинное нажатие кнопки. Сбрасывается при считывании
 * **btnVeryLongPressed()**: Обнаружено очень длинное (более 10с) нажатие кнопки. Сбрасывается при считывании

 Кроме того, на "10 быстрых нажатий" любой кнопки можно навесить одно из предопределенных 
 действий: перезагрузить устройство, сбросить настройки, выкючить WiFi и тд.

Состояние именованных кнопок публикуется в MQTT в поддереве устройства:
 * **\<Button\>/State**: "1" или "0", текущее состояние кнопки (retained)
 * **\<Button\>/ShortPressed**: отправляется однократно при обнаружении короткого нажатия
 * **\<Button\>/LongPressed**: отправляется однократно при обнаружении длинного нажатия
 * **\<Button\>/VeryLongPressed**: отправляется однократно при обнаружении очень длинного (более 10с) нажатия



### Relays: управление исполнительными выходами

Если при регистрации реле в качестве имени передано NULL или пустая строка, то ему получит имя в 
формате "Relay\<N\>" и в дальнейшем может быть переименовано по MQTT.

Модуль публикует состояние кнопок в MQTT в поддереве устройства:
 * **\<RelayName\>/State**: "1" или "0", Текущее состояние реле (retained). При запуске реле по умолчанию всегда выключено.
   Значение по умолчанию можно изменить в прошивке а хранение предыдущего значения можно реализовать средствами MQTT, 
   установив отправив топик SetState с флагом retained.
 
Топики, обрабатываемые модулем: 
 * **\<RelayName\>/SetState**, payload "1" или "0": задать текущее состояние реле
 * **\<RelayName\>/Switch**: переключить реле в другое состояние
 * **\<RelayName\>/SetName**, payload = "New Relay Name": переименовать реле, если оно не было жестко определено при регистрации

### LED: управление светодиодом

Реализация нескольких светодиодных эффектов на встроенном или подключенном GPIO светодиоде.
В конфиге необходимо определить константу LED_Pin.

Поддерживаемые эффекты:
 * On, Off, Standby
 * Blink, BlinkFast, BlinkSlow, BlinkTwice, BlinkInverted, BlinkSlowInverted
 * Glowing

### Barometer: Поддержка сенсора BMP280 (барометр + термометр).

Используется библиотека [**BMP280_DEV** by MartinL](https://github.com/MartinL1/BMP280_DEV)

В Config.h необходимо определить пины I2C шины (константы I2C_SDA и I2C_SCL).

Публикуемые MQTT топики:
 * **Sensors/Temperature**: температура (Retained)
 * **Sensors/Pressure**: Давление в мм.рт.ст (Retained)
 * **Sensors/BarometerValid**: Показания сенсора актуальны и действительны (Retained)

### LightMeter: Поддержка сенсора BH1750 (цифровой датчик освещенности)

Используется библиотека [**BH1750** by Christopher Laws](https://github.com/claws/BH1750)

Публикуемые MQTT топики:
 * **Sensors/LightMeter**: текущий уровень освещенности (Retained)
 * **Sensors/LightMeterValid**: Показания сенсора актуальны и действительны (Retained)

### TAH: Поддержка датчика влажности и температуры DHT22 и аналогичных

Используется библиотека [**DHTesp** by beegee-tokyo](https://github.com/beegee-tokyo/DHTesp)

В конфигурации необходимо определить номер пина, к которому подключен сенсор (константа DHT_Pin)

Публикуемые MQTT топики:
 * **Sensors/Temperature**: Температура (Retained)
 * **Sensors/Humidity**: Влажность воздуха (Retained)
 * **Sensors/AbsHumidity**: Абсолютная влажность, г/м3 (Retained)
 * **Sensors/HeatIndex**: "Ощущаемая температура" (Retained)
 * **Sensors/TAHValid**: Показания сенсоры актуальны и действительны (Retained)

### TAH_HTU21D: Поддержка датчика влажности и температуры GY-213 и аналогичных на базе сенсора HTU21D

Используется библиотека [**SparkFun** HTU21D library](https://github.com/sparkfun/SparkFun_HTU21D_Breakout_Arduino_Library)

Модуль подключается через i2c, поэтому перед его инициализацией небоходим вызов Wire.begin() 
MQTT интерфейс полностью совпадает с модулем TAH.

### PIR: Поддержка нескольких детекторов движения

Детекторы движения подключаются к цифровым входам ESP. Если при регистрации датчика движения указать непустое имя, 
то оно будет публиковаться в MQTT. Кроме того можно определить "комбинированный" детектор движения, реагирующий
на срабатывание любого из зарегистрированных датчиков движения

Публикуемые MQTT топики:
 * **\<PIR1\>/Active**, "1" или "0": обнаружено движение (retained)
 * **\<PIR1\>/Enabled**, "1" или "0": датчик включен и активен (retained)
 * **\<PIR1\>/Timeout**: интервал в секундах, в течение которого датчик будет оставаться активен после обнаружения движения (retained)

 Топики, обрабатываемые модулем:
 * **\<PIR1\>/Enable**: включить датчик движения
 * **\<PIR1\>/Disable**: выключить датчик движения
 * **\<PIR1\>/SetTimeout**, payload=<интервал в секундах>: установить таймаут


