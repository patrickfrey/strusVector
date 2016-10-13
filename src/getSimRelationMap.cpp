/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Function for evaluating similarity relations (incl. multithreaded)
#include "getSimRelationMap.hpp"
#include "logger.hpp"
#include "strus/base/string_format.hpp"

using namespace strus;

SimRelationMap strus::getSimRelationMap( const std::vector<SimHash>& samplear, unsigned int simdist, const char* logfile)
{
	Logger logout( logfile);
	SimRelationMap rt;
	if (logout) logout << _TXT("calculate similarity relation map");
	std::vector<SimHash>::const_iterator si = samplear.begin(), se = samplear.end();
	for (SampleIndex sidx=0; si != se; ++si,++sidx)
	{
		if (logout && sidx % 10000 == 0) logout << string_format( _TXT("processed %u lines with %u similarities"), sidx, rt.nofRelationsDetected());
		std::vector<SimRelationMap::Element> row;

		std::vector<SimHash>::const_iterator pi = samplear.begin();
		for (SampleIndex pidx=0; pi != si; ++pi,++pidx)
		{
			if (pidx != sidx && si->near( *pi, simdist))
			{
				unsigned short dist = si->dist( *pi);
				row.push_back( SimRelationMap::Element( pidx, dist));
			}
		}
		rt.addRow( sidx, row);
	}
	if (logout) logout << string_format( _TXT("got %u with %u similarities"), samplear.size(), rt.nofRelationsDetected());
	return rt.mirror();
}


