// Подключение библиотек
#include <EEPROM.h>
#include <LCD5110_Basic.h>
#include <MFRC522.h>

#define MFRC_RST_PIN 9
#define MFRC_SS_PIN 10

#define LCD_SCK_PIN 7
#define LCD_MOSI_PIN 6
#define LCD_DC_PIN 5
#define LCD_RST_PIN 4
#define LCD_CS_PIN 3

#define LCD_LN1 0
#define LCD_LN2 8
#define LCD_LN3 16
#define LCD_LN4 24
#define LCD_LN5 32
#define LCD_LN6 40

#define DELAY_ACCESS 5000

#define BYTE_BUFFER_UID 4
#define BYTE_BUFFER_NAME 14
#define DICTIONARY_LENGTH 14

// Инициализация MFRC522
MFRC522 mfrc522(MFRC_SS_PIN, MFRC_RST_PIN); // Create MFRC522 instance.

LCD5110 myGLCD(LCD_SCK_PIN, LCD_MOSI_PIN, LCD_DC_PIN, LCD_RST_PIN, LCD_CS_PIN); // объявляем номера пинов LCD

extern uint8_t SmallFont[]; // малый шрифт (из библиотеки)

byte* _storedUIDs [DICTIONARY_LENGTH];
byte* _storedNames [DICTIONARY_LENGTH];

byte _recordNumber = -1;
byte* _lastUID;

String lastUser = "NONE";
unsigned long previousMillis = 0;
bool displayLastUser = false;

enum MenuScreen
{
  msNone,
  msMain,
  msList,
  msAddNumber,
  msAddName,
  msDelete
};

MenuScreen _currentMenuScreen = msNone;
bool serialOutput = true;

void setup()
{
  Serial.begin(9600); // инициализация последовательного порта
  SPI.begin(); // инициализация SPI
  
  mfrc522.PCD_Init(); // инициализация MFRC522
  
  myGLCD.InitLCD(); // инициализация LCD дисплея
  myGLCD.clrScr(); // очистка экрана
  myGLCD.setFont(SmallFont); // задаём размер шрифта
  
  // ОТЛАДКА - добавление записей пользователей в EEPROM   
  /*
  byte uidOne[BYTE_BUFFER_UID] = {0x60, 0x3C, 0x4D, 0x80};
  byte uidTwo[BYTE_BUFFER_UID] = {0xA0, 0xFF, 0xF2, 0x79};
                                  //12345678901234
  byte nameOne[BYTE_BUFFER_NAME] = "TRIFONOV_V";
  byte nameTwo[BYTE_BUFFER_NAME] = "BOLSHAKOV_R";

  int origin = 0;
  
  origin = eeprom_safe_write(uidOne, 0, BYTE_BUFFER_UID);
  origin = eeprom_safe_write(nameOne, 4, BYTE_BUFFER_NAME);
  origin = eeprom_safe_write(uidTwo, 18, BYTE_BUFFER_UID);
  origin = eeprom_safe_write(nameTwo, 22, BYTE_BUFFER_NAME);  
*/
  Serial.println("===Smart Coffee Machine 1.0===");
  Serial.println();

  // EEPROM readings
  int recordsCount = load_users_data();

  Serial.print("--- ");
  Serial.print(recordsCount);
  Serial.println(" records have been successfully get from EEPROM");
  
  print_menu_welcome_message();
}

