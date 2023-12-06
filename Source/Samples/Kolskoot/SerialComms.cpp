#include "SerialComms.h"


//---------------------------------------------------------------------------

/******************************************************************************/
/*Function Name:            CCommunication                                    */
/*                                                                            */
/*Function Description:     Communication constructor                         */
/*                                                                            */
/*Return Value Descript:    None                                              */
/*                                                                            */
/*Function Para. Descript:  None                                              */
/*                                                                            */
/*Author :                  Wynand Erasmus                                    */
/*                                                                            */
/*Comments:                                                                   */
/*                                                                            */
/******************************************************************************/
CCommunication::CCommunication(void)
{
  CommPortOpen = false;
}
//---------------------------------------------------------------------------

/******************************************************************************/
/*Function Name:            ~CCommunication                                   */
/*                                                                            */
/*Function Description:     Communication destructor                          */
/*                                                                            */
/*Return Value Descript:    None                                              */
/*                                                                            */
/*Function Para. Descript:  None                                              */
/*                                                                            */
/*Author :                  Wynand Erasmus                                    */
/*                                                                            */
/*Comments:                                                                   */
/*                                                                            */
/******************************************************************************/
CCommunication::~CCommunication()
{
  if(CommsHandle != NULL) {
    CloseHandle(CommsHandle);
  }
}
//---------------------------------------------------------------------------

/******************************************************************************/
/*Function Name:            OpenPort                                          */
/*                                                                            */
/*Function Description:     Opens and setup communication port                */
/*                                                                            */
/*Return Value Descript:    int - 0 = ok,                                     */
/*                                1 = open port error                         */
/*                                2 = internal input/output buffer error      */
/*                                3 = get current control settings error      */
/*                                4 = new control setup error                 */
/*                                5 = timeout setup error                     */
/*                                                                            */
/*Function Para. Descript:  eComPort - comms port                             */
/*                          eBaudRate - comms baudrate                        */
/*                          unsigned int - comms byte data size               */
/*                          eParity - comms parity                            */
/*                          eStopBits - number of stop bits                   */
/*                                                                            */
/*Author :                  Wynand Erasmus                                    */
/*                                                                            */
/*Comments:                                                                   */
/*                                                                            */
/******************************************************************************/
int CCommunication::OpenPort(eComPort ComPort, eBaudRate BaudRate, unsigned int ByteSize, eParity Parity, eStopBits StopBits)
{
  DCB dcb;
  COMMTIMEOUTS comm_timeouts;
  int error = 0;
  int success;
  const unsigned int baud_rates[brLAST] = {
     2400, 4800, 9600, 14400, 19200, 38400, 56000, 57600, 115200, 230400, 460800, 921600
  };
//  const CString comm_ports[12] = {
//	  "COM1:", "COM2:", "COM3:", "COM4:", "COM5:", "COM6:",
  //  "COM7:", "COM8:", "COM9:", "\\\\.\\COM10:", "\\\\.\\COM11:", "\\\\.\\COM12:"
 // };
  
  // If handle already exists, close port and delete handle
  if(CommsHandle != NULL) {
    CloseHandle(CommsHandle);
    CommsHandle = NULL;
	  CommPortOpen = false;
  }

  WCHAR comStr[32];
  wsprintf( comStr, L"COM%d:", ComPort+1 );
  // Open comms port for reading and writing
  CommsHandle = CreateFile(comStr,          //L"COM1:",//comm_ports[ComPort],
                           GENERIC_READ | GENERIC_WRITE,
                           0,
                           NULL,
                           OPEN_EXISTING,
                           FILE_FLAG_WRITE_THROUGH,
                           NULL);
  
  // Setup port with input settings. Returns on any error
  if(CommsHandle == INVALID_HANDLE_VALUE) {
    error = 1;
  }
  else {
    // Set input/output buffer sizes
    success = SetupComm(CommsHandle, 4096, 1024);
    if(!success) {
      error = 2;
    }
    else {
      // Get current control settings
      success = GetCommState(CommsHandle, &dcb);
      if(success) {
        // Use input settings to change control settings
        dcb.BaudRate = baud_rates[BaudRate];
        dcb.ByteSize = ByteSize;
        dcb.Parity   = Parity;
        dcb.StopBits = StopBits;
        success = SetCommState(CommsHandle, &dcb);
        if(success) {
          // Set comms timeouts
          comm_timeouts.ReadIntervalTimeout         = MAXDWORD;
          comm_timeouts.ReadTotalTimeoutMultiplier  = 0;
          comm_timeouts.ReadTotalTimeoutConstant    = 0;
          comm_timeouts.WriteTotalTimeoutMultiplier = 10;
          comm_timeouts.WriteTotalTimeoutConstant   = 1;
          success = SetCommTimeouts(CommsHandle, &comm_timeouts);
          if(success) {
            PurgeComm(CommsHandle, PURGE_TXCLEAR|PURGE_RXCLEAR);
            MutexHandle = CreateMutex(NULL, false, NULL);
			      CommPortOpen = true;
          }
          else {
            error = 5;
          }
        }
        else {
          error = 4;
        }
      }
      else {
        error = 3;
      } 
    }
  }

  return(error);
}
//---------------------------------------------------------------------------

