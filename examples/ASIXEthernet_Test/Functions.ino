#define HACKED //Limits number of times usbthread loops per second

void usbthread() {
  uint32_t cc = 0;
  elapsedMillis checkTimer;
  while(1) {
    myusb.Task();
    asix1.read();
    if(fnet_netif_is_initialized(current_netif)){
      fnet_poll();
      fnet_service_poll();
      if(checkTimer >= 1000) {
        checkTimer -= 1000;
        checkLink();
      }
    }
    else{
      checkTimer = 0;
    }
    
    #ifdef STATS
    LoopedUSB++;
    #endif
    #ifdef HACKED
    cc++;
    if ( cc > 20 ) {
      cc=0;
      threads.yield();
    }
    #endif
  }
}

void dhcp_cln_callback_updated(fnet_dhcp_cln_desc_t _dhcp_desc, fnet_netif_desc_t netif, void *p ) { //Called when DHCP updates
  struct fnet_dhcp_cln_options current_options;
  fnet_dhcp_cln_get_options(dhcp_desc, &current_options);
  
  uint8_t *ip = (uint8_t*)&current_options.ip_address.s_addr;
  Serial.print("IPAddress: ");
  Serial.print((uint8_t)*ip++);
  Serial.print(".");
  Serial.print((uint8_t)*ip++);
  Serial.print(".");
  Serial.print((uint8_t)*ip++);
  Serial.print(".");
  Serial.println((uint8_t)*ip);

  ip = (uint8_t*)&current_options.netmask.s_addr;
  Serial.print("SubnetMask: ");
  Serial.print((uint8_t)*ip++);
  Serial.print(".");
  Serial.print((uint8_t)*ip++);
  Serial.print(".");
  Serial.print((uint8_t)*ip++);
  Serial.print(".");
  Serial.println((uint8_t)*ip);

  ip = (uint8_t*)&current_options.gateway.s_addr;
  Serial.print("Gateway: ");
  Serial.print((uint8_t)*ip++);
  Serial.print(".");
  Serial.print((uint8_t)*ip++);
  Serial.print(".");
  Serial.print((uint8_t)*ip++);
  Serial.print(".");
  Serial.println((uint8_t)*ip);

  ip = (uint8_t*)&current_options.dhcp_server.s_addr;
  Serial.print("DHCPServer: ");
  Serial.print((uint8_t)*ip++);
  Serial.print(".");
  Serial.print((uint8_t)*ip++);
  Serial.print(".");
  Serial.print((uint8_t)*ip++);
  Serial.print(".");
  Serial.println((uint8_t)*ip);

  
  Serial.print("State: ");
  Serial.println(fnet_dhcp_cln_get_state(_dhcp_desc));
  Serial.println();
  Serial.println();

  

}

void checkLink(){
//  Serial.print("Link: ");
//  Serial.println(asix1.connected);
  if(asix1.connected && fnet_dhcp_cln_is_enabled(dhcp_desc)){
    //Serial.println("DHCP already running!");
  }
  else if(asix1.connected){
    Serial.println("Initializing DHCP");
          fnet_memset_zero(&dhcp_desc, sizeof(dhcp_desc));
          fnet_memset_zero(&dhcp_params, sizeof(dhcp_params));
          dhcp_params.netif = current_netif;
          // Enable DHCP client.
          if((dhcp_desc = fnet_dhcp_cln_init(&dhcp_params))){
              /*Register DHCP event handler callbacks.*/
             fnet_dhcp_cln_set_callback_updated(dhcp_desc, dhcp_cln_callback_updated, (void*)dhcp_desc);
             fnet_dhcp_cln_set_callback_discover(dhcp_desc, dhcp_cln_callback_updated, (void*)dhcp_desc);
             Serial.println("DHCP initialization done!");
          }
          else{
            Serial.println("ERROR: DHCP initialization failed!");
          }
  }
  else if(!asix1.connected && fnet_dhcp_cln_is_enabled(dhcp_desc)){
    Serial.println("DHCP Released");
    fnet_dhcp_cln_release(dhcp_desc);
    fnet_memset_zero(dhcp_desc, sizeof(dhcp_desc));
  }
  else if(!asix1.connected){
//    Serial.println("DHCP already released!");
  }
}

