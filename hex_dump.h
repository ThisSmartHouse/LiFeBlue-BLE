/* hex_hump (char *data, int size, char *caption)
*  
*  From: http://www.alexonlinux.com/hex-dump-functions
*  
*  Modified to output in the Arduino IDE environment -- JR
*  
*/

extern "C" {
  void hex_dump(char *data, int size, char *caption);
}
