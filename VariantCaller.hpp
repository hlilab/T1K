#ifndef _MOURISL_VARIANT_CALLER
#define _MOURISL_VARIANT_CALLER

#include "SeqSet.hpp"
#include "Genotyper.hpp"

struct _variant
{
	int seqIdx ;
	int refStart, refEnd ;
	char ref[10] ;
	char var[10] ;
	double allSupport ;
	double varSupport ;
	double varUniqSupport ;
	
	int qual ;
} ;

struct _baseVariant
{
	double count[4] ;
	double uniqCount[4] ;
	bool exon ;
	int candidateId ; // -1: not a variant candidate. 

	double AllCountSum()
	{
		return count[0] + count[1] + count[2] + count[3] ;
	}
	
	double UniqCountSum()
	{
		return uniqCount[0] + uniqCount[1] + uniqCount[2] + uniqCount[3] ;
	}
} ;

struct _adjFragmentToBaseVariant
{
	int seqIdx ;
	int refPos ;
	char nuc[5] ; // which nucleotide this fragment support
	int weight ;

	int next ;	
} ;

struct _adjBaseVariantToFragment
{
	int fragIdx ;
	char nuc[5] ;
	int next ;	
} ;

struct _enumVarResult
{
	double bestCover ; // the number of covered results
	SimpleVector<char> bestEnumVariants ;
	SimpleVector<char> equalBestEnumVariants ;
} ;


class VariantCaller
{
private:
	SeqSet &refSet ;
	std::vector< SimpleVector<struct _baseVariant> > baseVariants ;
	std::vector<double> seqAbundance ;
	SimpleVector<struct _pair> candidateVariants ; // a: seqidx, b: refpos
	SimpleVector<int> candidateVariantGroup ; // variant id to group id.
	std::vector<struct _variant> finalVariants ; 
	void UpdateBaseVariantFromOverlap(char *read, double weight, struct _overlap o)
	{
		if (o.seqIdx == -1)
			return ;
		int i, k ;
		int readLen = strlen(read)	;
		char *r = read ;
		if (o.strand == -1)
		{
			r = strdup(read) ;
			refSet.ReverseComplement(r, read, readLen) ;
		}

		//char *align = new char[ 3 * readLen + 2 ] ;
		char *align = o.align ;
		if (align == NULL)
		{
		  char *align = new char[ 3 * readLen + 2 ] ;
			AlignAlgo::GlobalAlignment( refSet.GetSeqConsensus(o.seqIdx) + o.seqStart, 
				o.seqEnd - o.seqStart + 1,
				r + o.readStart,
				o.readEnd - o.readStart + 1, align) ;
		}

		int refPos = o.seqStart ;
		int readPos = o.readStart ;

		refPos = o.seqStart ;
		readPos = o.readStart ;
		for ( k = 0 ; align[k] != -1 ; ++k )
		{
			if ( align[k] == EDIT_MATCH || align[k] == EDIT_MISMATCH)
			{
				if (weight == 1)
					baseVariants[o.seqIdx][refPos].uniqCount[nucToNum[r[readPos] - 'A']] += weight ;
				baseVariants[o.seqIdx][refPos].count[ nucToNum[r[readPos] - 'A']] += weight;
			}
			//TODO: handle indels
			//if (isValidDiff[refPos].exon && (align[k] == EDIT_DELETE) )
			//	printf("%s\n%s\n", seq.consensus + o.seqStart, r) ;
			if (align[k] != EDIT_INSERT)
				++refPos ;
			if (align[k] != EDIT_DELETE)
				++readPos ;
		}

		if (o.strand == -1)
			free(r);
	}

	int GetCandidateVariantGroup(int gid)
	{
		if (candidateVariantGroup[gid] != gid)
			return candidateVariantGroup[gid] = GetCandidateVariantGroup(candidateVariantGroup[gid]) ;
		return gid ;
	}
	
