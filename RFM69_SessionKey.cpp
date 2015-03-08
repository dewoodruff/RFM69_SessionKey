// **********************************************************************************
// Session key class derived from RFM69 library. Session key prevents replay of wireless transmissions.
// **********************************************************************************
// Copyright Dan Woodruff
// **********************************************************************************
// License
// **********************************************************************************
// This program is free software; you can redistribute it 
// and/or modify it under the terms of the GNU General    
// Public License as published by the Free Software       
// Foundation; either version 3 of the License, or        
// (at your option) any later version.                    
//                                                        
// This program is distributed in the hope that it will   
// be useful, but WITHOUT ANY WARRANTY; without even the  
// implied warranty of MERCHANTABILITY or FITNESS FOR A   
// PARTICULAR PURPOSE. See the GNU General Public        
// License for more details.                              
//                                                        
// You should have received a copy of the GNU General    
// Public License along with this program.
// If not, see <http://www.gnu.org/licenses/>.
//                                                        
// Licence can be viewed at                               
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// **********************************************************************************
#include <RFM69_SessionKey.h>
#include <RFM69.h>   // include the RFM69 library files as well
#include <RFM69registers.h>
#include <SPI.h>

volatile uint8_t RFM69_SessionKey::SESSION_KEY_INCLUDED; // flag in CTL byte indicating this packet includes a session key
volatile uint8_t RFM69_SessionKey::SESSION_KEY_REQUESTED; // flag in CTL byte indicating this packet is a request for a session key
volatile uint8_t RFM69_SessionKey::SESSION_KEY; // set to the session key for a particular transmission
volatile uint8_t RFM69_SessionKey::INCOMING_SESSION_KEY; // set on an incoming packet and used to decide if receiveDone should be true and data should be processed

//=============================================================================
// initialize() - Some extra initialization before calling base class
//=============================================================================
bool RFM69_SessionKey::initialize(uint8_t freqBand, uint8_t nodeID, uint8_t networkID)
{
  _sessionKeyEnabled = false; // default to disabled
  SESSION_KEY_INCLUDED = 0;
  SESSION_KEY_REQUESTED = 0;
  return RFM69::initialize(freqBand, nodeID, networkID);  // use base class to initialize everything else
}