void loop()
{
  // change text every 5 seconds
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > DELAY_ACCESS)
  {
    previousMillis = currentMillis;
    displayLastUser = !displayLastUser;
    myGLCD.clrRow(2);
    myGLCD.clrRow(3);
  }
  
  myGLCD.print("Wanna coffee?)", CENTER, LCD_LN1);

  if (displayLastUser)
  {
    myGLCD.print("Place your >", RIGHT, LCD_LN3);
    myGLCD.print("mug there >", RIGHT, LCD_LN4);
  }
  else
  {
    myGLCD.print("Last user:", CENTER, LCD_LN3);
    myGLCD.print(lastUser, CENTER, LCD_LN4);
  }

  // get user input
  int commandLength = Serial.available();
  if (commandLength > 0)
  {
    char command[commandLength];
    for (int i = 0; i < commandLength; ++i)
    {
      char inchar = Serial.read();
      
      if (inchar != '\n')  
        command[i] = inchar;
      else
        commandLength--;  // reduce command length if \n presents in input string
    }

    if (commandLength > 0)
      show_menu(command, commandLength);
  }

  // try to read card
  if ( ! mfrc522.PICC_IsNewCardPresent())
    return;

  if ( ! mfrc522.PICC_ReadCardSerial())
    return;
    
  // UID
    byte* uid = mfrc522.uid.uidByte;
    byte uidSize = mfrc522.uid.size;

    _lastUID = uid;     // CAREFUL WITH SIZES!

    if (serialOutput)
    {
      Serial.println();
      Serial.println(F("Card UID:"));
      dump_byte_array(uid, min(uidSize, BYTE_BUFFER_UID));  
    }
    
    myGLCD.clrRow(2);
    myGLCD.clrRow(3);

    String userName;
    if (get_user_name_by_uid(uid, userName))
    {
      myGLCD.print(userName, CENTER, LCD_LN3);
      myGLCD.print("Access granted", CENTER, LCD_LN4);

      // save user name for onscreen demonstartion
      lastUser = userName;
    }
    else
    {
      myGLCD.print("Unknown UID", CENTER, LCD_LN3);
      myGLCD.print("Access denied", CENTER, LCD_LN4);
    }

    delay(DELAY_ACCESS);
    myGLCD.clrRow(2);
    myGLCD.clrRow(3);
}

void show_menu(char* command, byte commandLength)
{
  int inNumber = char_array_to_int(command, commandLength);                 // used in main menu and record number inputs
  int initialShift = _recordNumber * (BYTE_BUFFER_UID + BYTE_BUFFER_NAME);  // used in add/replace and delete commands
  
  switch (_currentMenuScreen)
  {
    case msNone:
    case msList:
      _currentMenuScreen = msMain;
      serialOutput = false;
      
      Serial.println();
      Serial.println("===Main menu===");
      Serial.println("1 - Show user list");
      Serial.println("2 - Add/replace user");
      Serial.println("3 - Remove user");
      Serial.println("Other - Exit menu");
      Serial.println();
      break;
      
    case msMain:      
      switch (inNumber)
      {
        case 1:
          _currentMenuScreen = msList;
          
          for (int i =0; i < DICTIONARY_LENGTH; ++i)
          {
            Serial.print(i);
            
            Serial.print("\tUID: ");
            dump_byte_array(_storedUIDs[i], BYTE_BUFFER_UID);
            
            Serial.print("\tName: ");
            Serial.println((char*)_storedNames[i]);
          }

          Serial.println();
          Serial.println(">>> type anything to enter menu <<<");
          break;

        case 2:
          _currentMenuScreen = msAddNumber;
          
          Serial.println("<<< Enter record number to ADD/REPLACE USER");
          break;

        case 3:
          _currentMenuScreen = msDelete;

          Serial.println("<<< Enter record number to DELETE");
          break;
        
        default:
          _currentMenuScreen = msNone;
          serialOutput = true;

          print_menu_welcome_message();
      }
      break;

    case msAddNumber:
      if (0 > inNumber || DICTIONARY_LENGTH <= inNumber)
      {
        Serial.print(command);
        Serial.println(" is incorrect number!");

        _currentMenuScreen = msNone;
        print_menu_welcome_message();
      }
      else
      { 
        _recordNumber = inNumber;

        // scan new UID [don't know any way to do this better =(]
        Serial.println("Place UID source on the tray...");
        
        while(!mfrc522.PICC_IsNewCardPresent()) {}
        while(!mfrc522.PICC_ReadCardSerial()) {}
    
        _lastUID = mfrc522.uid.uidByte;
        byte uidSize = mfrc522.uid.size;

        Serial.println("UID reading OK");
        Serial.println();

        // enter new user name
        _currentMenuScreen = msAddName;
        Serial.println("<<< Enter new user name (or keep blank to use current name)");
        Serial.println();
      }
      break;

    case msAddName:
      if (commandLength > BYTE_BUFFER_NAME)
        Serial.println("User name is too long and will be cut off automatically!");

      // create limited records (nice whitespaces and no overflows)
      byte newUID[BYTE_BUFFER_UID];
      for (int i = 0; i < BYTE_BUFFER_UID; ++i)
        newUID[i] = _lastUID[i];

      byte newName[BYTE_BUFFER_NAME];
      for (int i = 0; i < BYTE_BUFFER_NAME; ++i)
        newName[i] = i < commandLength ? command[i] : 0x00;
      
      eeprom_safe_write(newUID, initialShift, BYTE_BUFFER_UID);
      eeprom_safe_write(newName, initialShift + BYTE_BUFFER_UID, BYTE_BUFFER_NAME);

      Serial.println("Record has been modified.");

      // reload data
      load_users_data();

      _currentMenuScreen = msNone;
      serialOutput = true;
      print_menu_welcome_message();
      break;

    case msDelete:
    if (0 > inNumber || DICTIONARY_LENGTH <= inNumber)
      {
        Serial.print(command);
        Serial.println(" is incorrect number!");
      }
      else
      { 
        _recordNumber = inNumber;
      
        // write blank record to delete it
        byte blankUID[BYTE_BUFFER_UID] = {0xFF, 0xFF, 0xFF, 0xFF};
        byte blankName[BYTE_BUFFER_NAME];
        for (int i = 0; i < BYTE_BUFFER_NAME; ++i)
          blankName[i] = 0x00;
        
        eeprom_safe_write(blankUID, initialShift, BYTE_BUFFER_UID);
        eeprom_safe_write(blankName, initialShift + BYTE_BUFFER_UID, BYTE_BUFFER_NAME);
        
        Serial.println("Record has been deleted.");
  
        // reload data
        load_users_data();
      }

      _currentMenuScreen = msNone;
      serialOutput = true;
      print_menu_welcome_message();
      break;
  }
}