	bool containCandidateVar(int start, int end, SimpleVector<int> &candidateVarAccuCount)
	{
		/*if (start == 0)
		{
			if (candidateVarAccuCount[start] > 0
					|| candidateVarAccuCount[end] != candidateVarAccuCount[start])
				return true ;
		}
		else
		{
			if (candidateVarAccuCount[end] != candidateVarAccuCount[start - 1])
				return true ;
		} 
		return false ;*/

		// candidateVarAccuCount starts from position 0 for value 0
		return candidateVarAccuCount[start] != candidateVarAccuCount[end + 1] ;
	}
	
	struct _overlap SelectOverlapFromFragmentOverlap(int k, struct _fragmentOverlap &frag)
	{
		if (k == 0)
			return frag.overlap1 ;
		else if (k == 1)
			return frag.overlap2 ;	
		return frag.overlap1 ;
	}

	/*char *SelectRead(char *r1, char *r2, struct _fragmentOverlap &frag)
	{
	}*/
	
	void ComputeCandidateVarAccuCount(int seqIdx, SimpleVector<int> &candidateVarAccuCount)
	{
		int i ;
		int len = baseVariants[seqIdx].Size() ;
		candidateVarAccuCount.ExpandTo(len + 1) ;
		candidateVarAccuCount[0] = 0 ;
		for (i = 0 ; i < len ; ++i)
		{
			if (baseVariants[seqIdx][i].candidateId != -1)
				candidateVarAccuCount[i + 1] = candidateVarAccuCount[i] + 1 ;
			else
				candidateVarAccuCount[i + 1] = candidateVarAccuCount[i] ;
		}
	}
public:
	VariantCaller(SeqSet &inRefSeq):refSet(inRefSeq) 
	{
		int i, j ;
		int seqCnt = refSet.Size() ;
		baseVariants.resize(seqCnt) ;
		for (i = 0 ; i < seqCnt ; ++i)
		{
			int len = refSet.GetSeqConsensusLen(i) ;
			baseVariants[i].ExpandTo(len) ;
			baseVariants[i].SetZero(0, len) ;
			for (j = 0 ; j < len ; ++j)
			{
				baseVariants[i][j].exon = refSet.IsPosInExon(i, j) ;
				baseVariants[i][j].candidateId = -1 ;
			}
		}
	} 
	~VariantCaller() {} 

	void SetSeqAbundance(Genotyper &genotyper) 
	{
		int seqCnt = refSet.Size() ;
		int i ;
		seqAbundance.resize(seqCnt) ;
		for (i = 0 ; i < seqCnt ; ++i)
			seqAbundance[i] = genotyper.GetAlleleAbundance(i) ;
	}

	void UpdateBaseVariantFromFragmentOverlap(char *read1, char *read2, std::vector<struct _fragmentOverlap> &fragmentAssignment)
	{
		int i ;
		int assignCnt = fragmentAssignment.size() ;
		
		double totalWeight = 0 ;
		for (i = 0 ; i < assignCnt ; ++i)
			totalWeight += seqAbundance[fragmentAssignment[i].seqIdx] ;
	
		for (i = 0 ; i < assignCnt ; ++i)
		{
			struct _fragmentOverlap &fragOverlap = fragmentAssignment[i] ;
			int seqIdx = fragOverlap.seqIdx ;
			double weight = seqAbundance[seqIdx] / totalWeight ;

			if (fragOverlap.hasMatePair)
			{
				UpdateBaseVariantFromOverlap(read1, weight, fragOverlap.overlap1) ;
				UpdateBaseVariantFromOverlap(read2, weight, fragOverlap.overlap2) ;
			}
			else
			{
				if (!fragOverlap.o1FromR2)
					UpdateBaseVariantFromOverlap(read1, weight, fragOverlap.overlap1) ;
				else
					UpdateBaseVariantFromOverlap(read2, weight, fragOverlap.overlap1) ;
			}
		}
	}

