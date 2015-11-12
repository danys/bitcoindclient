#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char encoding_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static int mod_table[] = {0, 2, 1};

char *base64Encode(char *data)
{
    int input_length = strlen(data);
    /* convert 3 chars to 4 base64 chars (every base64 char consists of 6 bits) */
    int output_length = 4 * ((input_length + 2) / 3);
    char *encoded_data = (char*)malloc((output_length+1)*sizeof(char));
    if (encoded_data == NULL) return NULL;
    for (int i = 0, j = 0; i < input_length;)
    {
        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
        /* triple form MSB to LSB => first byte all zeros, second byte next data byte, third byte next after next data byte, and so on */
        encoded_data[j++] = encoding_table[(triple >> 18) & 0x3F]; /* shift 18 positions to the right and retain the 6 remaining bits */
        encoded_data[j++] = encoding_table[(triple >> 12) & 0x3F]; /* shift 12 positions to the right and filter out the 6 LSBs */
        encoded_data[j++] = encoding_table[(triple >> 6) & 0x3F]; /* shift 6 positions to the right and filter out the 6 LSBs */
        encoded_data[j++] = encoding_table[(triple) & 0x3F]; /* filter out the 6 LSBs */
    }
    for (int i = 0; i < mod_table[input_length % 3]; i++) encoded_data[output_length - 1 - i] = '=';
    encoded_data[output_length]='\0';
    return encoded_data;
}