void print_menu_welcome_message()
{
  Serial.println();
  Serial.println(">>> type anything to enter menu <<<");
}

int char_array_to_int(char* chArray, int chArraySize)
{  
  for (int i = 0; i < chArraySize; ++i)
    if (!isDigit(chArray[i]))
      return -1;

  String str(chArray);
  return str.toInt();
}

int load_users_data()
{
  int recordsCount = 0;
  
  for (int i = 0, index = 0; i < DICTIONARY_LENGTH; i++)
  {
    _storedUIDs[i] = new byte[BYTE_BUFFER_UID];
    _storedNames[i] = new byte[BYTE_BUFFER_NAME];

    if (eeprom_read(index, BYTE_BUFFER_UID, _storedUIDs[i]))
      recordsCount++; // only not-FF records
      
    index += 4;
    
    eeprom_read(index, BYTE_BUFFER_NAME, _storedNames[i]);
    index += 14;
  }

  return recordsCount;
}

bool get_user_name_by_uid(byte* uid, String& userName)
{
  for(int i = 0; i < DICTIONARY_LENGTH; i++)
  {
    if (ByteArrayCompare(_storedUIDs[i], uid, BYTE_BUFFER_UID))
    {
      userName = String((char*)_storedNames[i]);
      userName.trim();

      if (serialOutput)
      {
        Serial.print(">");
        Serial.print(userName);
        Serial.print("<");
      }
      
      return true;
    }
  }

  return false;
}

bool ByteArrayCompare(byte* a,byte* b,int array_size)
{
   for (int i = 0; i < array_size; ++i)
     if (a[i] != b[i])
       return(false);
   return(true);
}

int eeprom_safe_write(byte* strBuffer, byte origin, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
    EEPROM.update(origin + i, strBuffer[i]);

  return origin + bufferSize;
}

bool eeprom_read(byte origin, byte bufferSize, byte*& outArray)
{
  bool allFFs = true;
  
  for (byte i = 0; i < bufferSize; i++)
  {
    outArray[i] = EEPROM.read(origin + i);

    if (outArray[i] != 0xFF)
      allFFs = false;
  }

  return !allFFs;
}

// Вывод результата чтения данных в HEX-виде
void dump_byte_array(byte *byteBuffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(byteBuffer[i] < 0x10 ? " 0" : " ");
    Serial.print(byteBuffer[i], HEX);
  }
}


