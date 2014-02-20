// Copyright 2010 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
#pragma once
#ifndef _GPA_INSTRUMENTATION_H_
#define _GPA_INSTRUMENTATION_H_

/*! Intel Graphics Performance Analyizer (GPA) allows for CPU tracing of tasks
    in a frame.  Define PROFILEGPA to send task notifications to GPA.  
*/
#if defined( PROFILEGPA )
#include <ittnotify.h>

class GPAScopedTask
{
public:

    __itt_domain* m_pDomain;

    inline GPAScopedTask( const char* szName, __itt_domain* pDomain )
    {
        m_pDomain = pDomain;
        __itt_string_handle* szTaskName = __itt_string_handle_createA( szName );
        __itt_task_begin( m_pDomain, __itt_null, __itt_null, szTaskName );
    }

    inline ~GPAScopedTask( void )
    {
        __itt_task_end( m_pDomain );
    }
};

#else

#define __itt_domain void
#define __itt_domain_createA( Name ) NULL
#define __itt_string_handle void
#define __itt_string_handle_createA( Name ) NULL
#define __itt_null 0

class GPAScopedTask
{
public:

    __itt_domain* m_pDomain;

    inline GPAScopedTask( const char* szName, __itt_domain* pDomain )
    {
    }

    inline ~GPAScopedTask( void )
    {
    }
};

#endif // PROFILEGPA


#define GPA_SCOPED_TASK( FunctionName, Domain ) \
            GPAScopedTask _task_( FunctionName, (__itt_domain*)Domain )


#endif //_GPA_INSTRUMENTATION_H_