/******************************************************************************/
/*Function Name:            ClosePort                                         */
/*                                                                            */
/*Function Description:     Closes comms port                                 */
/*                                                                            */
/*Return Value Descript:    None                                              */
/*                                                                            */
/*Function Para. Descript:  None                                              */
/*                                                                            */
/*Author :                  Wynand Erasmus                                    */
/*                                                                            */
/*Comments:                                                                   */
/*                                                                            */
/******************************************************************************/
void CCommunication::ClosePort(void)
{
  if(CommsHandle != NULL) {
    CloseHandle(CommsHandle);
    CommsHandle = NULL;
  }

  CommPortOpen = false;
}
//---------------------------------------------------------------------------

/******************************************************************************/
/*Function Name:            Write                                             */
/*                                                                            */
/*Function Description:     Sends data                                        */
/*                                                                            */
/*Return Value Descript:    unsigned int - number of bytes sent               */
/*                                                                            */
/*Function Para. Descript:  unsigned char* - tx data buffer                   */
/*                          unsigned int - tx data size                       */
/*                                                                            */
/*Author :                  Wynand Erasmus                                    */
/*                                                                            */
/*Comments:                                                                   */
/*                                                                            */
/******************************************************************************/
unsigned int CCommunication::Write(unsigned char *Data, unsigned Len)
{
  unsigned long bytes_written = 0;


  if(CommPortOpen) {
    WaitForSingleObject(MutexHandle, INFINITE);
    WriteFile(CommsHandle, Data, Len, &bytes_written, NULL);
    ReleaseMutex(MutexHandle);
  }
  return(bytes_written);
}
//---------------------------------------------------------------------------

/******************************************************************************/
/*Function Name:            Read                                              */
/*                                                                            */
/*Function Description:     Read rx dat                                       */
/*                                                                            */
/*Return Value Descript:    unsigned long - number of bytes read              */
/*                                                                            */
/*Function Para. Descript:  unsigned char* - rx data buffer                   */
/*                          unsigned int - rx data size                       */
/*                                                                            */
/*Author :                  Wynand Erasmus                                    */
/*                                                                            */
/*Comments:                                                                   */
/*                                                                            */
/******************************************************************************/
unsigned long CCommunication::Read(unsigned char *RxBuf, unsigned int Len)
{
  unsigned long bytes_read = 0;


  if(CommPortOpen) {
    WaitForSingleObject(MutexHandle, INFINITE);
    ReadFile(CommsHandle, RxBuf, Len, &bytes_read, NULL);
    ReleaseMutex(MutexHandle);
  }

  return(bytes_read);
}
//---------------------------------------------------------------------------

/******************************************************************************/
/*Function Name:            IsOpen                                            */
/*                                                                            */
/*Function Description:     Get comms status                                  */
/*                                                                            */
/*Return Value Descript:    None                                              */
/*                                                                            */
/*Function Para. Descript:  None                                              */
/*                                                                            */
/*Author :                  Wynand Erasmus                                    */
/*                                                                            */
/*Comments:                                                                   */
/*                                                                            */
/******************************************************************************/
bool CCommunication::IsOpen(void)
{
  return(CommPortOpen);
}
//---------------------------------------------------------------------------


