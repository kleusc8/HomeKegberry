#include "KegboardPacket.h"


uint16_t crc16(const uint8_t* input, uint16_t len, uint16_t crc)
{
    static const uint8_t oddparity[16] =
        { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

    for (uint16_t i = 0 ; i < len ; i++) {
        // Even though we're just copying a byte from the input,
        // we'll be doing 16-bit computation with it.
        uint16_t cdata = input[i];
        cdata = (cdata ^ crc) & 0xff;
        crc >>= 8;

        if (oddparity[cdata & 0x0F] ^ oddparity[cdata >> 4])
            crc ^= 0xC001;

        cdata <<= 6;
        crc ^= cdata;
        cdata <<= 1;
        crc ^= cdata;
    }

    return crc;
}

static void serial_print_int(int value) {
  Serial.write(value & 0xff);
  Serial.write((value >> 8) & 0xff);
}

KegboardPacket::KegboardPacket()
{
  Reset();
}

bool KegboardPacket::IsReset() {
  return (m_type == 0) && (m_len == 0);
}

void KegboardPacket::Reset()
{
  m_len = 0;
  m_type = 0;
}

void KegboardPacket::AddTag(uint8_t tag, uint8_t buflen, const char *buf)
{
  m_payload[m_len++] = tag;
  m_payload[m_len++] = buflen;
  AppendBytes(buf, buflen);
}

int KegboardPacket::FindTagLength(uint8_t tagnum) {
  uint8_t* buf = FindTag(tagnum);
  if (buf == NULL) {
    return -1;
  }
  return buf[1] & 0xff;
}

uint8_t* KegboardPacket::FindTag(uint8_t tagnum) {
  uint8_t pos=0;
  while (pos < m_len && pos < KBSP_PAYLOAD_MAXLEN) {
    uint8_t tag = m_payload[pos];
    if (tag == tagnum) {
      return m_payload+pos;
    }
    pos += 2 + m_payload[pos+1];
  }
  return NULL;
}

bool KegboardPacket::ReadTag(uint8_t tagnum, uint8_t *value) {
  uint8_t *offptr = FindTag(tagnum);
  if (offptr == NULL) {
    return false;
  }
  *value = *(offptr+2);
  return true;
}

int KegboardPacket::CopyTagData(uint8_t tagnum, void* dest) {
  uint8_t *offptr = FindTag(tagnum);
  if (offptr == NULL) {
    return -1;
  }
  uint8_t slen = *(offptr+1);
  memcpy(dest, (offptr+2), slen);
  return slen;
}

void KegboardPacket::AppendBytes(const char *buf, int buflen)
{
  int i=0;
  while (i < buflen && m_len < KBSP_PAYLOAD_MAXLEN) {
    m_payload[m_len++] = (uint8_t) (*(buf+i));
    i++;
  }
}

uint16_t KegboardPacket::GenCrc()
{
  uint16_t crc = KBSP_PREFIX_CRC;

  crc = crc16((uint8_t*)&m_type, sizeof(m_type), crc);
  crc = crc16(&m_len, sizeof(m_len), crc);

  crc = crc16(m_payload, m_len, crc);

  return crc;
}

void KegboardPacket::Print()
{
  int i;
  uint16_t crc = KBSP_PREFIX_CRC;

  // header
  // header: prefix
  Serial.print(KBSP_PREFIX);

  // header: message_id
  serial_print_int(m_type);
  crc = crc16((uint8_t*)&m_type, sizeof(m_type), crc);

  // header: payload_len
  serial_print_int(m_len);
  crc = crc16(&m_len, (sizeof(m_len)), crc);

  // payload
  for (i=0; i<m_len; i++) {
    Serial.write(m_payload[i]);
  }
  crc = crc16(m_payload, m_len, crc);

  // trailer
  serial_print_int(crc);
  Serial.write('\r');
  Serial.write('\n');
}