//=============================================================================
// send() - Modified to either send the raw frame or send with session key 
//          if session key is enabled
//=============================================================================
void RFM69_SessionKey::send(uint8_t toAddress, const void* buffer, uint8_t bufferSize, bool requestACK)
{
  writeReg(REG_PACKETCONFIG2, (readReg(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART); // avoid RX deadlocks
  uint32_t now = millis();
  while (!canSend() && millis() - now < RF69_CSMA_LIMIT_MS) receiveDone();
  if (sessionKeyEnabled())
    sendWithSession(toAddress, buffer, bufferSize, requestACK);
  else
    sendFrame(toAddress, buffer, bufferSize, requestACK, false);
}

//=============================================================================
// sendWithSession() - Function to do the heavy lifting of session handling so it is transparent to sketch
//=============================================================================
void RFM69_SessionKey::sendWithSession(uint8_t toAddress, const void* buffer, uint8_t bufferSize, bool requestACK, uint8_t retryWaitTime) {
  // reset session key to blank value to start
  SESSION_KEY = 0;
  // start the session by requesting a key. don't request an ACK - ACKs are handled at the whole session level
  //Serial.println("sendWithSession: Requesting session key.");
  sendFrame(toAddress, null, 0, false, false, true, false);
  receiveBegin();
  // loop until session key received, or timeout
  uint32_t sentTime = millis();
  while (millis() - sentTime < retryWaitTime && SESSION_KEY == 0);
  if (SESSION_KEY == 0) 
    return;
  //Serial.print("sendWithSession: Received key: ");
  //Serial.println(SESSION_KEY);
  // finally send the data! request the ACK if needed
  sendFrame(toAddress, buffer, bufferSize, requestACK, false, false, true);
}

//=============================================================================
// sendAck() - Updated to call new sendFrame with additional parameters.
//             Should be called immediately after reception in case sender wants ACK
//=============================================================================
void RFM69_SessionKey::sendACK(const void* buffer, uint8_t bufferSize) {
  uint8_t sender = SENDERID;
  int16_t _RSSI = RSSI; // save payload received RSSI value
  writeReg(REG_PACKETCONFIG2, (readReg(REG_PACKETCONFIG2) & 0xFB) | RF_PACKET2_RXRESTART); // avoid RX deadlocks
  uint32_t now = millis();
  while (!canSend() && millis() - now < RF69_CSMA_LIMIT_MS) receiveDone();
  // if session keying is enabled, call sendFrame to include the session key
  // otherwise send as the built in library would
  if (sessionKeyEnabled())
    sendFrame(sender, buffer, bufferSize, false, true, false, true);
  else
    sendFrame(sender, buffer, bufferSize, false, true);
  RSSI = _RSSI; // restore payload RSSI
}
    
//=============================================================================
// sendFrame() - The basic version is used to match the RFM69 library so it can be extended
//               This sendFrame is generally called by the internal RFM69 functions
//               Simply transfer to the modified version with additional parameters
//=============================================================================
void RFM69_SessionKey::sendFrame(uint8_t toAddress, const void* buffer, uint8_t bufferSize, bool requestACK, bool sendACK)
{
  sendFrame(toAddress, buffer, bufferSize, requestACK, sendACK, false, false);
}

//=============================================================================
// sendFrame() - New with additional parameters. Handles the CTLbyte bits needed for session key
//=============================================================================
void RFM69_SessionKey::sendFrame(uint8_t toAddress, const void* buffer, uint8_t bufferSize, bool requestACK, bool sendACK, bool sessionRequested, bool sessionIncluded)
 {
  setMode(RF69_MODE_STANDBY); // turn off receiver to prevent reception while filling fifo
  while ((readReg(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY) == 0x00); // wait for ModeReady
  writeReg(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_00); // DIO0 is "Packet Sent"
  if (bufferSize > RF69_MAX_DATA_LEN) bufferSize = RF69_MAX_DATA_LEN;
  
  // start with blank control byte
  uint8_t CTLbyte = 0x00;
  // layer on the bits to the CTLbyte as needed
  if (sendACK)
    CTLbyte = CTLbyte | RFM69_CTL_SENDACK;
  if (requestACK)
    CTLbyte = CTLbyte | RFM69_CTL_REQACK;
  if (sessionRequested){
    CTLbyte = CTLbyte | RFM69_CTL_EXT1;
    //Serial.println("Sendframe: Session requested");
  }
  if (sessionIncluded){
    CTLbyte = CTLbyte | RFM69_CTL_EXT2;
    //Serial.println("Sendframe: Session included");
  }
    // write to FIFO
  select();
  SPI.transfer(REG_FIFO | 0x80);
  if (sessionIncluded)
    SPI.transfer(bufferSize + 4);
  else
    SPI.transfer(bufferSize + 3);
  SPI.transfer(toAddress);
  SPI.transfer(_address);
  SPI.transfer(CTLbyte);
  if (sessionIncluded)
    SPI.transfer(SESSION_KEY);
 
  for (uint8_t i = 0; i < bufferSize; i++)
    SPI.transfer(((uint8_t*) buffer)[i]);
  unselect();
  
  // no need to wait for transmit mode to be ready since its handled by the radio
  setMode(RF69_MODE_TX);
  uint32_t txStart = millis();
  while (digitalRead(_interruptPin) == 0 && millis() - txStart < RF69_TX_LIMIT_MS); // wait for DIO0 to turn HIGH signalling transmission finish
  //while (readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PACKETSENT == 0x00); // wait for ModeReady
  setMode(RF69_MODE_STANDBY);
 }

//=============================================================================
// interruptHook() - Gets called by the base class interruptHandler right after the header is fetched
//=============================================================================
void RFM69_SessionKey::interruptHook(uint8_t CTLbyte) {
  SESSION_KEY_REQUESTED = CTLbyte & RFM69_CTL_EXT1; // extract session key request flag
  SESSION_KEY_INCLUDED = CTLbyte & RFM69_CTL_EXT2; //extract session key included flag
  // if a new session key was requested, send it right here in the interrupt to avoid having to handle it in sketch manually, and for greater speed
  if (sessionKeyEnabled() && SESSION_KEY_REQUESTED && !SESSION_KEY_INCLUDED) {
    unselect();
    //Serial.println("SESSION_KEY_REQUESTED && !SESSION_KEY_INCLUDED");
    setMode(RF69_MODE_STANDBY);
    // generate and set new random key
    SESSION_KEY = random(256);
    // send it!
    sendFrame(SENDERID, null, 0, false, false, true, true);
    // don't process any data
	DATALEN = 0;
    return;
  }
  // if both session key bits are set, the incoming packet has a new session key
  // set the session key and do not process data
  if (sessionKeyEnabled() && SESSION_KEY_REQUESTED && SESSION_KEY_INCLUDED) {
    SESSION_KEY = SPI.transfer(0);
    // don't process any data
	DATALEN = 0;
    return;
  }
  // if a session key is included, make sure it is the key we expect
  // if the key does not match, do not set DATA and return false
  if (sessionKeyEnabled() && SESSION_KEY_INCLUDED && !SESSION_KEY_REQUESTED) {
    INCOMING_SESSION_KEY = SPI.transfer(0);
    if (INCOMING_SESSION_KEY != SESSION_KEY){
      // don't process any data
	  DATALEN = 0;
      return;
    }
    // if keys do match, actual data is payload - 4 instead of 3 to account for key
    DATALEN = PAYLOADLEN - 4;
    return;
  }
}

//=============================================================================
//  receiveBegin() - Need to clear out session flags before calling base class function
//=============================================================================
void RFM69_SessionKey::receiveBegin() {
  SESSION_KEY_INCLUDED = 0;
  SESSION_KEY_REQUESTED = 0;
  RFM69::receiveBegin();
}

//=============================================================================
//  receiveDone() - Added check so if session key is enabled, incoming key is checked 
//                  against the stored key for this session. Returns false if not matched
//=============================================================================
 bool RFM69_SessionKey::receiveDone() {
//ATOMIC_BLOCK(ATOMIC_FORCEON)
//{
  noInterrupts(); // re-enabled in unselect() via setMode() or via receiveBegin()
  if (_mode == RF69_MODE_RX && PAYLOADLEN > 0)
  {
    // if session key on and keys don't match
    // return false, as if nothing was even received
    if (sessionKeyEnabled() && INCOMING_SESSION_KEY != SESSION_KEY) {
      interrupts(); // explicitly re-enable interrupts
      receiveBegin();
      return false;
    }
    setMode(RF69_MODE_STANDBY); // enables interrupts
    return true;
  }
  else if (_mode == RF69_MODE_RX) // already in RX no payload yet
  {
    interrupts(); // explicitly re-enable interrupts
    return false;
  }
  receiveBegin();
  return false;
}

//=============================================================================
//  useSessionKey() - Enables session key support for transmissions
//=============================================================================
void RFM69_SessionKey::useSessionKey(bool onOff) {
  _sessionKeyEnabled = onOff;
}

//=============================================================================
//  useSessionKey() - Check if session key support is enabled
//=============================================================================
bool RFM69_SessionKey::sessionKeyEnabled() {
  return _sessionKeyEnabled;
}