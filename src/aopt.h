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
/**
 * @file aopt.h
 *
 * @brief   aOpt is tiny set of ANSI C library functions for parsing command line. 
 *
 * @details This module is written in ANSI C89. Support short and long styles of
 *          option. Automatic error messages can be turned on by _AOPT_CONF_TRACE==TRUE.
 *          Easy initialization, simple set of operations and automatic help make
 *          this library very friendly and flexible.
 *          Example of option usage:
 *          short style: -o123, -o 123, -o=123, -o
 *          long style: --option 123, --option=123, --option
 *
 * @author  Igor Ivanov <Igor.Ivanov@itseez.com> 
 *
 **/
#ifndef _AOPT_H_
#define _AOPT_H_


#ifdef __cplusplus
extern "C" {
#endif


/** 
 * @enum AOPT_TYPE
 * @brief Define option type.
 */
typedef enum
{
    AOPT_NOARG      = 0,        /**< option does not have value */
    AOPT_REPEAT     = 1,        /**< option can appear few times */
    AOPT_ARG        = 2,        /**< option should have value */
    AOPT_OPTARG     = 4         /**< option can have value */
} AOPT_TYPE;


/**
 * @def AOPT_MAX_NUMBER
 * @brief Maximum number of options
 */
#define AOPT_MAX_NUMBER    4


/**
 * @struct _AOPT_DESC
 * @brief 
 *      Description of supported options.
 */
typedef struct _AOPT_DESC
{
    int key;                                    /**< unique identifier */
    AOPT_TYPE   flags;                          /**< option type */
    const char  shorts[AOPT_MAX_NUMBER];        /**< short name */
    const char* const longs[AOPT_MAX_NUMBER];   /**< long name */
    const char *note;                           /**< option description */
} AOPT_DESC;


/**
 * @struct _AOPT_OBJECT
 * @brief 
 *      Option container.
 */
typedef struct _AOPT_OBJECT
{
    int key;                    /**< unique identifier */
    const char *arg;            /**< option value */
} AOPT_OBJECT;


/**
 * @def aopt_set_literal
 * @brief 
 *      Set option as literal.
 */
#define aopt_set_literal( ... ) \
    { __VA_ARGS__, 0 }


/**
 * @def aopt_set_string
 * @brief 
 *      Set option as string.
 */
#define aopt_set_string( ... ) \
    { __VA_ARGS__, NULL }


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
const AOPT_OBJECT* aopt_init( int *argc, const char **argv, const AOPT_DESC *desc );


/**
 * aopt_exit
 *
 * @brief
 *    The function is used as a destructor. Releases memory allocated in 
 *    the corresponding call. Object can not be used later.
 *
 * @return @a none
 ***************************************************************************/
void aopt_exit( AOPT_OBJECT *aopt_obj );


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
int aopt_check( const AOPT_OBJECT *aopt_obj, int key );


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
const char* aopt_value( const AOPT_OBJECT *aopt_obj, int key );


/**
 * aopt_help
 *
 * @brief
 *    This function form help information  basing options description and
 *    return string with one. The string should be freed using the free() 
 *    function when you are done with it. NULL is returned if the it would 
 *    produce an empty string or if the string cannot be allocated.
 *
 * @param[in]    desc           Option description.
 *
 * @retval pointer to string - on success
 * @retval NULL - on failure
 ***************************************************************************/
const char* aopt_help( const AOPT_DESC *desc );


#ifdef __cplusplus
}
#endif


#endif /* _AOPT_H_ */
