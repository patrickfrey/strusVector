/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for thread safe operations on representants of similarity classes with help of genetic algorithms
#include "genGroupContext.hpp"
#include "dependencyGraph.hpp"
#include "strus/base/string_format.hpp"

using namespace strus;
using namespace strus::utils;

#define STRUS_LOWLEVEL_DEBUG


static unsigned int age_mutations( const SimGroup& group, unsigned int maxage, unsigned int conf_mutations)
{
	return (conf_mutations * (maxage - std::min( maxage, group.age()))) / maxage;
}

static unsigned int age_mutation_votes( const SimGroup& group, unsigned int maxage, unsigned int conf_votes)
{
	return conf_votes * (std::min( group.age(), maxage) / maxage) + 1;
}

std::vector<ConceptIndex> GenGroupContext::getNeighbourGroups( const SimGroup& group)
{
	std::set<ConceptIndex> neighbour_groups;
	SimGroup::const_iterator mi = group.begin(), me = group.end();
	for (; mi != me; ++mi)
	{
		std::vector<ConceptIndex> nodes;
		{
			SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, *mi);
			nodes = m_sampleSimGroupMap.nodes( SLOCK);
		}
		std::vector<ConceptIndex>::const_iterator si = nodes.begin(), se = nodes.end();
		for (; si != se; ++si)
		{
			if (*si != group.id())
			{
				neighbour_groups.insert( *si);
			}
		}
	}
	return std::vector<ConceptIndex>( neighbour_groups.begin(), neighbour_groups.end());
}

std::string GenGroupContext::groupMembersString( const ConceptIndex& group_id) const
{
	std::ostringstream rt;
	SimGroupRef group = m_groupMap.get( group_id);
	if (group.get())
	{
		SimGroup::const_iterator mi = group->begin(), me = group->end();
		for (unsigned int midx=0; mi != me; ++mi,++midx)
		{
			if (midx) rt << ", ";
			rt << *mi;
		}
	}
	return rt.str();
}

void GenGroupContext::checkSimGroupStructures()
{
	m_sampleSimGroupMap.check();
	std::ostringstream errbuf;
	enum {MaxNofErrors = 30};
	unsigned int nofErrors = 0;

	ConceptIndex gi = 1, ge = m_groupMap.nofGroupIdsAllocated()+1;
	for (; gi != ge; ++gi)
	{
		SimGroupRef group = m_groupMap.get( gi);
		if (!group.get()) continue;
		if (group->id() != gi)
		{
			throw strus::runtime_error( _TXT("group has id %u that is not matching to index %u"), group->id(), gi);
		}
		SimGroup::const_iterator mi = group->begin(), me = group->end();
		for (unsigned int midx=0; mi != me; ++mi,++midx)
		{
			SharedSampleSimGroupMap::Lock LOCK( &m_sampleSimGroupMap, *mi);
			if (!m_sampleSimGroupMap.contains( LOCK, group->id()))
			{
				std::vector<ConceptIndex> car = m_sampleSimGroupMap.nodes( LOCK);
				std::ostringstream carbuf;
				std::vector<ConceptIndex>::const_iterator ci = car.begin(), ce = car.end();
				for (unsigned int cidx=0; ci != ce; ++ci,++cidx)
				{
					if (cidx) carbuf << ", ";
					carbuf << *ci;
				}
				std::string conceptstr( carbuf.str());
				if (m_logout) m_logout << string_format( _TXT("missing element group %u relation of feature %u {%s}"), group->id(), *mi, conceptstr.c_str());
				if (++nofErrors > MaxNofErrors) throw strus::runtime_error(_TXT("internal: errors in sim group structures"));
			}
		}
		group->check();
	}
	SampleIndex si=0, se=m_samplear->size();
	for (; si != se; ++si)
	{
		std::vector<ConceptIndex> car;
		{
			SharedSampleSimGroupMap::Lock LOCK( &m_sampleSimGroupMap, si);
			car = m_sampleSimGroupMap.nodes( LOCK);
		}
		unsigned int cidx=0;
		std::vector<ConceptIndex>::const_iterator ci = car.begin(), ce = car.end();
		for (; ci != ce; ++ci,++cidx)
		{
			SimGroupRef group = m_groupMap.get( *ci);
			if (!group.get())
			{
				if (m_logout) m_logout << string_format( _TXT("entry not found for group %u in group instance map (check structures)"), *ci);
				if (++nofErrors > MaxNofErrors) throw strus::runtime_error(_TXT("internal: errors in sim group structures"));
			}
			else if (!group->isMember( si))
			{
				std::string members = groupMembersString( *ci);
				if (m_logout) m_logout << string_format( _TXT("illegal entry %u in sim group map (check structures), expected to be member of group %u {%s}"), si, group->id(), members.c_str());
				if (++nofErrors > MaxNofErrors) throw strus::runtime_error(_TXT("internal: errors in sim group structures"));
			}
		}
	}
	if (nofErrors)
	{
		throw strus::runtime_error(_TXT("internal: %u errors in sim group structures"), nofErrors);
	}
}