	void FindCandidateVariants()
	{
		int i, j, k ;
		candidateVariants.Clear() ;  		
		int seqCnt = refSet.Size() ;
		const int countThreshold = 2 ;

		for (i = 0 ; i < seqCnt ; ++i)
		{
			int len = baseVariants[i].Size() ;
			const char *s = refSet.GetSeqConsensus(i) ;
			for (j = 0 ; j < len ; ++j)
			{
				for (k = 0 ; k < 4 ; ++k)
				{
					if (baseVariants[i][j].count[k] >= countThreshold 
							&& baseVariants[i][j].count[k] >= baseVariants[i][j].count[ nucToNum[ s[j] - 'A' ]]
							&& k != nucToNum[s[j] - 'A'])
					{
						int id = candidateVariants.Size() ;
						struct _pair np ;
						np.a = i ;
						np.b = j ;	
						candidateVariants.PushBack(np) ;
						baseVariants[i][j].candidateId = id ;
						candidateVariantGroup.PushBack(id) ;
						//printf("%lf %lf\n", baseVariants[i][j].AllCountSum(), baseVariants[i][j].count[k]) ;
						break ;
					}
				}
			}
		}
	}

	void ExpandCandidateVariantsFromFragmentOverlap(char *read1, char *read2, std::vector<struct _fragmentOverlap> &fragmentAssignment, std::vector<SimpleVector<int> > &seqCandidateAccuCount)
	{
		if (fragmentAssignment.size() <= 0)
			return ;

		int i, j, k ;

		SimpleVector<int> refPos, readPos ;
		SimpleVector<int> alignIdx ;
		SimpleVector<char *> r ;
		int assignCnt = fragmentAssignment.size() ;

		refPos.ExpandTo(assignCnt) ;
		readPos.ExpandTo(assignCnt) ;
		r.ExpandTo(assignCnt) ;
		alignIdx.ExpandTo(assignCnt) ;
		for (k = 0 ; k <= 1 ; ++k) // 0-read1, 1-read2
		{
			// Check whether there is variants in the alignment region
			if (k == 1 && !fragmentAssignment[0].hasMatePair)
				break ;

			for (i = 0 ; i < assignCnt ; ++i)
			{
				int seqIdx = fragmentAssignment[i].seqIdx ;
				struct _overlap o = SelectOverlapFromFragmentOverlap(k, fragmentAssignment[i]) ;
				if (containCandidateVar(o.seqStart, o.seqEnd, seqCandidateAccuCount[seqIdx])) ;
					break ;
			}

			if (i >= assignCnt) // no candidate variants
				continue ;
			
			char *read = read1 ;
			if (k == 1
					|| (k == 0 && fragmentAssignment[0].o1FromR2))
				read = read2 ;
			int len = strlen(read) ;
			//char *rc = (char *)malloc(sizeof(char) * (len + 1)) ;
			//refSet.ReverseComplement(rc, read, len) ;
			for (i = 0 ; i < assignCnt ; ++i) 
			{
				int seqIdx = fragmentAssignment[i].seqIdx ;
				struct _overlap o = SelectOverlapFromFragmentOverlap(k, fragmentAssignment[i]) ;
				/*if (o.strand == 1)
					r[i] = read ;
				else if (o.strand == -1)
					r[i] = rc ;*/
				refPos[i] = o.seqStart ;
				readPos[i] = o.readStart ;
			}	
			///free(rc) ;
			
			alignIdx.SetZero(0, assignCnt) ;
			for (j = 0 ; j < len ; ++j) // use the read pos as the anchor
			{
				// Expand the set of candidate variants
				int firstCandidateId = -1 ;
				int firstCandidateIdx = -1 ;
				for (i = 0 ; i < assignCnt ; ++i)
				{
					struct _overlap o = SelectOverlapFromFragmentOverlap(k, fragmentAssignment[i]) ;
					if (refPos[i] < refSet.GetSeqConsensusLen(o.seqIdx) && baseVariants[o.seqIdx][refPos[i]].candidateId != -1)
					{
						firstCandidateId = baseVariants[o.seqIdx][refPos[i]].candidateId ;
						firstCandidateIdx = i ;
						break ;
					}
				}

				if (firstCandidateId != -1)
				{
					// contains candidate varivants
					for (i = 0 ; i < assignCnt ; ++i)
					{
						struct _overlap o = SelectOverlapFromFragmentOverlap(k, fragmentAssignment[i]) ;
						if (baseVariants[o.seqIdx][refPos[i]].candidateId == -1
								&& (o.align[alignIdx[i]] != -1 
									&& (o.align[ alignIdx[i] ] == EDIT_MATCH || o.align[ alignIdx[i] ] == EDIT_MISMATCH)))
						{
							int cid = candidateVariants.Size() ;
							struct _pair np ;
							np.a = o.seqIdx ;
							np.b = refPos[i] ;
							candidateVariants.PushBack(np) ;
							baseVariants[o.seqIdx][refPos[i]].candidateId = cid ;
							candidateVariantGroup.PushBack(cid) ;

							/*if (GetCandidateVariantGroup(firstCandidateId) == 0 )
							{
								printf("strange %d %d %d %d %d: %s %s\n", o.seqIdx, refPos[i], cid, k, o.strand, read1, read2) ;

								char *r = read ;
								char *rc = NULL ;
								if (o.strand == -1)
								{
									rc = strdup(r) ;
									refSet.ReverseComplement(rc, r, len) ;
									r = rc ;
								}
								AlignAlgo::VisualizeAlignment(refSet.GetSeqConsensus(o.seqIdx) + o.seqStart, o.seqEnd - o.seqStart + 1, r + o.readStart, o.readEnd - o.readStart + 1, o.align) ;
								//if (rc != NULL)
								//	free(rc) ;
								
								struct _overlap tmpo = SelectOverlapFromFragmentOverlap(k, fragmentAssignment[firstCandidateIdx]) ;
								r = read ;
								if (tmpo.strand == -1)
									r = rc ;
								printf("anchor %d %d %d %d\n", tmpo.seqIdx, refPos[firstCandidateIdx], k, o.strand) ;
								AlignAlgo::VisualizeAlignment(refSet.GetSeqConsensus(tmpo.seqIdx) + tmpo.seqStart, tmpo.seqEnd - tmpo.seqStart + 1, r + tmpo.readStart, tmpo.readEnd - tmpo.readStart + 1, tmpo.align) ;
								if (rc != NULL)
									free(rc) ;
							}*/
						}
						int cid = baseVariants[o.seqIdx][refPos[i]].candidateId ;
						if (cid != -1)
							candidateVariantGroup[cid] = GetCandidateVariantGroup(firstCandidateId) ;
					}
				}

				// Move to the next read position
				for (i = 0 ; i < assignCnt ; ++i)
				{
					struct _overlap o = SelectOverlapFromFragmentOverlap(k, fragmentAssignment[i]) ;
					char *align = o.align ;

					while (align[alignIdx[i]] != -1
							&& readPos[i] <= j)
					{
						int aidx = alignIdx[i] ;
						if (align[aidx] != EDIT_INSERT)
							++refPos[i] ;
						if (align[aidx] != EDIT_DELETE)
							++readPos[i] ;

						++alignIdx[i] ;
					}
				}
			} // for j on read position
		} // for k on read end selection
	}

