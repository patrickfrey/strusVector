/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "strus/lib/vector_std.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/base/dll_tags.hpp"
#include "strus/storageModule.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"

static const strus::VectorStorageConstructor standard_vsmodel =
{
	"vector_std", strus::createVectorStorage_std
};

static const char* lapack_license =
	" LAPACK library (http://www.netlib.org/lapack):\n"
	" ----------------------------------------------\n"
	" Copyright (c) 1992-2013 The University of Tennessee and The University\n"
	"    of Tennessee Research Foundation.  All rights reserved.\n"
	" Copyright (c) 2000-2013 The University of California Berkeley.\n"
	"    All rights reserved.\n"
	" Copyright (c) 2006-2013 The University of Colorado Denver.\n"
	"    All rights reserved.\n\n"
	" Redistribution and use in source and binary forms, with or without\n"
	" modification, are permitted provided that the following conditions are met:\n"
	"  * Redistributions of source code must retain the above copyright notice,\n"
	"    this list of conditions and the following disclaimer.\n"
	"  * Redistributions in binary form must reproduce the above copyright\n"
	"    notice, this list of conditions and the following disclaimer in the\n"
	"    documentation and/or other materials provided with the distribution.\n"
	"  * Neither the name of Intel Corporation nor the names of its contributors\n"
	"    may be used to endorse or promote products derived from this software\n"
	"    without specific prior written permission.\n\n"
	" THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"\n"
	" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n"
	" IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n"
	" ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE\n"
	" LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR\n"
	" CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF\n"
	" SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS\n"
	" INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN\n"
	" CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)\n"
	" ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE\n"
	" POSSIBILITY OF SUCH DAMAGE.\n";


extern "C" DLL_PUBLIC strus::StorageModule entryPoint;

strus::StorageModule entryPoint( &standard_vsmodel, 0, lapack_license);




