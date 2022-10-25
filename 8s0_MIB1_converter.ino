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

unsigned char msgBuf_0xC5[6]  ={0xC5, 0x20, 0x20, 0x20, 0x20, 0x20},               // msg 0xC5/0xD5
              msgBuf_xx_211[8]={0x31, 0xD3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   // msg 0x31/0x41 0xD3 (xx 211) - specified pressures
              last0x585Msg[8] ={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};   // last Systeminfo_01 message received

int no585count=0;

MCP_CAN ECU(9);    // Set ECU CAN interface CS to pin 9
MCP_CAN BUS(10);   // Set CANBus CAN interface CS to pin 10

void setup()
{
  BUS.begin(MCP_STDEXT,CAN_500KBPS,MCP_8MHZ);   // Use MCP2515 mask/filter capability to filter out unnecessary messages
  BUS.init_Mask(0,1,0x13000000);                // Set mask 0
  BUS.init_Filt(0,1,0x13000000);                // Allow extended ID (BAP) messages including 0x17XXXXXX and 0x1BXXXXXX
  BUS.init_Filt(1,0,0x00FD0000);                // Allow only ID 0xFD (ESP_21) message
  BUS.init_Mask(1,0,0x07000000);                // Set mask 1 to filter msg IDs from 0x100 to 0x7FF
  BUS.init_Filt(2,0,0x03000000);                // Allow 0x3XX
  BUS.init_Filt(3,0,0x05000000);                // Allow 0x5XX
  BUS.init_Filt(4,0,0x06000000);                // Allow 0x6XX
  BUS.init_Filt(5,0,0x07000000);                // Allow 0x6XX
  BUS.setMode(MCP_NORMAL);

  ECU.begin(MCP_ANY,CAN_500KBPS,MCP_8MHZ);
  ECU.setMode(MCP_NORMAL);
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

            if (msgId==0x585) {                                    // save last Systeminfo_01 message received
                for(int i=0; i<8; i++) last0x585Msg[i]=rxBuf[i];
                no585count=0;
            }

            if (msgId==0x3C0) {                                    // repeat last Systeminfo_01 message if none is received after 10 Klemmen_Status_01 messages
                no585count++;
                if (no585count>=10) {
                    ECU.sendMsgBuf(0x585, 0, 8, last0x585Msg);
                    no585count=0;
               }
            }
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

        if(msgId==tpmsDataId && (rxBuf[0]==0x80 || rxBuf[0]==0x90)) {        // Modify messages 80/90 according to byte 3 contents
                 if(rxBuf[3]==193) rxBuf[1]=44;                              // "long message" - set byte 1 (data length) to 44 bytes
                 if(rxBuf[3]==195) rxBuf[6]=0xF1;                            // "short message" - set byte 6 (flags) to modified version
                 BUS.sendMsgBuf(msgId, msgType , msgLen, rxBuf);
             }
        else if(msgId==tpmsDataId && (rxBuf[0]==0xC0 || rxBuf[0]==0xD0)) {   // Modify messages C0/D0
                 if(msgLen!=5) rxBuf[7]=0xF1;                                // "long message" - set byte 7 (flags) to modified version
                 BUS.sendMsgBuf(msgId, msgType , msgLen, rxBuf);
             }
        else if(msgId==tpmsDataId && (rxBuf[0]==0xC4 || rxBuf[0]==0xD4)) {   // Extend message C4/D4 to 8 bytes long and add message C5/D5
                 rxBuf[7]=0;
                 msgLen=8;
                 BUS.sendMsgBuf(msgId, msgType , msgLen, rxBuf);
                 msgBuf_0xC5[0]=rxBuf[0]+1;                                  // Sends C5 after C4 (or D5 after D4)
                 BUS.sendMsgBuf(msgId, msgType , 6, msgBuf_0xC5);
             }
        else if(msgId==tpmsDataId && rxBuf[1]==209) {                        // xx 209: tire pressure data
                 for(int i=3; i<=6; i++) if (rxBuf[i]==0xFF) rxBuf[i]=0;     // Replace absent pressure values (0xFF) with zero
                 BUS.sendMsgBuf(msgId, msgType , msgLen, rxBuf);
             }
        else if(msgId==tpmsDataId && rxBuf[1]==210) {                        // xx 210: tire temperature data
                 for(int i=3; i<=6; i++) if (rxBuf[i]==0x00) {               // Replace absent temperature values (0x00) with values displayed as zero degrees
                     if (rxBuf[2]==0) rxBuf[i]=60; else rxBuf[i]=38;         // (decimal 60 if Celsius; decimal 38 if Fahrenheit)
                     }
                 BUS.sendMsgBuf(msgId, msgType , msgLen, rxBuf);

                 msgBuf_xx_211[0]=rxBuf[0];                                  // Send xx 211 after xx 210
                 BUS.sendMsgBuf(msgId, msgType, 8, msgBuf_xx_211);
             }
        else BUS.sendMsgBuf(msgId, msgType, msgLen, rxBuf);                  // Forward all other messages unmodified
    }
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