	void BuildFragmentCandidateVarGraph(char *read1, char *read2, int fragIdx, std::vector<struct _fragmentOverlap> &fragmentAssignment, std::vector< SimpleVector<int> > &seqCandidateAccuCount, SimpleVector<struct _adjFragmentToBaseVariant> &adjFrag, SimpleVector<struct _adjBaseVariantToFragment> &adjVar)
	{
		int i, j, k ;
		int assignCnt = fragmentAssignment.size() ;
		if (assignCnt <= 0)
			return ;
		for (k = 0 ; k <= 1 ; ++k) // 0-read1, 1-read2
		{
			// Check whether there is variants in the alignment region
			if (k == 1 && !fragmentAssignment[0].hasMatePair)
				break ;

			for (i = 0 ; i < assignCnt ; ++i)
			{
				int seqIdx = fragmentAssignment[i].seqIdx ;
				struct _overlap o = SelectOverlapFromFragmentOverlap(k, fragmentAssignment[i]) ;
				if (containCandidateVar(o.seqStart, o.seqEnd, seqCandidateAccuCount[seqIdx])) ;
					break ;
			}

			if (i >= assignCnt) // no candidate variants
				continue ;
			
			char *read = read1 ;
			if (k == 1
					|| (k == 0 && fragmentAssignment[0].o1FromR2))
				read = read2 ;
			int len = strlen(read) ;
			char *rc = (char *)malloc(sizeof(char) * (len + 1)) ;
			refSet.ReverseComplement(rc, read, len) ;
			for (i = 0 ; i < assignCnt ; ++i) 
			{
				int seqIdx = fragmentAssignment[i].seqIdx ;
				struct _overlap o = SelectOverlapFromFragmentOverlap(k, fragmentAssignment[i]) ;
				char *r ;
				if (o.strand == 1)
					r = read ;
				else if (o.strand == -1)
					r = rc ;
				int refPos = o.seqStart ;
				int readPos = o.readStart ;
				char *align = o.align ;
				for (j = 0 ; align[j] != -1 ; ++j)
				{
					int cid = baseVariants[seqIdx][refPos].candidateId ;
					if (cid != -1)
					{
						char var[5] ;
						var[0] = r[readPos] ;
						var[1] = '\0' ;

						// Check whether the edge has already been put.
						int p ;
					
						p = adjVar[cid].next ;
						while (p != -1)
						{
							if (adjVar[p].fragIdx == fragIdx 
									&& !strcmp(var, adjVar[p].nuc))
								break ;
							p = adjVar[p].next ;
						}
						
						if (p == -1)
						{
							// Add the edge
							struct _adjFragmentToBaseVariant nFragToBaseVar ;
							struct _adjBaseVariantToFragment nBaseVarToFrag ;

							strcpy(nFragToBaseVar.nuc, var) ;
							nFragToBaseVar.seqIdx = seqIdx ;
							nFragToBaseVar.refPos = refPos ;
							nFragToBaseVar.weight = 1 ;
							nFragToBaseVar.next = adjFrag[fragIdx].next ;
							adjFrag[fragIdx].next = adjFrag.Size() ;
							adjFrag.PushBack(nFragToBaseVar) ;

							strcpy(nBaseVarToFrag.nuc, var) ;
							nBaseVarToFrag.fragIdx = fragIdx ;
							nBaseVarToFrag.next = adjVar[cid].next ;
							adjVar[cid].next = adjVar.Size() ;
							adjVar.PushBack(nBaseVarToFrag) ;
						}
					}
					if (align[j] != EDIT_INSERT)
						++refPos ;
					if (align[j] != EDIT_DELETE)
						++readPos ;
				}
			}	// for j on read position
			free(rc) ;
		} // for k on read end selection
	}	
	
