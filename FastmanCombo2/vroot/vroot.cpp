#include <stdio.h>
#include <stdlib.h>

#include "base64.h"
#include "md5.h"

int convert_to_hex(const char* pstring, int size, char* xptr) {
    char buf[3] = { 0 };
    int i;
    if (size % 2)
        return 0;
    for (i = 0; i < size / 2; i++) {
        buf[0] = pstring[i + i];
        buf[1] = pstring[i + i + 1];
        xptr[i] = strtol(buf, 0, 16);
    }
    return i;
}

int calcxor(char* buffer_x_8);

int step2(const char* buffer_x_8, char* buffer_xor_8) {
    int tmp = calcxor((char*) buffer_x_8);
    //	printf("%u\n", tmp);
    sprintf(buffer_xor_8, "%u", tmp);
    return strlen(buffer_xor_8);
}

int step3(bool m_check, const char*m_random, char*buffer_xor_8, char** base64_buffer_address);

int from_string_to_string(const char*m_csSrcString, bool m_check, const char*m_random, char* outbuffer, int outsize) {
    char buffer_x_8[8];
    char buffer_xor_8[100] = { 0 };
    char* base64bufferaddress = NULL;
    int sizebuffer_xor_8 = 0;

    convert_to_hex(m_csSrcString, strlen(m_csSrcString), buffer_x_8);

    // crc hex value
    sizebuffer_xor_8 = step2(buffer_x_8, buffer_xor_8);

    step3(m_check, m_random, buffer_xor_8, &base64bufferaddress);

    int buflen = strlen(base64bufferaddress);
    if (buflen > outsize) {
        // buffer is not enough
        delete[] base64bufferaddress;
        return 0;
    }

    strncpy(outbuffer, base64bufferaddress, outsize);
    delete[] base64bufferaddress;
    return buflen;

}

int calcxor(char* buffer_x_8) {
    int i = 0;
    unsigned long retval = 0;
    unsigned char* data = (unsigned char*) buffer_x_8;

    for (i = 7; i > 0; i--) {
        data[i] ^= data[i - 1];
    }

    retval = *(unsigned long*) (buffer_x_8 + 4) ^ *(unsigned long*) buffer_x_8;
    return retval;
}

int Base64Encode(char * base64code, const char * src, int src_len = 0) {
//    QByteArray d(src, src_len);
//    QByteArray out = d.toBase64();
//    memcpy(base64code, out.data(), out.size());
//    base64code[out.size()] = '\0';
//    return out.size();

    std::string enc = base64_encode((const unsigned char*)src, src_len);
    memcpy(base64code, enc.data(), enc.size());
    base64code[enc.size()] = '\0';
    return enc.size();
}

int md5_cale(const unsigned char* data, unsigned int len, unsigned char* out) {
//    QCryptographicHash md5(QCryptographicHash::Md5);
//    md5.addData((const char *) data, len);
//    QByteArray d = md5.result();
//    memcpy(out, d.data(), d.size());
//    return d.size();

    MD5 md5;
    md5.ComputMd5(data, len);
    int ret = md5.getResult(out);
    return ret;
}

int encode_base64(char* buffer_data, int datalen, char** base64_outbuffer) {
    if (datalen == 0) {
        datalen = strlen(buffer_data);
    }

    int buflen = datalen * 16 / 7 + 4;
    *base64_outbuffer = new char[buflen];
    if (*base64_outbuffer == NULL) {
        return 0;
    }

    int len_b64 = Base64Encode(*base64_outbuffer, buffer_data, datalen);

    return len_b64;
}

unsigned long atox(const char *buffer, int begin) {
    unsigned long hex = 0;
    int szlen = strlen(buffer);
    for (int i = begin; i < szlen; i++) {
        unsigned char ch = (unsigned char) buffer[i];
        if (ch >= '0' && ch <= '9') {
            ch = ch - '0';
        } else if ((ch | 0x20) >= 'a' && (ch | 0x20) <= 'f') {
            ch = (ch | 0x20) - 'a' + 10;
        } else {
            return 0;
        }
        hex = (hex << 4) | ch;
    }

    return hex;
}

// two encrypt table
unsigned char ArrayTable[0x0030] = { 0xae, 0xfe, 0x8b, 0x8c, 0xa2, 0xe9, 0xda, 0x4d, 0xf7, 0x71, 0x58, 0x65, 0xa5, 0xe5,
                                     0x2d, 0xa8, 0xbc, 0xc7, 0x06, 0x10, 0x00, 0x00, 0x00, 0x00, 0x2e, 0x3f, 0x41, 0x56, 0x5f, 0x35, 0x36, 0x41,
                                     0x42, 0x30, 0x45, 0x30, 0x44, 0x40, 0x40, 0x00, 0xbc, 0xc7, 0x06, 0x10, 0x00, 0x00, 0x00, 0x00 };

unsigned long DwordTable[] = { 0xE8550564, 0x262F025A, 0x2587925A, 0xFEEB7FDA, 0x57D98C86, 0x736A2DE9, 0xC1F286A5,
                               0xA80A4D8B, 0x80CC18EA, 0x18C11656, 0xA7EDA8AE, 0xFFE50408, 0x058D9C3E, 0x0DF5DAF7, 0x62BE380B, 0x35FBA718,
                               0x8C8BFEAE, 0x4DDAE9A2, 0x655871F7, 0xA82DE5A5 };

int step3(bool m_check, const char*m_random, char*buffer_xor_8, char** base64_buffer_address) {
    unsigned char index[4];
    unsigned char chval;

    srand(1000);
    int randval = rand();
    if (m_check) {
        randval = atox(m_random, 0);
    }

    index[0] = (randval >> 0x0C) & 0xf;
    index[1] = (randval >> 0x8) & 0xf;
    index[2] = (randval >> 0x4) & 0xf;
    index[3] = randval & 0xf;

    chval = (randval >> 8) & 0xFF;
    int tmp = randval & 0xFF;

    int len = strlen(buffer_xor_8);
    int i;
    //
    // 	for (i=0; i<len; i++)
    // 	{
    // 		printf("%02X", buffer_xor_8[i]);
    // 	}
    // 	printf("\n");
    // 	printf("rand = %08x\n", randval);

    buffer_xor_8[0] ^= ArrayTable[index[3]];
    for (i = 1; i < len; i++) {
        buffer_xor_8[i] ^= buffer_xor_8[i - 1];
    }

    // decrypt  code
    // 	for(i=len-1; i>0; i--)
    // 	{
    // 		buffer_xor_8[i] ^= buffer_xor_8[i-1];
    // 	}
    // 	buffer_xor_8[0] ^= ArrayTable[index[3]];

    unsigned long CalcMD5Cache[4];
    md5_cale((unsigned char*) buffer_xor_8, (unsigned int) len, (unsigned char*) CalcMD5Cache);

    for (i = 0; i < 4; i++) {
        CalcMD5Cache[i] ^= DwordTable[index[i]];
    }

    // src + md5(src) + crc(rand())
    memcpy(buffer_xor_8 + len, CalcMD5Cache, sizeof(CalcMD5Cache));
    buffer_xor_8[len + 16] = chval ^ 0x6e;
    buffer_xor_8[len + 17] = tmp ^ 0xa3;
    buffer_xor_8[len + 18] = (tmp ^ chval) ^ 0x4F;

    // base64( src + md5(src) + crc(rand()) )
    encode_base64(buffer_xor_8, len + 19, base64_buffer_address);
    return 0;
}
