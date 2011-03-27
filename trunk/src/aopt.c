 /*
 * Copyright (c) 2011 Mellanox Technologies Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the Mellanox Technologies Ltd nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "aopt.h"

#define _AOPT_CONF_TRACE 	TRUE

#if defined(_MSC_VER)
    #ifndef snprintf
        #define snprintf _snprintf
    #endif
#endif

#if defined(_AOPT_CONF_TRACE) && (_AOPT_CONF_TRACE==TRUE)
    #if defined(__KERNEL__)
        #if defined(__LINUX__)
            #define AOPT_TRACE(fmt, ...)  printk(fmt, ##__VA_ARGS__)
        #else
            #define AOPT_TRACE(fmt, ...)  DbgPrint(fmt, ##__VA_ARGS__)
        #endif
    #else
        #define AOPT_TRACE(fmt, ...)  printf(fmt, ##__VA_ARGS__)
    #endif
#else
    #define AOPT_TRACE(fmt, ...)
#endif /* _AOPT_CONF_TRACE */



/**
 * aopt_init
 *
 * @brief
 *    This function parses command line and creates object with options
 *    basing options description. It returns count of correct tokens.
 *
 * @param[in,out] argc           Argument count.
 * @param[in]     argv           Argument vector.
 * @param[in]     desc           Option description.
 *
 * @retval pointer to object - on success
 * @retval NULL - on failure
 ***************************************************************************/