	void EnumerateVariants(int depth, SimpleVector<char> &choices, struct _enumVarResult &result, SimpleVector<int> &fragIds, SimpleVector<int> &vars, SimpleVector<struct _adjFragmentToBaseVariant> &adjFrag, SimpleVector<struct _adjBaseVariantToFragment> &adjVar)
	{
		int i ;
		if (depth == vars.Size())
		{
			SimpleVector<bool> fragCovered ;
			int fragCnt = fragIds.Size() ;
			int varCnt = vars.Size() ;
			
			int tmp = 0 ;
			for (i = 0 ; i < fragCnt ; ++i)
				if (fragIds[i] > tmp)
					tmp = fragIds[i] ;

			fragCovered.ExpandTo(tmp) ;
			fragCovered.SetZero(0, tmp) ;
			for (i = 0 ; i < varCnt ; ++i)
			{
				int p = adjVar[ vars[i] ].next ;
				while (p != -1)
				{
					if (adjVar[p].nuc[0] == choices[i])
					{
						int fragIdx = adjVar[p].fragIdx ;
						fragCovered[fragIdx] = true ;
					}
					p = adjVar[p].next ;
				}
			}
			double covered = 0 ;
			for (i = 0 ; i < fragCnt ; ++i)
			{
				if (fragCovered[fragIds[i]])
					++covered ;
			}

			if (covered > result.bestCover)
			{
				result.bestCover = covered ;
				result.bestEnumVariants = choices ;
				result.equalBestEnumVariants.Clear() ;
			}
			else if (covered == result.bestCover)
			{
				result.equalBestEnumVariants = choices ;
			}
			return ;	
		}

		for (i = 0 ; i < 4 ; ++i)
		{
			choices[depth] = numToNuc[i] ;
			EnumerateVariants(depth + 1, choices, result, fragIds, vars, adjFrag, adjVar) ;
		}
	}

