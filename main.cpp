// Define the pins we're going to call pinMode on
int LED = D7; // This one is the built-in tiny one to the right of the USB jack

#define MAC_BYTES 6
#define REPEAT_MAC 16
#define MAGIC_HEADER_LENGTH 6
#define REPEAT_PACKET 5


uint8_t bootSecs = 5; // Number of seconds to wait for computer to boot
uint16_t port = 9; // Port to send WoL to. Usually 7 or 9. Use 9 for testing with WireShark
IPAddress broadcastIP(255,255,255,255); // Broadcast IP

IPAddress pingIP; 
char wolVarState[32];
char ParticleHostAddress[16];
bool hasSentPingingStatus = false;
bool hasSentWaitingStatus = false;
char MacAddress[80];

enum WolState
{
    NotConnected,
    Waiting,
    SendingWol,
    WolSent,
    TestingAwake,
    TestingAwake2,
    TestingAwake3,
    FailedToWakeWaiting,
    ConfirmedAwakeWaiting,
};

WolState wolState = NotConnected;

// Define our LED Status displays.
// Sending WOL packets
LEDStatus LEDSendingWol(RGB_COLOR_YELLOW, LED_PATTERN_BLINK, LED_SPEED_SLOW, LED_PRIORITY_IMPORTANT);

// Pinging to check status
LEDStatus LEDPinging(RGB_COLOR_YELLOW, LED_PATTERN_BLINK, LED_SPEED_FAST, LED_PRIORITY_IMPORTANT);

// Confirmed to be awake
LEDStatus LEDConfirmedAwakeWaiting(RGB_COLOR_GREEN, LED_PATTERN_FADE);

// Not awake
LEDStatus LEDFailedToAwakeWaiting(RGB_COLOR_RED, LED_PATTERN_FADE);

// Set the Wifi status LED states
void setSystemTheme() {
    LEDSystemTheme theme;
    
    theme.setColor(LED_SIGNAL_NETWORK_CONNECTING, RGB_COLOR_WHITE);
    theme.setColor(LED_SIGNAL_NETWORK_DHCP, RGB_COLOR_WHITE);
    theme.setColor(LED_SIGNAL_NETWORK_CONNECTED, RGB_COLOR_WHITE);
    theme.setColor(LED_SIGNAL_CLOUD_CONNECTED, RGB_COLOR_YELLOW);
    theme.apply();
}

STARTUP(setSystemTheme());


// Set the LED status to one of the specific statuses while disabling the others.
void setLEDStatus(uint8_t status) {
    LEDSendingWol.setActive(false);
    LEDPinging.setActive(false);
    LEDConfirmedAwakeWaiting.setActive(false);
    LEDFailedToAwakeWaiting.setActive(false);
    
    switch (status) {
        case 0:
            LEDSendingWol.setActive(true);
            break;
        case 1: 
            LEDPinging.setActive(true);
            break;
        case 2:
            LEDConfirmedAwakeWaiting.setActive(true);
            break;
        case 3:
            LEDFailedToAwakeWaiting.setActive(true);
            break;
        case 4:
            break;
    }
}


// Convert two hex digits into a byte.
uint8_t hex_to_byte(uint8_t h, uint8_t l) {
    uint8_t retval = 0x00;

    // higher nibble
    if (h >= 0x30 && h <= 0x39) { // 0-9
        retval |= (h - 0x30) << 4;
    }

    if (h >= 0x41 && h <= 0x46) { // A-F
        retval |= (h - 0x41 + 0x0A) << 4;
    }

    if (h >= 0x61 && h <= 0x66) { // a-f
        retval |= (h - 0x61 + 0x0A) << 4;
    }

    // lower nibble
    if (l >= 0x30 && l <= 0x39) { // 0-9
        retval |= l - 0x30;
    }

    if (l >= 0x41 && l <= 0x46) { // A-F
        retval |= l - 0x41 + 0x0A;
    }

    if (l >= 0x61 && l <= 0x66) { // a-f
        retval |= l - 0x61 + 0x0A;
    }

    return retval;
}


// Copy state to string and publish a wolstate event
void publishState(String string) {
  strcpy(wolVarState, string);
  Particle.publish("wolstate", string, 60, PRIVATE);
}


// Parse a MAC into the given target
void parseMacAddress(const char* string, uint8_t* target) {
    uint8_t i = 0;
    uint8_t j = 0;
    uint8_t max = 17; // MAC String is 17 characters.
    while (i < max) {
        target[j++] = hex_to_byte(string[i], string[i + 1]);
        i += 3; // Skip the colons
    }
}


