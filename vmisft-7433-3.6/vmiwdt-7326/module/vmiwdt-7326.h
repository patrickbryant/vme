
/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2002-2005 GE Fanuc
    International Copyright Secured.  All Rights Reserved.

-------------------------------------------------------------------------------

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   o Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.
   o Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   o Neither the name of GE Fanuc nor the names of its contributors may be used
     to endorse or promote products derived from this software without specific
     prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

===============================================================================
*/


#ifndef __VMIWDTF_H
#define __VMIWDTF_H


#define VMIWDT_WCSR                     0x02
#define VMIWDT_WKPA                     0x01

#define VMIWDT_WCSR__SERR               0x0002
#define VMIWDT_WCSR__TO                 0x0070
#define VMIWDT_WCSR__TO__2048ns         0x0070
#define VMIWDT_WCSR__TO__32768ns        0x0060
#define VMIWDT_WCSR__TO__131ms          0x0050
#define VMIWDT_WCSR__TO__262ms          0x0040
#define VMIWDT_WCSR__TO__524ms          0x0030
#define VMIWDT_WCSR__TO__2100ms         0x0020
#define VMIWDT_WCSR__TO__33600ms        0x0010
#define VMIWDT_WCSR__TO__67s            0x0000
#define VMIWDT_WCSR__ENABLE             0x0001
#define VMIWDT_WCSR__DISABLE            0x0000


#endif				/* __VMIWDTF_H */