	void SolveVariantGroup(SimpleVector<int> vars, SimpleVector<struct _adjFragmentToBaseVariant> &adjFrag, SimpleVector<struct _adjBaseVariantToFragment> &adjVar)
	{
		int i ;
		SimpleVector<char> choices ;
		int varCnt = vars.Size() ;
		struct _enumVarResult result ;
		SimpleVector<int> fragIds ;
		std::map<int, int> fragUsed ;
		
		for (i = 0 ; i < varCnt ; ++i)
		{
			int seqIdx = candidateVariants[vars[i]].a ;
			int refPos = candidateVariants[vars[i]].b ;
			if (baseVariants[seqIdx][refPos].exon)
				break ;
		}
		if (i >= varCnt) // only compute for exons
			return ;
		choices.ExpandTo(varCnt) ;
		// Obtain related fragments
		for (i = 0 ; i < varCnt ; ++i)
		{
			int p = adjVar[ vars[i] ].next ;
			while (p != -1)
			{
				int fragIdx = adjVar[p].fragIdx ;
				if (fragUsed.find(fragIdx) == fragUsed.end())
				{
					fragUsed[fragIdx] = 1 ;
					fragIds.PushBack(fragIdx) ;
				}
				p = adjVar[p].next ;
			}
		}
		
		result.bestCover = -1 ;	
		EnumerateVariants(0, choices, result, fragIds, vars, adjFrag, adjVar) ;

		// Process the final results.
		bool uniq = true ;
		if (result.equalBestEnumVariants.Size() > 0)
			uniq = false ;

		for (i = 0 ; i < varCnt ; ++i)
		{
			int seqIdx = candidateVariants[vars[i]].a ;
			int refPos = candidateVariants[vars[i]].b ;
			if (!baseVariants[seqIdx][refPos].exon)
				continue ;
			char refNuc = refSet.GetSeqConsensus(seqIdx)[refPos] ;
			char varNuc = result.bestEnumVariants[i] ;
			if (refNuc == varNuc)
				continue ;

			struct _variant nv ;
			nv.seqIdx = seqIdx ;
			nv.refStart = refPos ;
			nv.refEnd = refPos ;
			nv.ref[0] = refNuc ;
			nv.ref[1] = '\0' ;
			nv.var[0] = varNuc ;
			nv.var[1] = '\0' ;
			nv.allSupport = baseVariants[seqIdx][refPos].AllCountSum() ;
			nv.varSupport = baseVariants[seqIdx][refPos].count[ nucToNum[varNuc - 'A'] ] ;
			nv.varUniqSupport = baseVariants[seqIdx][refPos].uniqCount[ nucToNum[varNuc - 'A'] ] ;
			if (uniq == false)
				nv.qual = 0 ;
			else
				nv.qual = 60 ;
			finalVariants.push_back(nv) ;
		}
	}