// Same but for IPs
bool parseIPAddress(String string, IPAddress* target) {
    uint8_t values[4] = { 0, 0, 0, 0 };
    int prevIndex = -1;
    for (int i = 0; i < 4; ++i)
    {
        int dotIndex = string.indexOf('.', prevIndex + 1);
        if (dotIndex < 0)
        {
            if (i != 3)
            {
                // ERROR
                return false;
            }

            dotIndex = string.length();
        }

        values[i] = string.substring(prevIndex + 1, dotIndex).toInt();
        prevIndex = dotIndex;
    }

    *target = IPAddress(values);

    return true;
}


// Format an IP
void formatIPAddress(const IPAddress& ipAddress, char* target) {
    String ip(ipAddress[0]);
    for (int i = 1; i < 4; ++i)
    {
        ip.concat(".");
        ip.concat(ipAddress[i]);
    }
    ip.toCharArray(target, ip.length() + 1);
}


// Wake a computer by sending the magic packet
int wake(const char* mac) {
    uint8_t contents[MAGIC_HEADER_LENGTH + REPEAT_MAC * MAC_BYTES];
    uint8_t rawMac[MAC_BYTES];

    parseMacAddress(mac, rawMac);

    UDP udp;
    udp.begin(port);
    udp.beginPacket(broadcastIP, port);

    for (int i = 0; i < MAGIC_HEADER_LENGTH; i++) {
        contents[i] = 0xFF;
    }
    for (uint8_t i = MAGIC_HEADER_LENGTH; i < sizeof contents; i++) {
        contents[i] = rawMac[(i - MAGIC_HEADER_LENGTH) % MAC_BYTES];
    }

    udp.write(contents, sizeof contents);

    udp.endPacket();
    udp.stop();

    return TRUE;
}



// Function called from cloud to wake computer
// Takes IP;MAC as parameter
// E.G. "192.168.1.1;00:00:00:00:00:01"
int wakeHost(String param) {
    if (param.length() == 0)
    {
        return FALSE;
    }

    int index = param.indexOf(';');
    if (index == -1 || param.indexOf(';', index + 1) >= 0 || !parseIPAddress(param.substring(0, index), &pingIP))
    {
        return FALSE;
    }

    param.substring(index + 1).toCharArray(MacAddress, 80);
    wolState = SendingWol;
    return TRUE;
}


// Function to call for pinging a host. Takes IP as arg
int pingHost(String param) {
    if (param.length() == 0)
    {
        return FALSE;
    }

    if (!parseIPAddress(param, &pingIP))
    {
        return FALSE;
    }
    
    hasSentPingingStatus = false;
    wolState = TestingAwake;
    return TRUE;
}

void setup() {
    pinMode(LED, OUTPUT);

    strcpy(wolVarState, "Waiting");
    Particle.variable("state", wolVarState, STRING);
    
    formatIPAddress(WiFi.localIP(), ParticleHostAddress);
    
    Particle.variable("address", ParticleHostAddress, STRING);
    
    Spark.function("wakeHost", wakeHost);
    Spark.function("pingHost", pingHost);

    wolState = Waiting;
}

void loop() {
    // Main state machine. Manages all the cloud commands and publish events.
    switch (wolState)
    {
        case NotConnected:
        case Waiting:
            if (!hasSentWaitingStatus) {
                publishState("Waiting"); // Publish the message that we're waiting just once
                
                hasSentWaitingStatus = true;
                
                setLEDStatus(4); // Set LED status to waiting
            }
            break;
        case SendingWol:
            setLEDStatus(0); // Show that we are in the sending phase
            for (int i = 0; i < REPEAT_PACKET; i++) {
                wake(MacAddress);
            }
            wolState = WolSent;
            break;
        case WolSent:
            publishState("Sent WOL");
        
            delay(bootSecs * 1000UL);  // Wait for computer to boot before pinging. 
            
            wolState = TestingAwake;
            hasSentPingingStatus = false;
            break;
        case TestingAwake2:
        case TestingAwake3:
            delay(1000);              // Wait for 1 second
        case TestingAwake:
        {
            if (!hasSentPingingStatus) {
                publishState("Pinging"); // Publish the message that we're pinging just once
                hasSentPingingStatus = true;
                setLEDStatus(1); // Set LED status to pinging
            }

            if (WiFi.ping(pingIP) > 0) {
                wolState = ConfirmedAwakeWaiting;
            }
            else {
                wolState = (WolState) (wolState + 1);
            }
            break;
        }
        case ConfirmedAwakeWaiting:
            publishState("Reachable");
            
            setLEDStatus(2);
            delay(4000);
            
            hasSentWaitingStatus = false;
            wolState = Waiting;
            
            break;
        case FailedToWakeWaiting:
            publishState("Unreachable");
            
            setLEDStatus(3);
            delay(4000);
            
            hasSentWaitingStatus = false;
            wolState = Waiting;
            
            break;
    }
}