void GenGroupContext::removeGroup( SimGroupIdAllocator& localAllocator, const ConceptIndex& group_id)
{
#ifdef STRUS_LOWLEVEL_DEBUG
	std::string memberstr = groupMembersString( group_id);
	std::cerr << string_format( _TXT("remove group %u {%s}"), group_id, memberstr.c_str()) << std::endl;
#endif
	SimGroupRef group = m_groupMap.get( group_id);
	if (!group.get()) return;

	SimGroup::const_iterator mi = group->begin(), me = group->end();
	for (; mi != me; ++mi)
	{
		SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, *mi);
		m_sampleSimGroupMap.remove( SLOCK, group_id);
	}
	m_groupMap.resetGroup( group_id);
	localAllocator.free( group_id);
}

bool GenGroupContext::tryAddGroupMember(
		const ConceptIndex& group_id, const SampleIndex& newmember,
		unsigned int descendants, unsigned int mutations, unsigned int votes,
		unsigned int maxage)
{
	SimGroupRef group = m_groupMap.get( group_id);
	if (!group.get())
	{
		return false;
	}
	{
		SharedSampleSimGroupMap::Lock LOCK( &m_sampleSimGroupMap, newmember);
		if (!m_sampleSimGroupMap.insert( LOCK, group_id))
		{
			return false;
		}
	}
	SimGroupRef newgroup( new SimGroup(*group));
	newgroup->addMember( newmember);
	newgroup->mutate( *m_samplear, descendants, age_mutations( *newgroup, maxage, mutations), age_mutation_votes( *newgroup, maxage, votes));
	if (newgroup->fitness( *m_samplear) > group->fitness( *m_samplear))
	{
		m_groupMap.setGroup( group_id, newgroup);
		return true;
	}
	else
	{
		SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, newmember);
		(void)m_sampleSimGroupMap.remove( SLOCK, group_id);
		return false;
	}
}

ConceptIndex GenGroupContext::getSampleClosestSimGroup(
		const SampleIndex& main_sampleidx, const SampleIndex& search_sampleidx, 
		unsigned short min_dist) const
{
	ConceptIndex rt = 0;
	std::vector<ConceptIndex> gar;
	{
		SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, main_sampleidx);
		gar = m_sampleSimGroupMap.nodes( SLOCK);
	}
	std::vector<ConceptIndex>::const_iterator gi = gar.begin(), ge = gar.end();
	for (; gi != ge; ++gi)
	{
		SimGroupRef group = m_groupMap.get( *gi);
		if (!group.get()) continue;

		if ((*m_samplear)[ search_sampleidx].near( group->gencode(), min_dist))
		{
			rt = group->id();
			min_dist = (*m_samplear)[ search_sampleidx].dist( group->gencode());
		}
	}
	return rt;
}

bool GenGroupContext::findClosestFreeSample( SimRelationMap::Element& res, const SampleIndex& sampleidx, const SimRelationReader& simrelreader, unsigned int simdist)
{
	std::vector<ConceptIndex> car;
	{
		SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, sampleidx);
		car = m_sampleSimGroupMap.nodes( SLOCK);
	}
	std::vector<SimRelationMap::Element> elements = simrelreader.readSimRelations( sampleidx);
	std::vector<SimRelationMap::Element>::const_iterator ri = elements.begin(), re = elements.end();
	for (; ri != re; ++ri)
	{
		if (ri->simdist > simdist) break;
		{
			SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, ri->index);
			if (m_sampleSimGroupMap.hasSpace( SLOCK) && !m_sampleSimGroupMap.shares( SLOCK, car.data(), car.size()))
			{
				res = *ri;
				return true;
			}
		}
	}
	return false;
}