	void ComputeVariant(std::vector<char *> &read1, std::vector<char *> &read2, std::vector< std::vector<struct _fragmentOverlap> > &fragmentAssignments)
	{
		int fragCnt = fragmentAssignments.size() ;
		int seqCnt = refSet.Size() ;
		int i ;

		// Identify the preliminary set of candidate variants
		for (i = 0 ; i < fragCnt ; ++i)	
		{
			if (read2.size() > 0)
				UpdateBaseVariantFromFragmentOverlap(read1[i], read2[i], fragmentAssignments[i]) ;
			else
				UpdateBaseVariantFromFragmentOverlap(read1[i], NULL, fragmentAssignments[i]) ;
		}
		FindCandidateVariants() ;
		
		/*for (i = 0 ; i < candidateVariants.Size() ; ++i)
		{
			printf("%d %s %d %d\n", i, refSet.GetSeqName(candidateVariants[i].a), candidateVariants[i].b,
					candidateVariantGroup[i]) ;
		}*/	
		// Identity the candidate variants on other sequences aligned with preliminary
		// candidate variants
		std::vector< SimpleVector<int> > seqCandidateVarAccuCount ; // useful to quickly determine whether there is a overlap of the read and candidate variations.
		seqCandidateVarAccuCount.resize(seqCnt) ;		
		while (1) 
		{	
			int prevCandidateVarCount = candidateVariants.Size() ;
			for (i = 0 ; i < seqCnt ; ++i)
				ComputeCandidateVarAccuCount(i, seqCandidateVarAccuCount[i]) ;

			for (i = 0 ; i < fragCnt ; ++i)
			{
				if (read2.size() > 0)
					ExpandCandidateVariantsFromFragmentOverlap(read1[i], read2[i], 
							fragmentAssignments[i], seqCandidateVarAccuCount ) ;
				else
					ExpandCandidateVariantsFromFragmentOverlap(read1[i], NULL, 
							fragmentAssignments[i], seqCandidateVarAccuCount ) ;
			}
			if (prevCandidateVarCount == candidateVariants.Size())
				break ;
		}
		
		//for (i = 0 ; i < seqCnt ; ++i)
		//	ComputeCandidateVarAccuCount(i, seqCandidateVarAccuCount[i]) ;

		// Build the relation of fragments and variants
		int candidateVarCnt = candidateVariants.Size() ;
		//struct _adjFragmentToBaseVariant *adjFrag = (struct _adjFragmentToBaseVariant*)malloc(sizeof(struct _adjFragmentToBaseVariant) * fragCnt) ;
		//struct _adjBaseVariantToFragment *adjVar = (struct _adjBaseVariantToFragment*)malloc(sizeof(_adjBaseVariantToFragment) * candidateVarCnt) ;

		SimpleVector<struct _adjFragmentToBaseVariant> adjFrag ; 
		SimpleVector<struct _adjBaseVariantToFragment> adjVar ; 
		adjFrag.Reserve(fragCnt + candidateVarCnt) ;
		adjFrag.Resize(fragCnt) ;
		adjVar.Reserve(fragCnt + candidateVarCnt) ;
		adjVar.Resize(candidateVarCnt) ;

		/*for (i = 0 ; i < candidateVarCnt ; ++i)
		{
			printf("%d %d %s %d %d\n", i, candidateVariants[i].a, refSet.GetSeqName(candidateVariants[i].a), candidateVariants[i].b,
					candidateVariantGroup[i]) ;
		}*/
		
		for (i = 0 ; i < fragCnt ; ++i)
			adjFrag[i].next = -1 ;
		for (i = 0 ; i < candidateVarCnt ; ++i)
			adjVar[i].next = -1 ;

		for (i = 0 ; i < fragCnt ; ++i)
		{
			if (read2.size() > 0)
				BuildFragmentCandidateVarGraph(read1[i], read2[i], i, 
						fragmentAssignments[i], seqCandidateVarAccuCount, adjFrag, adjVar) ;	
			else
				BuildFragmentCandidateVarGraph(read1[i], NULL, i, 
						fragmentAssignments[i], seqCandidateVarAccuCount, adjFrag, adjVar) ;	
		}

		// Group variants
		std::map<int, int> reducedCandidateVarGroup ; // father to a number.
		std::vector< SimpleVector<int> > candidateVarGroup ;
		for (i = 0 ; i < candidateVarCnt ; ++i)
		{
			int gid = GetCandidateVariantGroup(i) ;
			if (reducedCandidateVarGroup.find(gid) == reducedCandidateVarGroup.end())
			{
				reducedCandidateVarGroup[gid] = candidateVarGroup.size() ;
				SimpleVector<int> nv ;
				candidateVarGroup.push_back(nv) ;
			}
			int reducedGid = reducedCandidateVarGroup[gid] ;
			candidateVarGroup[reducedGid].PushBack(i) ;
		}

		// Solve each group
		int reducedGroupCnt = candidateVarGroup.size() ;
		for ( i = 0 ; i < reducedGroupCnt ; ++i)
		{
			SolveVariantGroup(candidateVarGroup[i], adjFrag, adjVar) ;
			//break ;
		}
	}

