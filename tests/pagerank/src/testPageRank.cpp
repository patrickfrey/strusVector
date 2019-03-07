/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Test program for pagerank
#include "pageweight.hpp"
#include "strus/base/math.hpp"
#include <vector>
#include <iostream>
#include <stdexcept>

int main( int argc, const char** argv)
{
	try
	{
		typedef strus::PageWeight::PageId PageId;
		strus::PageWeight pg( 30, 0.85);
		// Example taken from http://mathscinotes.com/2012/01/worked-pagerank-example/
		PageId i1 = pg.getOrCreatePageId( "A", true);
		PageId i2 = pg.getOrCreatePageId( "B", true);
		PageId i3 = pg.getOrCreatePageId( "C", true);
		PageId i4 = pg.getOrCreatePageId( "D", true);
		PageId i5 = pg.getOrCreatePageId( "E", true);

		pg.addLink( i1, i2);
		pg.addLink( i1, i3);
		pg.addLink( i1, i4);

		pg.addLink( i2, i1);
		pg.addLink( i2, i3);
		pg.addLink( i2, i4);

		pg.addLink( i3, i1);
		pg.addLink( i3, i2);
		pg.addLink( i3, i4);

		pg.addLink( i4, i1);
		pg.addLink( i4, i2);
		pg.addLink( i4, i3);
		pg.addLink( i4, i5);

		std::vector<double> res = pg.calculate();
		std::vector<double>::const_iterator ri = res.begin(), re = res.end();
		double exp[] = { 0.143994, 0.143994, 0.144, 0.152406, 0.0623895 };
		for (unsigned int ridx=0; ri != re; ++ri,++ridx)
		{
			double err = strus::Math::abs(*ri - exp[ridx]);
			if (err > 1E-4)
			{
				throw std::runtime_error("output not as expected");
			}
			std::cout << *ri << " (" << err << ")" << std::endl;
		}
		return 0;
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << "error: " << err.what() << std::endl;
		return -1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << "out of memory" << std::endl;
		return -1;
	}
	catch (const std::logic_error& err)
	{
		std::cerr << "error: " << err.what() << std::endl;
		return -1;
	}
}