void GenGroupContext::tryGroupAssignments(
		std::size_t threadid,
		const GenGroupParameter& parameter)
{
	try
	{
		const std::vector<SampleSimGroupAssignment>& alist = m_groupAssignQueue.get( threadid);
		std::vector<SampleSimGroupAssignment>::const_iterator ai = alist.begin(), ae = alist.end();
		for (; ai != ae; ++ai)
		{
			SimGroupRef group = m_groupMap.get( ai->conceptIndex);
			if (!group.get()) continue; 
	
			SimGroupRef newgroup( new SimGroup(*group));
			{
				SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, ai->sampleIndex);
				if (!m_sampleSimGroupMap.insert( SLOCK, ai->conceptIndex)) continue;
			}
			newgroup->addMember( ai->sampleIndex);
			newgroup->mutate( *m_samplear, parameter.descendants, age_mutations( *newgroup, parameter.maxage, parameter.mutations), age_mutation_votes( *newgroup, parameter.maxage, parameter.votes));
			m_groupMap.setGroup( ai->conceptIndex, newgroup);
		}
		if (!alist.empty() && m_logout) m_logout << string_format( _TXT("assigned %u features to concepts"), alist.size());
	}
	catch (const std::bad_alloc& err)
	{
		reportError( _TXT("memory allocation error"));
	}
	catch (const std::runtime_error& err)
	{
		reportError( err.what());
	}
}

bool GenGroupContext::greedyChaseFreeFeatures(
		SimGroupIdAllocator& groupIdAllocator,
		const SampleIndex& sidx,
		const SimRelationReader& simrelreader,
		const GenGroupParameter& parameter)
{
	// Find the closest neighbour, that is not yet in a group with this sample:
	SimRelationMap::Element neighbour;
	if (findClosestFreeSample( neighbour, sidx, simrelreader, parameter.simdist))
	{
		// Try to find a group the current feature belongs to that is closer to the
		// candidate feature:
		ConceptIndex bestmatch_simgroup = getSampleClosestSimGroup( sidx, neighbour.index, neighbour.simdist);
		if (bestmatch_simgroup)
		{
			// ...if we found such a group, we check if the candidate could be added there instead:
			SimGroupRef group = m_groupMap.get( bestmatch_simgroup);
			if (group.get())
			{
				SimGroupRef testgroup( new SimGroup(*group));
				testgroup->addMember( neighbour.index);
				testgroup->mutate( *m_samplear, parameter.descendants, age_mutations( *testgroup, parameter.maxage, parameter.mutations), age_mutation_votes( *testgroup, parameter.maxage, parameter.votes));
				if (testgroup->fitness( *m_samplear) > group->fitness( *m_samplear))
				{
					// ... we have to go over a queue to avoid inconsistencies
					m_groupAssignQueue.push( neighbour.index, bestmatch_simgroup);
					return false;
				}
			}
		}

		// ...if we did not find a close group, then we found a new one with the two elements as members:
		ConceptIndex newidx = groupIdAllocator.alloc();
		SimGroupRef newgroup( new SimGroup( *m_samplear, sidx, neighbour.index, newidx));
		(void)newgroup->mutate( *m_samplear, parameter.descendants, age_mutations( *newgroup, parameter.maxage, parameter.mutations), age_mutation_votes( *newgroup, parameter.maxage, parameter.votes));
		bool success = true;
		{
			SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, neighbour.index);
			success &= m_sampleSimGroupMap.insert( SLOCK, newidx);
		}
		if (success)
		{
			SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, sidx);
			success &= m_sampleSimGroupMap.insert( SLOCK, newidx);
		}
		if (success)
		{
			m_groupMap.setGroup( newidx, newgroup);
			return true;
		}
		else
		{
			{
				SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, sidx);
				m_sampleSimGroupMap.remove( SLOCK, newidx);
			}
			{
				SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, neighbour.index);
				m_sampleSimGroupMap.remove( SLOCK, newidx);
			}
			groupIdAllocator.free( newidx);
			return false;
		}
	}
	return false;
}