	/*int GetSeqExonVariants(int seqIdx, std::vector<struct _variant> &variants)
	{
		int i, j, k ;
		const struct _seqWrapper &seq = seqs[seqIdx] ;
		k = 0 ;
		for (i = 0 ; i < seq.consensusLen ; ++i)
		{
			// major allele 
			if (!seq.isValidDiff[i].exon)
				continue ;
			double max = seq.baseVariants[i].count[0];
			int maxTag = 0 ;
			for (j = 1 ; j < 4 ; ++j)
			{
				if (seq.baseVariants[i].count[j] > max)
				{
					max = seq.baseVariants[i].count[j] ;
					maxTag = j ;
				}
			}
			//if (max == 0)
			//	printf("%s %d\n", seqs[seqIdx].name, i) ;
			if (numToNuc[maxTag] != seq.consensus[i] && max > 0)
			{
				// Variation happens
				struct _variant nv ;
				nv.seqIdx = seqIdx ;
				nv.refStart = k ;
				nv.refEnd = k ;
				nv.ref[0] = seq.consensus[i] ;
				nv.ref[1] = '\0' ;
				nv.var[0] = numToNuc[maxTag] ;
				nv.var[1] = '\0' ;
				nv.allSupport = seq.baseVariants[i].AllCountSum() ;
				nv.varSupport = max ;
				nv.varUniqSupport = seq.baseVariants[i].uniqCount[maxTag] ;
				variants.push_back(nv) ;
			}
			++k ;
		}
	}*/

	void ConvertVariantsToExonCoord()
	{
		
	}
	
	void OutputAlleleVCF(char *filename)
	{
		FILE *fp = fopen(filename, "w") ;
		int i ;
		int varCnt = finalVariants.size() ;	
		char buffer[10] = "PASS" ;	
		for (i = 0 ; i < varCnt ; ++i)
		{
			struct _variant &variant = finalVariants[i] ;
			if (variant.qual > 0)
				strcpy(buffer, "PASS") ;
			else
				strcpy(buffer, "FAIL") ;
			fprintf(fp, "%s %d . %s %s %s %lf %lf %lf\n", 
					refSet.GetSeqName(variant.seqIdx), variant.refStart + 1, // the VCF file is 1-based
					variant.ref, variant.var, buffer, 
					variant.varSupport, variant.allSupport,
					variant.varUniqSupport) ;
		}
		fclose(fp) ;
	}
} ;

#endif
