/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "strus/lib/error.hpp"
#include "strus/versionVector.hpp"
#include "strus/versionBase.hpp"
#include "strus/errorBufferInterface.hpp"
#include "internationalization.hpp"
#include "strus/base/fileio.hpp"
#include "strus/base/string_format.hpp"
#include <memory>
#include <iostream>
#include <stdexcept>

#undef STRUS_LOWLEVEL_DEBUG

static strus::ErrorBufferInterface* g_errorBuffer = 0;

int main( int argc, const char* argv[])
{
	int rt = 0;
	std::auto_ptr<strus::ErrorBufferInterface> errorBuffer( strus::createErrorBuffer_standard( 0, 2));
	if (!errorBuffer.get())
	{
		std::cerr << _TXT("failed to create error buffer") << std::endl;
		return -1;
	}
	g_errorBuffer = errorBuffer.get();

	return rt;
}