void GenGroupContext::greedyNeighbourGroupInterchange(
		SimGroupIdAllocator& groupIdAllocator,
		const ConceptIndex& group_id,
		const GenGroupParameter& parameter,
		unsigned int& interchangecnt)
{
	SimGroupRef group = m_groupMap.get( group_id);
	if (!group.get()) return;

	// Go through all neighbour groups that are in a distance closer to eqdist
	// and integrate their elements as long as it keeps the neighbour group in eqdist.
	// Then try to integrate elements of groups that are closer to simdist:
	std::vector<ConceptIndex> neighbour_groups( getNeighbourGroups( *group));
	std::vector<ConceptIndex>::const_iterator ni = neighbour_groups.begin(), ne = neighbour_groups.end();
	for (; ni != ne; ++ni)
	{
		SimGroupRef sim_group = m_groupMap.get( *ni);
		if (!sim_group.get()) continue;

		if (sim_group->gencode().near( group->gencode(), parameter.eqdist))
		{
			// Try to swallow members in sim_gi as long as it is in eqdist:
			SimGroup::const_iterator mi = sim_group->begin(), me = sim_group->end();
			std::size_t midx = 0;
			while (mi != me)
			{
				if (!group->isMember( *mi))
				{
					// Add eqdist member of sim_group to newgroup:
					SimGroupRef newgroup( new SimGroup(*group));
					{
						SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, *mi);
						if (!m_sampleSimGroupMap.insert( SLOCK, group->id())) continue;
					}
					newgroup->addMember( *mi);
					(void)newgroup->mutate( *m_samplear, parameter.descendants, age_mutations( *newgroup, parameter.maxage, parameter.mutations), age_mutation_votes( *newgroup, parameter.maxage, parameter.votes));
					m_groupMap.setGroup( group_id, group = newgroup);
					++interchangecnt;
					if (!sim_group->gencode().near( group->gencode(), parameter.eqdist))
					{
						break;
					}
					++midx;

					// Skip to current element:
					mi = sim_group->begin(), me = sim_group->end();
					std::size_t mcnt = 0;
					for (; mi != me && mcnt < midx; ++mi,++mcnt){}
				}
				else
				{
					++mi;
					++midx;
				}
			}
			
		}
		if (group->gencode().near( sim_group->gencode(), parameter.simdist))
		{
			// Try add one member of sim_gi to gi that is similar to gi:
			SimGroup::const_iterator mi = sim_group->begin(), me = sim_group->end();
			for (; mi != me; ++mi)
			{
				if (group->gencode().near( (*m_samplear)[*mi], parameter.simdist) && !group->isMember( *mi))
				{
					if (tryAddGroupMember( group_id, *mi, parameter.descendants, parameter.mutations, parameter.votes, parameter.maxage))
					{
						++interchangecnt;
					}
				}
			}
		}
	}
}

bool GenGroupContext::improveGroup(
		SimGroupIdAllocator& groupIdAllocator,
		const ConceptIndex& group_id,
		const GenGroupParameter& parameter)
{
	SimGroupRef group = m_groupMap.get( group_id);
	if (!group.get()) return false;

	bool rt = group->mutate( *m_samplear, parameter.descendants, age_mutations( *group, parameter.maxage, parameter.mutations), age_mutation_votes( *group, parameter.maxage, parameter.votes));

	SimGroup::const_iterator mi = group->begin(), me = group->end();
	for (std::size_t midx=0; mi != me; ++mi,++midx)
	{
		if (!group->gencode().near( (*m_samplear)[ *mi], parameter.simdist))
		{
			// Dropped members that got too far out of the group:
			SampleIndex member = *mi;
			mi = group->removeMemberItr( mi);
			--mi;
			--midx;
			{
				SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, member);
				m_sampleSimGroupMap.remove( SLOCK, group->id());
			}
			if (group->size() < 2) break;
			group->mutate( *m_samplear, parameter.descendants, age_mutations( *group, parameter.maxage, parameter.mutations), age_mutation_votes( *group, parameter.maxage, parameter.votes));
		}
	}
	if (group->size() < 2)
	{
		// Delete group that lost too many members:
		removeGroup( groupIdAllocator, group->id());
	}
	return rt;
}

bool GenGroupContext::similarNeighbourGroupElimination(
		SimGroupIdAllocator& groupIdAllocator,
		const ConceptIndex& group_id,
		const GenGroupParameter& parameter)
{
	SimGroupRef group = m_groupMap.get( group_id);
	if (!group.get()) return false;

	// Go through all neighbour groups that are in a distance closer to eqdist
	// and delete them if most of the members are all within the group:
	std::vector<ConceptIndex> neighbour_groups = getNeighbourGroups( *group);
	std::vector<ConceptIndex>::const_iterator ni = neighbour_groups.begin(), ne = neighbour_groups.end();
	for (; ni != ne; ++ni)
	{
		SimGroupRef sim_group = m_groupMap.get( *ni);
		if (!sim_group.get()) continue;

		if (group->gencode().near( group->gencode(), parameter.eqdist))
		{
			// Eliminate similar group that contains most of this group members and that has a smaller fitness:
			float eqdiff = (float)std::min( group->size(), sim_group->size()) * parameter.eqdiff;
			if (group->diffMembers( *sim_group, eqdiff) < (unsigned int)eqdiff)
			{
				if (sim_group->fitness( *m_samplear) > group->fitness( *m_samplear))
				{
					removeGroup( groupIdAllocator, group->id());
					return true;
				}
			}
		}
	}
	return false;
}

