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
#ifndef RFM69_SessionKey_h
#define RFM69_SessionKey_h
#include <RFM69.h>

class RFM69_SessionKey: public RFM69 {
  public:
    static volatile uint8_t SESSION_KEY_INCLUDED; // flag in CTL byte indicating this packet includes a session key
    static volatile uint8_t SESSION_KEY_REQUESTED; // flag in CTL byte indicating this packet is a request for a session key
    static volatile uint8_t SESSION_KEY; // set to the session key for a particular transmission
    static volatile uint8_t INCOMING_SESSION_KEY; // set on an incoming packet and used to decide if receiveDone should be true and data should be processed
	
    RFM69_SessionKey(uint8_t slaveSelectPin=RF69_SPI_CS, uint8_t interruptPin=RF69_IRQ_PIN, bool isRFM69HW=false, uint8_t interruptNum=RF69_IRQ_NUM) :
      RFM69(slaveSelectPin, interruptPin, isRFM69HW, interruptNum) {
    }
    
    bool initialize(uint8_t freqBand, uint8_t ID, uint8_t networkID=1); // need to call initialize because _sessionKeyEnabled (new variable) needs to be set as false when first loaded
    
    void send(uint8_t toAddress, const void* buffer, uint8_t bufferSize, bool requestACK=false); // some additions needed
    void sendACK(const void* buffer = "", uint8_t bufferSize=0); // some additions needed
    bool receiveDone(); // some additions needed
    
    // new public functions for session handling
    void useSessionKey(bool enabled);
    bool sessionKeyEnabled();

  protected:
    void interruptHook(uint8_t CTLbyte);
    void sendFrame(byte toAddress, const void* buffer, byte size, bool requestACK=false, bool sendACK=false);  // Need this one to match the RFM69 library
    void sendFrame(uint8_t toAddress, const void* buffer, uint8_t size, bool requestACK, bool sendACK, bool sessionRequested, bool sessionIncluded); // two parameters added for session key support
    void sendWithSession(uint8_t toAddress, const void* buffer, uint8_t bufferSize, bool requestACK=false, uint8_t retryWaitTime=40); // new function to transparently handle session without sketch needing to change
    void receiveBegin(); // some additions needed
    bool _sessionKeyEnabled; // protected variable to indicate if session key support is enabled
};

#endif