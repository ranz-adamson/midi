//////////////////////////
// MIDI Pin assign
// 2 : GND
// 4 : +5V(Vcc) with 220ohm
// 5 : TX
//////////////////////////

#include <MIDI.h>
#include <usbh_midi.h>
#include <usbhub.h>
#include "U8glib.h"

// Satisfy the IDE, which needs to see the include statment in the ino too.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif
#include <SPI.h>


// setup OLED monitor
U8GLIB_SH1106_128X64 u8g(13, 11, 10, 9); // SCK = 13, MOSI = 11, CS = 10, A0 = 9

//Arduino MIDI library v4.2 compatibility
#ifdef MIDI_CREATE_DEFAULT_INSTANCE
MIDI_CREATE_DEFAULT_INSTANCE();
#endif
#ifdef USBCON
#define _MIDI_SERIAL_PORT Serial1
#else
#define _MIDI_SERIAL_PORT Serial
#endif

USB Usb;
USBH_MIDI Midi(&Usb);

void MIDI_poll();

//If you want handle System Exclusive message, enable this #define otherwise comment out it.
#define USBH_MIDI_SYSEX_ENABLE

#ifdef USBH_MIDI_SYSEX_ENABLE
//SysEx:
void handle_sysex( byte* sysexmsg, unsigned sizeofsysex) {
  Midi.SendSysEx(sysexmsg, sizeofsysex);
}
#endif

void setup()
{
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.turnThruOff();
#ifdef USBH_MIDI_SYSEX_ENABLE
  MIDI.setHandleSystemExclusive(handle_sysex);
#endif
  if (Usb.Init() == -1) {
    while (1); //halt
  }//if (Usb.Init() == -1...
  delay( 200 );

  // setup OLED Output
  // flip screen, if required
  u8g.setRot180();

  //set font
  u8g.setFont(u8g_font_5x7);
  
  // set SPI backup if required
  //u8g.setHardwareBackup(u8g_backup_avr_spi);

  // assign default color value
  if ( u8g.getMode() == U8G_MODE_R3G3B2 ) {
    u8g.setColorIndex(255); // white
  }else if ( u8g.getMode() == U8G_MODE_GRAY2BIT ) {
    u8g.setColorIndex(3); // max intensity
  }else if ( u8g.getMode() == U8G_MODE_BW ) {
    u8g.setColorIndex(1); // pixel on
  }else if ( u8g.getMode() == U8G_MODE_HICOLOR ) {
    u8g.setHiColorByRGB(255,255,255);
  }
  u8g.drawStr(0,8,"MIDI Monitor");
  
}

void draw( String str ) {
  // send message to OLED
  int str_len = str.length() + 1;
  char char_array[str_len];
  str.toCharArray(char_array, str_len);
  // draw the screen
  u8g.setPrintPos( 0, 16 );
  u8g.print(char_array);
}

void loop()
{
  uint8_t msg[4];
  int channel, number;
  String str;
  u8g.firstPage();  
  do {  
    Usb.Task();
    if ( Midi ) {
      MIDI_poll();
      if (MIDI.read()) {
        msg[0] = MIDI.getType();
        channel = MIDI.getChannel();
        number = MIDI.getData1();
        switch (msg[0]) {
          case midi::ActiveSensing :
            break;
          case midi::SystemExclusive :
            //SysEx is handled by event.
            break;
          case midi::NoteOn :
            str = String("Note On") + (",Note#") + number;
            break;
          case midi::NoteOff :
            str = String("Note Off") + (",Note#") + number;
            break;
          case midi::ControlChange :
            str = String("CC") + (",CC#") + number;
            break;
          default :
            // send data to OLED
            //str = String("MIDI: ") + number;
            msg[1] = MIDI.getData1();
            msg[2] = MIDI.getData2();
            Midi.SendData(msg, 0);
            break;
        }
        // OLED display
        draw(str);
       
      }
    }
  } while ( u8g.nextPage() );
  // rebuild the picture after a delay
  //delay(500); 
}

// Poll USB MIDI Controler and send to serial MIDI
void MIDI_poll()
{
  uint8_t size;
#ifdef USBH_MIDI_SYSEX_ENABLE
  uint8_t recvBuf[MIDI_EVENT_PACKET_SIZE];
  uint8_t rcode = 0;     //return code
  uint16_t  rcvd;
  uint8_t   readPtr = 0;

  rcode = Midi.RecvData( &rcvd, recvBuf);

  //data check
  if (rcode != 0) return;
  if ( recvBuf[0] == 0 && recvBuf[1] == 0 && recvBuf[2] == 0 && recvBuf[3] == 0 ) {
    return ;
  }

  uint8_t *p = recvBuf;
  while (readPtr < MIDI_EVENT_PACKET_SIZE)  {
    if (*p == 0 && *(p + 1) == 0) break; //data end

    uint8_t outbuf[3];
    uint8_t rc = Midi.extractSysExData(p, outbuf);
    if ( rc == 0 ) {
      p++;
      size = Midi.lookupMsgSize(*p);
      _MIDI_SERIAL_PORT.write(p, size);
      p += 3;
    } else {
      _MIDI_SERIAL_PORT.write(outbuf, rc);
      p += 4;
    }
    readPtr += 4;
  }
#else
  uint8_t outBuf[3];
  do {
    if ( (size = Midi.RecvData(outBuf)) > 0 ) {
      //MIDI Output
      _MIDI_SERIAL_PORT.write(outBuf, size);
    }
  } while (size > 0);
#endif
}