bool GenGroupContext::unfittestGroupElimination(
		SimGroupIdAllocator& groupIdAllocator,
		const ConceptIndex& group_id,
		const GenGroupParameter& parameter)
{
	bool rt = false;
	bool doRemoveGroup = false;

	SimGroupRef group = m_groupMap.get( group_id);
	if (!group.get()) return false;

	SimGroup::const_iterator mi = group->begin(), me = group->end();
	while (mi != me)
	{
		double minFitness = std::numeric_limits<double>::max();
		double maxFitness = 0.0;
		ConceptIndex unfitestGroup_id = 0;

		std::vector<ConceptIndex> gar;
		{
			SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, *mi);
			if (m_sampleSimGroupMap.hasSpace( SLOCK))
			{
				++mi;
				continue;
			}
			gar = m_sampleSimGroupMap.nodes( SLOCK);
		}
		std::vector<ConceptIndex>::const_iterator gi = gar.begin(), ge = gar.end();
		for (; gi != ge; ++gi)
		{
			SimGroupRef member_group = m_groupMap.get( *gi);
			if (!member_group.get()) continue;

			double fitness = member_group->fitness( *m_samplear);
			if (fitness > maxFitness)
			{
				maxFitness = fitness;
			}
			if (member_group->age() >= ((parameter.maxage * STRUS_VECTOR_MAXAGE_MATURE_PERCENTAGE) / 100) && fitness < minFitness)
			{
				minFitness = fitness;
				unfitestGroup_id = *gi;
			}
		}
		if (unfitestGroup_id == group_id && minFitness < maxFitness * STRUS_VECTOR_BAD_FITNESS_FRACTION)
		{
			SimGroupRef newgroup( new SimGroup( *group));
			{
				SharedSampleSimGroupMap::Lock SLOCK( &m_sampleSimGroupMap, *mi);
				m_sampleSimGroupMap.remove( SLOCK, group_id);
			}
			newgroup->removeMember( *mi);
			m_groupMap.setGroup( group_id, group = newgroup);
			rt = true;
			if (group->size() < 2)
			{
				doRemoveGroup = true;
				break;
			}
			mi = group->begin();
			me = group->end();
		}
		else
		{
			++mi;
		}
	}
	if (doRemoveGroup)
	{
		removeGroup( groupIdAllocator, group_id);
	}
	return rt;
}

void GenGroupContext::eliminateRedundantGroups( const GenGroupParameter& parameter)
{
	if (m_logout) m_logout << string_format( _TXT("build the dependency graph of about %u groups"), m_groupMap.nofGroupIdsAllocated());
	DependencyGraph groupDependencyGraph = buildGroupDependencyGraph( m_samplear->size(), m_groupMap.nofGroupIdsAllocated(), m_sampleSimGroupMap.base());
	groupDependencyGraph = reduceGroupDependencyGraphToIsa( groupDependencyGraph, m_groupMap, parameter.isaf);
#ifdef STRUS_LOWLEVEL_DEBUG
	std::cerr << _TXT("dependencies before elimination:") << std::endl;
	{
		DependencyGraph::const_iterator di = groupDependencyGraph.begin(), de = groupDependencyGraph.end();
		for (; di != de; ++di)
		{
			std::cerr << "(" << di->first << "," << di->second << ") {" << groupMembersString(di->first) << "}" << std::endl;
		}
	}
#endif
	if (m_logout) m_logout << _TXT("eliminate circular references from the graph");
	eliminateCircularReferences( groupDependencyGraph);
	if (m_logout) m_logout << string_format( _TXT("got %u dependencies after elimination"), groupDependencyGraph.size());
#ifdef STRUS_LOWLEVEL_DEBUG
	{
		DependencyGraph::const_iterator di = groupDependencyGraph.begin(), de = groupDependencyGraph.end();
		for (; di != de; ++di)
		{
			std::cerr << "(" << di->first << "," << di->second << ") {" << groupMembersString(di->first) << "}" << std::endl;
		}
	}
#endif
	SimGroupIdAllocator groupIdAllocator( m_cntalloc);
	if (m_logout) m_logout << _TXT("eliminate the groups dependent of others");
	DependencyGraph::const_iterator di = groupDependencyGraph.begin(), de = groupDependencyGraph.end();
	while (di != de)
	{
		ConceptIndex gid = di->first;
		removeGroup( groupIdAllocator, gid);
		for (++di; di != de && gid == di->first; ++di){}
	}
}


