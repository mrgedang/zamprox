//
//  injector.c
//  zamprox
//
//  Created by ~GG~ on 4/2/17.
//  Copyright © 2017 ~GG~. All rights reserved.
//

#include "injector.h"
#include <string.h>
#include <stdio.h>

unsigned char* en64 (const unsigned char *in, unsigned char *out, int inlen)
{
    for (; inlen > 0; inlen -= 3, in+=3)
    {
        
        *out++ = base64digits[in[0] >> 2];
        *out++ = base64digits[((in[0]&3)<<4) | ((inlen > 1)?(in[1]>>4):0)];
        *out++ = (inlen > 1)? base64digits[((in[1] << 2) & 0x3c) | ((inlen > 2)? (in[2] >> 6) : 0)]: '=';
        *out++ = (inlen > 2)? base64digits[in[2] & 0x3f] : '=';
    }
    *out = '\0';
    return out;
}

int de64 (const char *in, char *out, int maxlen)
{
    int len = 0;
    register unsigned char digit1, digit2, digit3, digit4;
    
    if (in[0] == '+' && in[1] == ' ')
        in += 2;
    if (*in == '\r')
        return(0);
    
    do {
        digit1 = in[0];
        if (DECODE64(digit1) == BAD)
            return(-1);
        digit2 = in[1];
        if (DECODE64(digit2) == BAD)
            return(-1);
        digit3 = in[2];
        if (digit3 != '=' && DECODE64(digit3) == BAD)
            return(-1);
        digit4 = in[3];
        if (digit4 != '=' && DECODE64(digit4) == BAD)
            return(-1);
        in += 4;
        *out++ = (DECODE64(digit1) << 2) | (DECODE64(digit2) >> 4);
        ++len;
        if (digit3 != '=')
        {
            *out++ = ((DECODE64(digit2) << 4) & 0xf0) | (DECODE64(digit3) >> 2);
            ++len;
            if (digit4 != '=')
            {
                *out++ = ((DECODE64(digit3) << 6) & 0xc0) | DECODE64(digit4);
                ++len;
            }
        }
    } while
        (*in && *in != '\r' && digit4 != '=' && (maxlen-=4) >= 4);
    
    return (len);
}

int get_first_line(char *buf)
{
    int i = 0, c = 0;
    if(buf == NULL || strlen(buf) < 10)return i;
    do
    {
        c = (unsigned char)buf[i++];
        if(c == '\n')
        {
            if((unsigned char)buf[i-2] == '\r'){
                break;
            }else{
                return 0;
            }
        }
    }while(i < strlen(buf));
    return i;
}

// Helper
void print_request(char *prefix, char *data, size_t length, int dump_mode)
{
    size_t	index;
    int		c;
    int		print_prefix = 1;
    int		line_length = 0;
    
    for (index = 0; index < length; ++index)
    {
        if (print_prefix)
        {
            print_prefix = 0;
            printf("%s", prefix);
        }
        
        c = data[index];
        
        if (c >= ' ' && c <= '~')
        {
            printf("%c", c);
            line_length += 1;
        }
        else if ((c == '\r') && !dump_mode)
        {
            printf("\\r");
            line_length += 2;
        }
        else if ((c == '\n') && !dump_mode)
        {
            printf("\\n");
            line_length += 2;
        }
        else
        {
            printf(".");
            line_length += 1;
        }
        
        if (((c == '\n') && !dump_mode) || (line_length >= 1000))
        {
            printf("\n");
            line_length = 0;
            print_prefix = 1;
        }
    }
    
    if (!print_prefix)
        printf("\n");
}