const AOPT_OBJECT* aopt_init( int *argc, const char **argv, const AOPT_DESC *desc )
{
    AOPT_OBJECT *arg_obj = NULL;
    int status = 0;

    if (*argc)
    {
        const char* const *arg_p= argv + 1;
        size_t opt_count= 0;

        for( ; *arg_p; ++arg_p )
        {
            if( '-' == (*arg_p)[0] && (*arg_p)[1] )
            {
                ++opt_count;
            }
        }
        if (opt_count)
        {
            arg_obj= (AOPT_OBJECT*)malloc( (opt_count + 1) * sizeof(AOPT_OBJECT) );
            memset(arg_obj, 0, (opt_count + 1) * sizeof(AOPT_OBJECT));
        }
    }
    if (arg_obj)
    {
        const char **arg_p= argv + 1;
        AOPT_OBJECT *next_option= arg_obj;
        *argc = 0;

        for( ; *arg_p; arg_p++ )
        {
            if( '-' == (*arg_p)[0] && (*arg_p)[1] )
            {
                /* process valid short option (-) */
                int token_type = ('-' == (*arg_p)[1] ? 2 : 1);
                const char *token_opt = (*arg_p) + token_type;
                const char *token_arg = NULL;

                if (!(*token_opt))
                {
                    status = __LINE__;
                    AOPT_TRACE("Incorrect token %s\n", (*arg_p));
                }
                else
                {
                    const AOPT_DESC *opt_spec_p= desc;
        
                    for( ; (opt_spec_p->key && !status); opt_spec_p++ )
                    {
                        /*
                         * Accept -o123, -o 123, -o=123, -o
                         */
                        if ( (token_type == 1) && strchr( opt_spec_p->shorts, *token_opt ))
                        {
                            token_arg = token_opt + 1;
                        }
                        
                        /*
                         * Accept --option 123, --option=123, --option
                         */
                        if ( token_type == 2)
                        {
                            const char *desc_long = *opt_spec_p->longs;
                            int i = 0;

                            for (i = 0; ((i < AOPT_MAX_NUMBER) && (desc_long)); i++)
                            {
                                if (!strncmp(token_opt, desc_long, strlen(desc_long)))
                                {
                                    if (!token_opt[strlen(desc_long)] || (token_opt[strlen(desc_long)] == '='))
                                    {
                                        token_arg = token_opt + strlen(desc_long);
                                        break;
                                    }
                                }
                            }
                        }

                        if (!status && token_arg)
                        {
                            /* 
                             * check if option can be repeatable
                             */
                            if( !( opt_spec_p->flags & AOPT_REPEAT ))
                            {
                                const AOPT_OBJECT *opt_p= arg_obj;

                                for( ; opt_p != next_option; ++opt_p )
                                {
                                    if( opt_p->key == opt_spec_p->key )
                                    {
                                        status = __LINE__;
                                        AOPT_TRACE("Option %s should not be repeatable\n", (*arg_p));
                                    }
                                }
                            }

                            if (!status)
                            {
                                /* set option identifier */
                                next_option->key = opt_spec_p->key;

                                /* set option argument */
                                next_option->arg= NULL;

                                /* have valid token (option) and count it */
                                (*argc)++;

                                if ( opt_spec_p->flags & AOPT_ARG )
                                {
                                    /* 
                                     * check if argument follows the option and =  in the same token
                                     * Example : [-o=1234]
                                     */
                                    if ( (token_arg[0] == '=') && token_arg[1] )
                                    {
                                        next_option->arg = token_arg + 1;
                                    }
                                    /* 
                                     * check if argument follows the option in the same token
                                     * Example : [-o1234]
                                     */
                                    else if ( token_arg[0] )
                                    {
                                        next_option->arg = token_arg;
                                    }
                                    /* 
                                     * check if argument follows the option next token
                                     * Example : [-o] [1234]
                                     */
                                    else if ( (*(arg_p + 1)) && ((*(arg_p + 1))[0] != '-') )
                                    {
                                        arg_p++;
                                        next_option->arg = *arg_p;

                                        /* have valid token (value) and count it */
                                        (*argc)++;
                                    }
                                    else
                                    {
                                        status = __LINE__;
                                        AOPT_TRACE("Option %s should have an argument\n", (*arg_p));
                                    }
                                }
                                else
                                {
                                    if ( token_arg[0] )
                                    {
                                        status = __LINE__;
                                        AOPT_TRACE("Option %s should not have an argument\n", (*arg_p));
                                    }
                                }
                            }
                            break;
                        }
                    }

                    if (opt_spec_p->key)
                    {
                        /* move to the next frame */
                        next_option++;
                    }
                    else
                    {
#if 0 /* check unsupported options */
                        status = __LINE__;
                        AOPT_TRACE("Token %s is incorrect\n", (*arg_p));
#endif
                    }
                }
            }
        }
    }

    if (status)
    {
        if (arg_obj)
        {
            free(arg_obj);
            arg_obj = NULL;
        }
#if 0 /* display error line */
        AOPT_TRACE("Error %d\n", status);
#endif
    }

    return arg_obj;
}


/**
 * aopt_exit
 *
 * @brief
 *    The function is used as a destructor. Releases memory allocated in 
 *    the corresponding call. Object can not be used later.
 *
 * @return @a none
 ***************************************************************************/
void aopt_exit( AOPT_OBJECT *aopt_obj )
{
    if (aopt_obj)
    {
        free(aopt_obj);
    }
}


/**
 * aopt_check
 *
 * @brief
 *    Returns number of appearance of the option in command line.
 *
 * @param[in]    aopt_obj       Object.
 * @param[in]    key            Option key.
 *
 * @retval (>0) - on success
 * @retval ( 0) - on failure
 ***************************************************************************/
int aopt_check( const AOPT_OBJECT *aopt_obj, int key )
{
    int count = 0;

    while ( aopt_obj && aopt_obj->key )
    {
        if ( aopt_obj->key == key ) 
        {
            count++;
        }
        aopt_obj++;
    }

    return count;
}


/**
 * aopt_value
 *
 * @brief
 *    Returns option value by key.
 *
 * @param[in]    aopt_obj       Object.
 * @param[in]    key            Option key.
 *
 * @retval pointer to value - on success
 * @retval NULL - on failure
 ***************************************************************************/
