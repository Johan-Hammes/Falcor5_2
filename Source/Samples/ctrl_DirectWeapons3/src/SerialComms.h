#ifndef SERIAL_H
#define SERIAL_H

//#include "stdafx.h"
#include "windows.h"
enum eComPort {
  COM1,
  COM2,
  COM3,
  COM4,
  COM5,
  COM6,
  COM7,
  COM8,
  COM9,
  COM10,
  COM11,
  COM12,
};

enum eBaudRate {
  br_2400,
  br_4800,
  br_9600,
  br_14400,
  br_19200,
  br_38400,
  br_56000,
  br_57600,
  br_115200,
  br_230400, 
  br_460800, 
  br_921600,
  brLAST
};

enum eParity {
  ptNONE,
  ptODD,
  ptEVEN,
  ptMARK,
  ptSPACE
};

enum eStopBits {
  sbONE,
  sbONE_POINT_FIVE,
  sbTWO
};


class CCommunication
{
private:
  bool   CommPortOpen;
  HANDLE CommsHandle;
  HANDLE MutexHandle;
protected:
public:
  CCommunication(void);
  ~CCommunication(void);
  int OpenPort(eComPort ComPort, eBaudRate BaudRate, unsigned int ByteSize, eParity Parity, eStopBits StopBits);
  void ClosePort(void);
  unsigned int Write(unsigned char *Data, unsigned Len);
  unsigned long Read(unsigned char *RxBuf, unsigned int Len);
  bool IsOpen(void);
};

#endif