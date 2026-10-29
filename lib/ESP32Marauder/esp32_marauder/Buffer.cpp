/** @file Buffer.cpp @brief Packet and text buffer implementation for Marauder. */

#include "Buffer.h"
#include "lang_var.h"

Buffer::Buffer(){
  bufA = nullptr;
  bufB = nullptr;
  // Allocate capture buffers only when Marauder starts. This object is global
  // and exists during normal boot even when Marauder is idle.
  bufSizeA = 0;
  bufSizeB = 0;
  packetsCounter = 0;
  writing = false;
  useA = true;
  saving = false;
  fs = nullptr;
  serial = false;
}

Buffer::~Buffer(){
  std::lock_guard<std::recursive_mutex> lock(bufferMutex);
  if (file) file.close();
  free(bufA);
  free(bufB);
}

void Buffer::reset(){
  std::lock_guard<std::recursive_mutex> lock(bufferMutex);
  if (file) file.close();
  free(bufA);
  free(bufB);
  bufA = (uint8_t*)malloc(BUF_SIZE);
  bufB = (uint8_t*)malloc(BUF_SIZE);
  bufSizeA = 0;
  bufSizeB = 0;
  if (bufA == nullptr || bufB == nullptr) {
    // Do not keep one half of a failed reset allocation alive. A later
    // capture reset must not slowly leak heap after low-memory failures.
    free(bufA);
    free(bufB);
    bufA = nullptr;
    bufB = nullptr;
    writing = false;
  } else {
    writing = true;
  }
  packetsCounter = 0;
  useA = true;
  saving = false;
  fs = nullptr;
  serial = false;
}

bool Buffer::createFile(String name, bool is_pcap){
  constexpr uint32_t MAX_FILE_ATTEMPTS = 10000;
  uint32_t i = 0;
  bool found = false;
  if (fs) fs->mkdir("/Captures");
  if (is_pcap) {
    for (; i < MAX_FILE_ATTEMPTS; ++i) {
      fileName = (name.startsWith("/") ? name : "/" + name)+(String)"_"+(String)i+".pcap";
      if (!fs->exists(fileName)) { found = true; break; }
    }
  }
  else {
    for (; i < MAX_FILE_ATTEMPTS; ++i) {
      fileName = (name.startsWith("/") ? name : "/" + name)+(String)"_"+(String)i+".log";
      if (!fs->exists(fileName)) { found = true; break; }
    }
  }

  if (!found) return false;

  esp32m_println(fileName);
  
  file = fs->open(fileName, FILE_WRITE);
  if (!file) return false;
  file.close();
  return true;
}

void Buffer::open(bool is_pcap){
  bufSizeA = 0;
  bufSizeB = 0;

  bufSizeB = 0;

  writing = true;

  if (is_pcap) {
    write(uint32_t(0xa1b2c3d4)); // magic number
    write(uint16_t(2)); // major version number
    write(uint16_t(4)); // minor version number
    write(int32_t(0)); // GMT to local correction
    write(uint32_t(0)); // accuracy of timestamps
    write(uint32_t(SNAP_LEN)); // max length of captured packets, in octets
    write(uint32_t(105)); // data link type
  }
}

void Buffer::openFile(String file_name, fs::FS* fs, bool serial, bool is_pcap) {
  close();
  bool save_pcap = settings_obj.loadSetting<bool>("SavePCAP");
  if (!save_pcap) {
    this->fs = NULL;
    this->serial = false;
    writing = false;
    return;
  }
  this->fs = fs;
  this->serial = serial;
  if (this->fs) {
    if (!createFile(file_name, is_pcap)) {
      esp32m_println("Could not create capture file");
      this->fs = nullptr;
    }
  }
  if (this->fs || this->serial) {
    open(is_pcap);
  } else {
    writing = false;
  }
}

void Buffer::pcapOpen(String file_name, fs::FS* fs, bool serial) {
  openFile(file_name, fs, serial, true);
}

void Buffer::logOpen(String file_name, fs::FS* fs, bool serial) {
  openFile(file_name, fs, serial, false);
}

void Buffer::add(const uint8_t* buf, uint32_t len, bool is_pcap){
  std::lock_guard<std::recursive_mutex> lock(bufferMutex);
  if (!writing || buf == nullptr || len == 0 || bufA == nullptr || bufB == nullptr) return;
  if (is_pcap && len > SNAP_LEN) return;
  const uint32_t overhead = is_pcap ? 16U : 0U;
  if (len > BUF_SIZE || overhead > BUF_SIZE - len) return;
  const uint32_t required = overhead + len;
  
  if (useA && bufSizeA > BUF_SIZE - required && bufSizeB == 0) {
    useA = false;
  } else if (!useA && bufSizeB > BUF_SIZE - required && bufSizeA == 0) {
    useA = true;
  }

  const uint32_t currentSize = useA ? bufSizeA : bufSizeB;
  if (currentSize > BUF_SIZE - required) return;

  uint32_t microSeconds = micros(); // e.g. 45200400 => 45s 200ms 400us
  uint32_t seconds = (microSeconds/1000)/1000; // e.g. 45200400/1000/1000 = 45200 / 1000 = 45s

  microSeconds -= seconds*1000*1000; // e.g. 45200400 - 45*1000*1000 = 45200400 - 45000000 = 400us (because we only need the offset)
  
  if (is_pcap) {
    write(seconds); // ts_sec
    write(microSeconds); // ts_usec
    write(len); // incl_len
    write(len); // orig_len
  }
  
  write(buf, len); // packet payload
  packetsCounter++;
}