const char* aopt_value( const AOPT_OBJECT *aopt_obj, int key )
{
    const char *value = NULL;

    while ( aopt_obj && aopt_obj->key )
    {
        if ( aopt_obj->key == key ) 
        {
            value = aopt_obj->arg;
            break;
        }
        aopt_obj++;
    }

    return value;
}


/**
 * aopt_help
 *
 * @brief
 *    This function form help informaion  basing options description and
 *    return string with one. The string should be freed using the free() 
 *    function when you are done with it. NULL is returned if the it would 
 *    produce an empty string or if the string cannot be allocated.
 *
 * @param[in]    desc           Option description.
 *
 * @retval pointer to string - on success
 * @retval NULL - on failure
 ***************************************************************************/
const char* aopt_help( const AOPT_DESC *desc )
{
    char *buf = NULL;
    int buf_size = 256;
    int buf_offset = 0;

    if (desc)
    {
        char *buf_temp = NULL;
        int ret = 0;

        buf = (char*)malloc(buf_size);
        memset(buf, 0, buf_size);

        for (; desc && desc->key && buf; desc++)
        {
            char buf_short[10];
            char buf_long[50];
            const char *cur_ptr_short = NULL;
            const char* const *cur_ptr_long = NULL;
            int cur_len_short = 0;
            int cur_len_long = 0;

            memset(buf_short, 0, sizeof(buf_short));
            memset(buf_long, 0, sizeof(buf_long));

            /* fill short option field */
            cur_ptr_short = desc->shorts;
            cur_len_short = 0;
            ret = 0;
            while (cur_ptr_short && (*cur_ptr_short) && (ret >= 0))
            {
                ret = snprintf( (buf_short + cur_len_short),
                                sizeof(buf_short) - cur_len_short,
                                (ret ? ",-%c" : "-%c"), 
                                (isprint(*cur_ptr_short) ? *cur_ptr_short : '.'));
                if (ret < 0)
                {
                    /* size of buffer is exceeded */
                    free(buf);
                    buf = NULL;
                    break;
                }
                cur_len_short += ret;
                cur_ptr_short++;
            }

            /* fill long option field */
            cur_ptr_long = desc->longs;
            cur_len_long = 0;
            ret = 0;
            while (cur_ptr_long && (*cur_ptr_long)  && (ret >= 0))
            {
                ret = snprintf( (buf_long + cur_len_long),
                                sizeof(buf_long) - cur_len_long,
                                (ret ? ",--%s" : "--%s"), 
                                (*cur_ptr_long ? *cur_ptr_long : ""));
                if (ret < 0)
                {
                    /* size of buffer is exceeded */
                    free(buf);
                    buf = NULL;
                    break;
                }
                cur_len_long += ret;
                cur_ptr_long++;
            }

            /* form help for current option */
            while (buf) 
            {
            	char format[50];

            	if (strlen(buf_long) > 21 )
            	{
            		sprintf(format, " %%-7s %%-21s\n %-7s %-21s\t-%%s\n", "", "");
            	}
            	else
            	{
            		sprintf(format, " %%-7s %%-21s\t-%%s\n");
            	}

            	ret = snprintf( (buf + buf_offset),
                                (buf_size - buf_offset),
                                 format,
                                 buf_short, 
                                 buf_long, 
                                 (desc->note ? desc->note : ""));

                /* If that worked, return */
                if (ret > -1 && ret <= (buf_size - buf_offset - 1))
                {
                    buf_offset += ret;
                    break;
                }

                /* Else try again with more space. */
                if (ret > -1)     /* ISO/IEC 9899:1999 */
                    buf_size = buf_offset + ret + 1;
                else            /* twice the old size */
                    buf_size *= 2;

                buf_temp = buf;
                buf = (char*)malloc(buf_size);
                memset(buf, 0, buf_size);
                memcpy(buf, buf_temp, buf_offset);
                free(buf_temp);
            }
        }
    }

    return buf;
}
