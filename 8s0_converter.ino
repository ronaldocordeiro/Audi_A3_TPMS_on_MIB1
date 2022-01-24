/*
 * TPMS CAN Bus filter
 * 
 * Filters messages sent by a 8S0907273 VAG TPMS ECU in order to make it compatible with
 * Audi A3 8V MIB1 Multimedia system
 * 
 * Coded by Ronaldo Cordeiro - cordeiroronaldo@hotmail.com
 * 
 * Uses MCP_CAN_lib-master - By Cory J. Fowler 
 * https://github.com/coryjfowler/MCP_CAN_lib
 */

#include <mcp_can.h>
#include <SPI.h>

long unsigned int msgId; // message ID
unsigned char msgLen=0,  // message length
              rxBuf[8],  // message buffer
              msgType;   // message type: 0=standard, 1=extended

const long unsigned int tpmsDataId = 0x17330710;

unsigned char msgBuf128[8]={0x80, 0x08, 0x31, 0xC3, 0x38, 0x03, 0xF1, 0x00},     // msg 0x80/0x90 (128/144) with modified byte 6
              msgBuf211[8]={0x31, 0xD3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};     // dummy msg xx 0xD3 (xx 211)

MCP_CAN ECU(9);    // Set ECU CAN interface CS to pin 9
MCP_CAN BUS(10);   // Set CANBus CAN interface CS to pin 10

void setup()
{
  Serial.begin(115200);
  
  ECU.begin(MCP_ANY,CAN_500KBPS,MCP_8MHZ); 
  BUS.begin(MCP_ANY,CAN_500KBPS,MCP_8MHZ);
  
  ECU.setMode(MCP_NORMAL);
  BUS.setMode(MCP_NORMAL);
}

void loop()
{
    if(CAN_MSGAVAIL == BUS.checkReceive())
    {
        BUS.readMsgBuf(&msgId, &msgLen, rxBuf);

        if((msgId & 0x80000000) == 0x80000000) {    // Determine if ID is standard (11 bits) or extended (29 bits)
            msgId = msgId & 0x1FFFFFFF;
            msgType = 1;
        }
        else msgType = 0;

        // Accept only: extended msgs IDs (BAP), TPMS ECU required msgs and diagnostic msgs (UDS)
        // 0xFD  (253)  - ESP_21
        // 0x30B (779)  - Kombi_01
        // 0x3C0 (960)  - Klemmen_Status_01
        // 0x585 (1413) - Systeminfo_01
        // 0x643 (1603) - Einheiten_01
        // 0x6B2 (1714) - Diagnose_01
        // 0x6b7 (1719) - Kombi_02
        // 0x700-0x7FF  - UDS diagnostics
        
        if (msgType==1 || msgId==0x3C0 || msgId==0x30B || msgId==0xFD || msgId==0x585 || msgId==0x643 || msgId==0x6B2 || msgId==0x6b7 || (msgId >= 0x700 && msgId <= 0x7FF)) {
            // Forward messages to ECU
            ECU.sendMsgBuf(msgId, msgType , msgLen, rxBuf);
        }
    }
 
    if(CAN_MSGAVAIL == ECU.checkReceive())
    {
        ECU.readMsgBuf(&msgId, &msgLen, rxBuf);

        if((msgId & 0x80000000) == 0x80000000) {    // Determine if ID is standard (11 bits) or extended (29 bits)
            msgId = msgId & 0x1FFFFFFF;
            msgType = 1;
        }
        else msgType = 0;

        if(msgId==tpmsDataId && (rxBuf[0]==0x80 || rxBuf[0]==0x90)) {        // Replace messages 128/144
                 msgBuf128[0]=rxBuf[0];
                 BUS.sendMsgBuf(msgId, msgType, 8, msgBuf128);
             }
        else if(msgId==tpmsDataId && rxBuf[1]==209) {                        // xx 209: pressure data
                 for(int i=3; i<=6; i++) if (rxBuf[i]==0xFF) rxBuf[i]=0;     // Replace absent pressure values (0xFF) with zero
                 BUS.sendMsgBuf(msgId, msgType , msgLen, rxBuf);
             }
        else if(msgId==tpmsDataId && rxBuf[1]==210) {                        // xx 210: temperature data
                 for(int i=3; i<=6; i++) if (rxBuf[i]==0x00) {               // Replace absent temperature values (0x00) with values displayed as zero degrees
                     if (rxBuf[2]==0) rxBuf[i]=60; else rxBuf[i]=38;         // (decimal 60 if Celsius; decimal 38 if Fahrenheit)
                     }
                 BUS.sendMsgBuf(msgId, msgType , msgLen, rxBuf);

                 msgBuf211[0]=rxBuf[0];                                      // Send xx 211 after xx 210
                 BUS.sendMsgBuf(msgId, msgType, 8, msgBuf211);
             }
        else BUS.sendMsgBuf(msgId, msgType, msgLen, rxBuf);                  // Forward all other messages
    }
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