void Buffer::append(wifi_promiscuous_pkt_t *packet, int len) {
  if (packet == nullptr || len <= 0) return;
  bool save_packet = settings_obj.loadSetting<bool>(text_table4[7]);
  if (save_packet) {
    add(packet->payload, len, true);
  }
}

void Buffer::append(String log) {
  bool save_packet = settings_obj.loadSetting<bool>(text_table4[7]);
  if (save_packet) {
    add((const uint8_t*)log.c_str(), log.length(), false);
  }
}

void Buffer::write(int32_t n){
  uint8_t buf[4];
  buf[0] = n;
  buf[1] = n >> 8;
  buf[2] = n >> 16;
  buf[3] = n >> 24;
  write(buf,4);
}

void Buffer::write(uint32_t n){
  uint8_t buf[4];
  buf[0] = n;
  buf[1] = n >> 8;
  buf[2] = n >> 16;
  buf[3] = n >> 24;
  write(buf,4);
}

void Buffer::write(uint16_t n){
  uint8_t buf[2];
  buf[0] = n;
  buf[1] = n >> 8;
  write(buf,2);
}

void Buffer::write(const uint8_t* buf, uint32_t len){
  if(!writing) return;
  if (buf == nullptr || len == 0) return;
  
  if(useA){
    if (len > BUF_SIZE - bufSizeA) return;
    memcpy(&bufA[bufSizeA], buf, len);
    bufSizeA += len;
  }else{
    if (len > BUF_SIZE - bufSizeB) return;
    memcpy(&bufB[bufSizeB], buf, len);
    bufSizeB += len;
  }
}

void Buffer::saveFs(){
  file = fs->open(fileName, FILE_APPEND);
  if (!file) {
    esp32m_println(text02+fileName+"'");
    return;
  }

  if(useA){
    if(bufSizeB > 0){
      if (file.write(bufB, bufSizeB) != bufSizeB) writing = false;
    }
    if(bufSizeA > 0){
      if (file.write(bufA, bufSizeA) != bufSizeA) writing = false;
    }
  } else {
    if(bufSizeA > 0){
      if (file.write(bufA, bufSizeA) != bufSizeA) writing = false;
    }
    if(bufSizeB > 0){
      if (file.write(bufB, bufSizeB) != bufSizeB) writing = false;
    }
  }

  file.close();
}

void Buffer::saveSerial() {
  // Saves to main console UART, user-facing app will ignore these markers
  // Uses / and ] in markers as they are illegal characters for SSIDs
  const char* mark_begin = "[BUF/BEGIN]";
  const size_t mark_begin_len = strlen(mark_begin);
  const char* mark_close = "[BUF/CLOSE]";
  const size_t mark_close_len = strlen(mark_close);

  // Additional buffer and memcpy's so that a single Serial.write() is called
  // This is necessary so that other console output isn't mixed into buffer stream
  const size_t frameSize = mark_begin_len + bufSizeA + bufSizeB + mark_close_len;
  uint8_t* buf = (uint8_t*)malloc(frameSize);
  if (buf == nullptr) {
    esp32m_println("Could not allocate Marauder serial buffer");
    return;
  }

  // Native USB Serial::write() retries forever while a connected host stops
  // reading. Drop this complete frame when the transport cannot accept it now,
  // keeping the cooperative Arduino loop alive and preserving frame boundaries.
  if (!Serial || Serial.availableForWrite() < static_cast<int>(frameSize)) {
    free(buf);
    return;
  }

  uint8_t* it = buf;
  memcpy(it, mark_begin, mark_begin_len);
  it += mark_begin_len;

  if(useA){
    if(bufSizeB > 0){
      memcpy(it, bufB, bufSizeB);
      it += bufSizeB;
    }
    if(bufSizeA > 0){
      memcpy(it, bufA, bufSizeA);
      it += bufSizeA;
    }
  } else {
    if(bufSizeA > 0){
      memcpy(it, bufA, bufSizeA);
      it += bufSizeA;
    }
    if(bufSizeB > 0){
      memcpy(it, bufB, bufSizeB);
      it += bufSizeB;
    }
  }

  memcpy(it, mark_close, mark_close_len);
  it += mark_close_len;
  Serial.write(buf, it - buf);
  free(buf);
}

void Buffer::save() {
  std::lock_guard<std::recursive_mutex> lock(bufferMutex);
  saving = true;

  if((bufSizeA + bufSizeB) == 0){
    saving = false;
    return;
  }

  if(this->fs) saveFs();
  if(this->serial) saveSerial();

  bufSizeA = 0;
  bufSizeB = 0;

  saving = false;
}

void Buffer::close() {
  std::lock_guard<std::recursive_mutex> lock(bufferMutex);
  if (file) file.close();
  fs = nullptr;
  serial = false;
  writing = false;
  bufSizeA = 0;
  bufSizeB = 0;
  packetsCounter = 0;
}
