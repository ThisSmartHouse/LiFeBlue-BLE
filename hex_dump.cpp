/* hex_hump (char *data, int size, char *caption)

   From: http://www.alexonlinux.com/hex-dump-functions

   Modified to output in the Arduino IDE environment -- JR

*/


#include <string.h>
#include "Arduino.h"  // Added reference to Arduino functions -- JR
#include "hex_dump.h"

extern "C" {
  void hex_dump(char *data, int size, char *caption)
  {
    int i; // index in data...
    int j; // index in line...
    char temp[8];
    char buffer[128];
    char *ascii;

    memset(buffer, 0, 128);

    Serial.printf("\n---------> %s <--------- (%d bytes from %p)\n", caption, size, data);

    // Printing the ruler...
    Serial.printf("        +0          +4          +8          +c            0   4   8   c   \n");

    // Hex portion of the line is 8 (the padding) + 3 * 16 = 52 chars long
    // We add another four bytes padding and place the ASCII version...
    ascii = buffer + 58;
    memset(buffer, ' ', 58 + 16);
    buffer[58 + 16] = '\n';
    buffer[58 + 17] = '\0';
    buffer[0] = '+';
    buffer[1] = '0';
    buffer[2] = '0';
    buffer[3] = '0';
    buffer[4] = '0';
    for (i = 0, j = 0; i < size; i++, j++)
    {
      if (j == 16)
      {
        Serial.printf("%s", buffer);
        memset(buffer, ' ', 58 + 16);

        sprintf(temp, "+%04x", i);
        memcpy(buffer, temp, 5);

        j = 0;
      }

      sprintf(temp, "%02x", 0xff & data[i]);
      memcpy(buffer + 8 + (j * 3), temp, 2);
      if ((data[i] > 31) && (data[i] < 127))
        ascii[j] = data[i];
      else
        ascii[j] = '.';
    }

    if (j != 0)
      Serial.printf("%s", buffer);
    Serial.printf("\t\t ======= END OF BUFFER DUMP  ======= \n\n");
    Serial.flush();
  }
}