void handleOutput(fnet_netif_t *netif, fnet_netbuf_t *nb) { //Called when a message is sent
  if(nb && (nb->total_length >= FNET_ETH_HDR_SIZE)){
    uint8_t* p = (uint8_t*)sbuf;
    _fnet_netbuf_to_buf(nb, 0u, FNET_NETBUF_COPYALL, p);

//    if(nb->total_length >= 12){
//      Serial.print("Message Transmitted: ");
//      Serial.println(nb->total_length);
//      const uint8_t* end = p + nb->total_length;
//      while(p < end){
//        if(*p <= 0x0F) Serial.print("0");
//        Serial.print(*p, HEX);
//        Serial.print(" ");
//        p++;
//      }
//      Serial.println();
//    }
//    p = (uint8_t*)sbuf;
    asix1.sendPacket(p, nb->total_length);
  }
}

int32_t _totalLength;
uint8_t* _lastIndex;
uint8_t* _rxEnd;
void handleRecieve(const uint8_t* data, uint16_t length) { //Called when ASIX gets a message
  if(length == 0) return;
  if(((data[0] + data[2]) == 0xFF) && ((data[1] + data[3]) == 0xFF)) { //Check for header
    _lastIndex = (uint8_t*)rbuf;
    const uint8_t* end = data + length;
    _totalLength = (data[1] << 8) | data[0];
    _rxEnd = (uint8_t*)rbuf + _totalLength;
    data += 6;
    while(data < end){
      *_lastIndex = *data;
      data++;
      _lastIndex++;
    }
  }
  else if(_lastIndex <= _rxEnd){
    const uint8_t* end = data + length;
    while(data < end){
      *_lastIndex = *data;
      data++;
      _lastIndex++;
    }
  }

//  if(_lastIndex >= _rxEnd && _totalLength > 100) {
//    Serial.print("Length: ");
//    Serial.println(_totalLength);
//    Serial.print("Message Recieved: ");
//    for(uint16_t i = 0; i < _totalLength; i++){
//      if(rbuf[i] <= 0x0F) Serial.print("0");
//      Serial.print(rbuf[i], HEX);
//      Serial.print(" ");
//    }
//    Serial.println();
//  }
//  Serial.println();
//  Serial.println();
  if(_lastIndex >= _rxEnd) {
//    Serial.println("Stack recieved");
    _fnet_eth_input(&fnet_eth0_if, (uint8_t*)rbuf, _totalLength);
  }
}

void handleSetMACAddress(uint8_t * hw_addr) { //Gets calls on initialization
  Serial.print("SetMACAddress: ");
  for(uint8_t i = 0; i < 6; i++) {
    if(hw_addr[i] <= 0x0F) Serial.print("0");
    Serial.print(hw_addr[i], HEX);
  }
  Serial.println();
}

void handleGetMACAddress(fnet_mac_addr_t * hw_addr) { //Gets called everytime a message is sent
  (*hw_addr)[0] = asix1.nodeID[0];
  (*hw_addr)[1] = asix1.nodeID[1];
  (*hw_addr)[2] = asix1.nodeID[2];
  (*hw_addr)[3] = asix1.nodeID[3];
  (*hw_addr)[4] = asix1.nodeID[4];
  (*hw_addr)[5] = asix1.nodeID[5];
//  Serial.print("GetMACAddress: ");
//  for(uint8_t i = 0; i < 6; i++) {
//    if(hw_addr[i] <= 0x0F) Serial.print("0");
//    Serial.print(hw_addr[i], HEX);
//  }
//  Serial.println();
}

void handlePHYRead(fnet_uint32_t reg_addr, fnet_uint16_t *data) { //Could be called, don't think it works correctly
  asix1.readPHY(reg_addr, data);
  Serial.print("PHYRead: ");
  Serial.print(reg_addr, HEX);
  Serial.print("  ");
  Serial.println(*data, HEX);
}

void handlePHYWrite(fnet_uint32_t reg_addr, fnet_uint16_t data) { //Could be called, might work correctly
  asix1.writePHY(reg_addr, data);
  Serial.println("PHYWrite");
}

void handleMulticastJoin(fnet_netif_t *netif, fnet_mac_addr_t multicast_addr) { //Called when joining multicast group
  //Not setup yet
  Serial.print("MulticastJoin: ");
  for(uint8_t i = 0; i < 6; i++) {
    if(multicast_addr[i] <= 0x0F) Serial.print("0");
    Serial.print(multicast_addr[i], HEX);
  }
  Serial.println();
}

void handleMulticastLeave(fnet_netif_t *netif, fnet_mac_addr_t multicast_addr) { //Called when leaving multicast group
  //Not setup yet
  Serial.println("MulticastLeave: ");
  for(uint8_t i = 0; i < 6; i++) {
    if(multicast_addr[i] <= 0x0F) Serial.print("0");
    Serial.print(multicast_addr[i], HEX);
  }
  Serial.println();
}

fnet_bool_t handleIsConnected() {
  return asix1.connected ? FNET_TRUE : FNET_FALSE;
}
