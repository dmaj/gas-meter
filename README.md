Kann jetzt endlich meinen Gaszähler remote auslesen. Habe ein Progrämmchen für den ESP32 gebastelt. Ist allerdings mit heißer Nadel gestrickt und hat sicher noch einige Bugs. Die Benutzung geschieht auf eigenes Risiko. Die erste Version war in einem Kommentar von “Tkaule” auf dieser Webseite: [ng3d-druck](https://ng3d-druck.de/products/reedkontakt-sensor-bk-gaszaehler-esp32). Ich habe dann einige Stunden investiert, um noch einige Features dazu zu packen. Auf der Seite kann man sich auch eine passende Halterung kaufen. 




**📘 Bedienungsanleitung: Smart Gas Meter (ESP32-C3)**

Dieses Gerät macht deinen analogen **BK-G4 Gaszähler** smart. Es misst Impulse via Reed-Kontakt, berechnet den Durchfluss in Echtzeit und sendet die Daten per MQTT an dein Smart Home (z. B. ioBroker).

### **1\. Status-Signale (LED an Pin 10\)**

Die verbaute RGB-LED kommuniziert den aktuellen Zustand des Geräts:

* **🔵 Blau (Dauerlicht):** **Setup-Modus.** Das Gerät hat keine WLAN-Daten und stellt ein eigenes Funknetz bereit.  
* **🔴 Rot (Dauerlicht):** **WLAN-Fehler.** Die Zugangsdaten sind korrekt, aber der Router ist nicht erreichbar.  
* **🟢 Grüner Blitz:** **Impuls-Bestätigung.** Ein Gas-Impuls (0,01 m³) wurde physikalisch erkannt.  
* **🟣 Violett (Dauerlicht):** **Reset-Warnung.** Der Boot-Button wird gerade gedrückt; nach 5 Sek. erfolgt der Werksreset.

### ---

**2\. Ersteinrichtung (WLAN-Konfiguration)**

Wenn das Gerät neu ist oder zurückgesetzt wurde (LED leuchtet blau):

1. Suche mit dem Smartphone nach dem WLAN **"Gas-Zaehler-Setup"**.  
2. Verbinde dich (Passwort: 12345678).  
3. Öffne im Browser die Adresse 192.168.4.1.  
4. Gib deine **WLAN-SSID** und das **Passwort** ein und klicke auf "Speichern".  
5. Das Gerät startet neu und verbindet sich mit deinem Heimnetzwerk.

### ---

**3\. Konfiguration über das Web-Interface**

Sobald das Gerät im WLAN ist, rufst du seine IP-Adresse im Browser auf. Klicke oben rechts auf das **Burger-Menü (☰)**:

#### **A. MQTT Setup**

Hier hinterlegst du die Daten deines Brokers (z. B. ioBroker IP):

* **Server:** IP-Adresse deines Brokers.  
* **Port:** Standardmäßig 1883\.  
* **Topic:** Das Basis-Präfix für die Datenpunkte (z. B. gas). Der ESP sendet dann automatisch an gas/counter, gas/ticks und gas/status.

#### **B. Basiswert setzen**

Damit die digitale Anzeige mit deinem echten Zähler übereinstimmt:

1. Lies den aktuellen Stand am mechanischen Zähler ab (z. B. 12345,678).  
2. Gib diesen Wert im Menü ein.  
3. Der ESP32 zählt ab sofort von diesem Wert aus weiter.

### ---

**4\. Hardware-Reset (Werkseinstellungen)**

Falls du den Router gewechselt hast oder das Gerät umziehen soll:

1. Halte den **Boot-Button** am ESP32 gedrückt.  
2. Nach **5 Sekunden** wechselt die LED zu **Violett**.  
3. Lasse den Button los.  
4. Alle WLAN-Daten werden gelöscht, und das Gerät startet wieder im blauen **Setup-Modus**.

### ---

**5\. Montage am BK-G4 Zähler**

* **Position:** Der Sensor muss in der Aussparung direkt unter der letzten (rechten) Ziffernrolle sitzen.  
* **Test:** Prüfe, ob die LED bei jeder Umdrehung der 0,01er-Rolle (meist bei der Ziffer 6\) genau **einmal** grün aufblitzt.  
* **Fixierung:** Einfach mit Klebeband den Reed-Kontakt in dem kleinen Fach unterhalb des Zählers fixieren. Oder eine schicke Halterung ordern: [Link](https://ng3d-druck.de/products/reedkontakt-sensor-bk-gaszaehler-esp32) 

### ---

**6\. Datenpunkte (MQTT)**

Der ESP32 sendet folgende Werte an deinen Broker:

| Topic | Inhalt | Einheit |

| :--- | :--- | :--- |

| topic/counter | Aktueller Gesamtzählerstand | $m^3$ |

| topic/ticks | Anzahl der Impulse seit Reset | \- |

| topic/status | Online-Status des Geräts | online/offline |