void GenGroupContext::collectResults(
		std::vector<SimHash>& results,
		std::vector<SampleIndex>& singletons,
		SampleConceptIndexMap& sampleConceptIndexMap,
		ConceptSampleIndexMap& conceptSampleIndexMap,
		const GenGroupParameter& parameter)
{
	// Count the number of singletons and possibly add them to the result:
	singletons.clear();
	conceptSampleIndexMap.clear();
	const SampleSimGroupMap& sampleSimGroupMap_base = m_sampleSimGroupMap.base();
	SampleIndex si=0, se=(*m_samplear).size();
	for (; si != se; ++si)
	{
		if (sampleSimGroupMap_base.nofElements( si) == 0)
		{
			singletons.push_back( si);
			if (parameter.with_singletons)
			{
				// Add singleton to result:
				results.push_back( (*m_samplear)[ si]);
				// Collect si as group member into conceptSampleIndexMap:
				std::vector<SampleIndex> members;
				members.push_back( si);
				conceptSampleIndexMap.add( results.size(), members);
			}
		}
	}
	if (m_logout) m_logout << string_format( _TXT("found %u singletons"), singletons.size());

	// Add the groups to the result and collect group members into conceptSampleIndexMap:
	std::vector<ConceptIndex> gidmap( m_groupMap.nofGroupIdsAllocated(), 0);
	ConceptIndex gi = 1, ge = m_groupMap.nofGroupIdsAllocated()+1;
	for (; gi != ge; ++gi)
	{
		const SimGroupRef& group = m_groupMap.get( gi);
		if (!group.get()) continue;

		results.push_back( group->gencode());
		gidmap[ gi-1] = results.size();
		std::vector<SampleIndex> members;
		SimGroup::const_iterator mi = group->begin(), me = group->end();
		for (; mi != me; ++mi)
		{
			members.push_back( *mi);
		}
		conceptSampleIndexMap.add( results.size(), members);
	}
	// Collect the group membership for every sample into sampleConceptIndexMap:
	sampleConceptIndexMap.clear();
	for (si=0; si != se; ++si)
	{
		std::vector<ConceptIndex> groups;
		SampleSimGroupMap::const_node_iterator
			ni = sampleSimGroupMap_base.node_begin(si),
			ne = sampleSimGroupMap_base.node_end(si);
		for (; ni != ne; ++ni)
		{
			if (*ni == 0 || *ni > (ConceptIndex)gidmap.size() || gidmap[*ni-1] == 0) throw strus::runtime_error(_TXT("illegal group id found: %u"), *ni);
			groups.push_back( gidmap[*ni-1]);
		}
		if (!groups.empty())
		{
			sampleConceptIndexMap.add( si, groups);
		}
	}
	// Collect the group membership for singletons into sampleConceptIndexMap if wished:
	if (parameter.with_singletons)
	{
		std::vector<SampleIndex>::const_iterator xi = singletons.begin(), xe = singletons.end();
		for (ConceptIndex xidx=1; xi != xe; ++xi,++xidx)
		{
			std::vector<ConceptIndex> groups;
			groups.push_back( xidx);
			sampleConceptIndexMap.add( xidx-1, groups);
			// ... singleton groups are numbered one to one to the sigleton features, because added first
		}
	}

#ifdef STRUS_LOWLEVEL_DEBUG
	std::cerr << string_format( _TXT("got %u categories"), results.size()) << std::endl;
	gi = 1, ge = m_groupMap.nofGroupIdsAllocated()+1;
	for (; gi != ge; ++gi)
	{
		const SimGroupRef& group = m_groupMap.get( gi);
		if (!group.get()) continue;

		std::cerr << string_format( _TXT("category %u: "), group->id());
		SimGroup::const_iterator mi = group->begin(), me = group->end();
		for (unsigned int midx=0; mi != me; ++mi,++midx)
		{
			if (midx) std::cerr << ", ";
			std::cerr << *mi;
		}
		std::cerr << std::endl;
	}
#endif
}


