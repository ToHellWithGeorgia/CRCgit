#include "replacement_state.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This file is distributed as part of the Cache Replacement Championship     //
// workshop held in conjunction with ISCA'2010.                               //
//                                                                            //
//                                                                            //
// Everyone is granted permission to copy, modify, and/or re-distribute       //
// this software.                                                             //
//                                                                            //
// Please contact Aamer Jaleel <ajaleel@gmail.com> should you have any        //
// questions                                                                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/*
** This file implements the cache replacement state. Users can enhance the code
** below to develop their cache replacement ideas.
**
*/


////////////////////////////////////////////////////////////////////////////////
// The replacement state constructor:                                         //
// Inputs: number of sets, associativity, and replacement policy to use       //
// Outputs: None                                                              //
//                                                                            //
// DO NOT CHANGE THE CONSTRUCTOR PROTOTYPE                                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
CACHE_REPLACEMENT_STATE::CACHE_REPLACEMENT_STATE( UINT32 _sets, UINT32 _assoc, UINT32 _pol )
{

    numsets    = _sets;
    assoc      = _assoc;
    replPolicy = _pol;

    mytimer    = 0;

    InitReplacementState();
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function initializes the replacement policy hardware by creating      //
// storage for the replacement state on a per-line/per-cache basis.           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::InitReplacementState()
{
    // Create the state for sets, then create the state for the ways
    repl  = new LINE_REPLACEMENT_STATE* [ numsets ];

    // ensure that we were able to create replacement state
    assert(repl);

    if (this->replPolicy == CRC_REPL_SRRIP) {
        hitpolicy = 0; //Use hit RRPV to 0 as default
        RRIP_MAX = 4; //0,1,2,3
    }
    else if (this->replPolicy == CRC_REPL_DRRIP) {
        hitpolicy = 0; 
        RRIP_MAX = 4; 
    }
    else if (this->replPolicy == CRC_REPL_SHiP)
    {
        RRIP_MAX = 4;
        hitpolicy = 0;
    }
    else if (this->replPolicy == CRC_REPL_EAF)
    {
        RRIP_MAX = 4;
        hitpolicy = 0;
    }

    // for DRRIP    
    stat_DRRIP_BI = 0;
    stat_DRRIP_SI = 0;
    stat_DRRIP_SL = 0;
    stat_DRRIP_BL = 0;
    NumLeaderSets = 32; // as shown on the paper
    BRRIP_rate = 16;
    PSEL_MAX = 1024;
    PSEL = PSEL/2; //starting from a mid point

    // for SHiP
    NumSHCTEntries = 16*1024; 
    NumSigBits = 14;
    NumSHCTCtrBits = 3; 
    // set up the SHCTable
    SHCT = new UINT32[NumSHCTEntries];
    for (UINT32 ii = 0; ii < NumSHCTEntries; ii++)
    {
        SHCT[ii] = 0;
    }
    stat_SHiP_BI = 0;
    stat_SHiP_GI = 0;

    // for EAF
    Alpha = 8;
    NumEAFEntry = 8 * 1024 * 16; // m = alpha * #cacheblocks (alpha = 8) 
    AddrCounter = 0; // counter of number of addresses
    EAF = new UINT32[NumEAFEntry];
    for (UINT32 ii = 0; ii < NumEAFEntry; ii++)
    {
        EAF[ii] = 0;
    }
    NumHash = 2;
    // Create the hash table (2^64 --> 2^17 space)
    // To implement H3 we need 2 (64 * 17) tables.
    // which means we need 64 random 2^17 numbers for each tables.
    Hash_a = new UINT32[64];
    Hash_b = new UINT32[64];

    for(UINT32 ii = 0; ii < 64; ii++)
    {
        Hash_a[ii] = rand()% (32576 * 4); // since the maximum pseduo random number is 32576 which 2^15 we need 2^17
    }
    for(UINT32 ii = 0; ii < 64; ii++)
    {
        Hash_b[ii] = rand()% (32576 * 4); // since the maximum pseduo random number is 32576 which 2^15 we need 2^17
    }
    stat_EAF_SBI = 0; //EAF bad insert static
    stat_EAF_SGI = 0; //EAF good insert static

    stat_EAF_BBI = 0; //EAF bad insert bypass
    stat_EAF_BGI = 0; //EAF good insert bypass

    stat_EAF_LSI = 0; //leader set static insert
    stat_EAF_LBI = 0; //leader set bypass insert

    // Create the state for the sets
    for(UINT32 setIndex=0; setIndex<numsets; setIndex++) 
    {
        repl[ setIndex ]  = new LINE_REPLACEMENT_STATE[ assoc ];

        for(UINT32 way=0; way<assoc; way++) 
        {
            // initialize stack position (for true LRU)
            repl[ setIndex ][ way ].LRUstackposition = way;
            // for SRRIP
            repl[ setIndex ][ way ].RRPV = RRIP_MAX - 1;
            // for SHiP
            repl[ setIndex ][ way ].signature_m = 0;
            repl[ setIndex ][ way ].outcome = false;
        }
    }

    // Contestants:  ADD INITIALIZATION FOR YOUR HARDWARE HERE

}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache on every cache miss. The input        //
// arguments are the thread id, set index, pointers to ways in current set    //
// and the associativity.  We are also providing the PC, physical address,    //
// and accesstype should you wish to use them at victim selection time.       //
// The return value is the physical way index for the line being replaced.    //
// Return -1 if you wish to bypass LLC.                                       //
//                                                                            //
// vicSet is the current set. You can access the contents of the set by       //
// indexing using the wayID which ranges from 0 to assoc-1 e.g. vicSet[0]     //
// is the first way and vicSet[4] is the 4th physical way of the cache.       //
// Elements of LINE_STATE are defined in crc_cache_defs.h                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::GetVictimInSet( UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc,
                                               Addr_t PC, Addr_t paddr, UINT32 accessType )
{
    // If no invalid lines, then replace based on replacement policy
    if( replPolicy == CRC_REPL_LRU ) 
    {
        return Get_LRU_Victim( setIndex );
    }
    else if( replPolicy == CRC_REPL_RANDOM )
    {
        return Get_Random_Victim( setIndex );
    }
    else if( replPolicy == CRC_REPL_SRRIP )
    {
        return Get_SRRIP_Victim( setIndex );
    }
    else if( replPolicy == CRC_REPL_DRRIP )
    {
        return Get_SRRIP_Victim( setIndex ); // the victim finding policy is the same as SRRIP
    }
    else if( replPolicy == CRC_REPL_SHiP )
    {
        return Get_SRRIP_Victim( setIndex ); // still the same
    }
    else if( replPolicy == CRC_REPL_EAF )
    {
        return Get_EAF_Victim( setIndex, vicSet ); //need the vicset to update the EAF
    }
    else if( replPolicy == CRC_REPL_CUSTOM )
    {
        // Contestants:  ADD YOUR VICTIM SELECTION FUNCTION HERE
    } 

    // We should never get here
    assert(0);

    return -1; // Returning -1 bypasses the LLC
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache after every cache hit/miss            //
// The arguments are: the set index, the physical way of the cache,           //
// the pointer to the physical line (should contestants need access           //
// to information of the line filled or hit upon), the thread id              //
// of the request, the PC of the request, the accesstype, and finall          //
// whether the line was a cachehit or not (cacheHit=true implies hit)         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::UpdateReplacementState( 
    UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine, 
    UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit )
{
    // What replacement policy?
    if( replPolicy == CRC_REPL_LRU ) 
    {
        UpdateLRU( setIndex, updateWayID );
    }
    else if( replPolicy == CRC_REPL_RANDOM )
    {
        // Random replacement requires no replacement state update
    }
    else if( replPolicy == CRC_REPL_SRRIP )
    {
        UpdateSRRIP( setIndex, updateWayID, cacheHit );
    }
    else if( replPolicy == CRC_REPL_DRRIP )
    {
        UpdateDRRIP( setIndex, updateWayID, cacheHit );
    }
    else if( replPolicy == CRC_REPL_SHiP )
    {
        UpdateSHiP( setIndex, updateWayID, cacheHit, PC);
    }
    else if( replPolicy == CRC_REPL_EAF )
    {
        UpdateEAF( setIndex, updateWayID, cacheHit, currLine);
    }
    else if( replPolicy == CRC_REPL_CUSTOM )
    {
        
    }
    
    
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//////// HELPER FUNCTIONS FOR REPLACEMENT UPDATE AND VICTIM SELECTION //////////
//                                                                            //
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds the LRU victim in the cache set by returning the       //
// cache block at the bottom of the LRU stack. Top of LRU stack is '0'        //
// while bottom of LRU stack is 'assoc-1'                                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_LRU_Victim( UINT32 setIndex )
{
    // Get pointer to replacement state of current set
    LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];

    INT32   lruWay   = 0;

    // Search for victim whose stack position is assoc-1
    for(UINT32 way=0; way<assoc; way++) 
    {
        if( replSet[way].LRUstackposition == (assoc-1) ) 
        {
            lruWay = way;
            break;
        }
    }

    // return lru way
    return lruWay;
}

INT32 CACHE_REPLACEMENT_STATE::Get_SRRIP_Victim( UINT32 setIndex )
{
    LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];

    INT32   FoundWay   = -1;

    while(1) {
        for(UINT32 way=0; way<assoc; way++) 
        {
            if( replSet[way].RRPV == RRIP_MAX - 1 ) 
            {
                FoundWay = way;
                break;
            }
        }
        if (FoundWay >= 0) break;
        // else increment all the counter
        for(UINT32 way=0; way<assoc; way++) {
            replSet[way].RRPV ++;
        }
    }
    

    return FoundWay;
}

INT32 CACHE_REPLACEMENT_STATE::Get_EAF_Victim( UINT32 setIndex , const LINE_STATE *vicSet)
{
    // find the way using SRRIP victim first.
    INT32 FoundWay = Get_SRRIP_Victim(setIndex);
    // Need to update the EAF here
    if(vicSet[FoundWay].valid)
    {
        Addr_t memaddr = (((vicSet[FoundWay].tag)*numsets)<<6) + (setIndex<<6);
        UINT32 hash_a = EAF_hash_a(memaddr);
        UINT32 hash_b = EAF_hash_b(memaddr);
        EAF[hash_a] = 1;
        EAF[hash_b] = 1;
        // increment the counter and reset if saturated.
        AddrCounter++;
    }
    
    if (AddrCounter >= 16 * 1024)
    {
        AddrCounter = 0;
        for(UINT32 ii = 0;ii < NumEAFEntry; ii++)
        {
            EAF[ii] = 0;
        }
    }
    return FoundWay;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds a random victim in the cache set                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_Random_Victim( UINT32 setIndex )
{
    INT32 way = (rand() % assoc);
    
    return way;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function implements the LRU update routine for the traditional        //
// LRU replacement policy. The arguments to the function are the physical     //
// way and set index.                                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::UpdateLRU( UINT32 setIndex, INT32 updateWayID )
{
    // Determine current LRU stack position
    UINT32 currLRUstackposition = repl[ setIndex ][ updateWayID ].LRUstackposition;

    // Update the stack position of all lines before the current line
    // Update implies incremeting their stack positions by one
    for(UINT32 way=0; way<assoc; way++) 
    {
        if( repl[setIndex][way].LRUstackposition < currLRUstackposition ) 
        {
            repl[setIndex][way].LRUstackposition++;
        }
    }

    // Set the LRU stack position of new line to be zero
    repl[ setIndex ][ updateWayID ].LRUstackposition = 0;
}

void CACHE_REPLACEMENT_STATE::UpdateSRRIP( UINT32 setIndex, INT32 updateWayID, bool cacheHit )
{
    // Below are SRRIP status update.
    // if hit change the RRPV depending on the policy
    LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
    if (cacheHit)
    {
        if(hitpolicy)
        {
            if (replSet[updateWayID].RRPV>0) replSet[updateWayID].RRPV--;
        }
        else replSet[updateWayID].RRPV = 0;
    }
    else
    {
        replSet[updateWayID].RRPV = RRIP_MAX - 2;
    }


}


void CACHE_REPLACEMENT_STATE::UpdateBRRIP( UINT32 setIndex, INT32 updateWayID, bool cacheHit )
{
    // Below are BRRIP status update.
    // if hit change the RRPV depending on the policy
    LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
    if (cacheHit)
    {
        if(hitpolicy)
        {
            if (replSet[updateWayID].RRPV>0) replSet[updateWayID].RRPV--;
        }
        else replSet[updateWayID].RRPV = 0;
    }
    else // if MISS install on a 1/freq chance to RRIP_MAX - 3
    {
        UINT32 randnum = rand() % BRRIP_rate; // rand 0 ~ freq-1
        if (randnum == BRRIP_rate-1) 
        {
            replSet[updateWayID].RRPV = RRIP_MAX - 2; // infrequent pattern
        }
        else 
        {
            replSet[updateWayID].RRPV = RRIP_MAX - 1; // frequent pattern
        }
        
    }


}

void CACHE_REPLACEMENT_STATE::UpdateDRRIP( UINT32 setIndex, INT32 updateWayID, bool cacheHit )
{
    // Below are DRRIP status update.
    // Set dueling for the selecting sets.
    // First for the leader sets.
    // default settings for L3 Cache is 1024 sets (1MB 64B Line 16 Associativity) and we choose 32 sets for leader sets
    // According to the BIP paper http://researcher.watson.ibm.com/researcher/files/us-moinqureshi/papers-dip.pdf
    // Every 0 and 33rd sets are dedicated to SRRIP
    // Every 31st set is dedicated to BRRIP
    // Remaining are the follower sets
    if (((setIndex % 33) == 0) && (setIndex < NumLeaderSets*33)) // leader sets for SRRIP PSEL-- if miss
    {
        UpdateSRRIP(setIndex, updateWayID, cacheHit);
        if(!cacheHit){
            if(PSEL > 0) PSEL--;
            stat_DRRIP_BL++;
        }
    }
    else if(((setIndex%31)==0) && (setIndex > 0) && (setIndex<=31*NumLeaderSets)) // leader sets for BRRIP PSEL++ if miss
    {
        UpdateBRRIP(setIndex, updateWayID, cacheHit);
        if(!cacheHit){
            if(PSEL < PSEL_MAX) PSEL++;
            stat_DRRIP_SL++;
        }
    }
    else if(PSEL >= PSEL_MAX/2) //follower sets (SRRIP wins)
    {
        UpdateSRRIP(setIndex, updateWayID, cacheHit);
        if(!cacheHit) stat_DRRIP_SI++;
    }
    else if(PSEL < PSEL_MAX/2) //follower sets (BRRIP wins)
    {
        UpdateBRRIP(setIndex, updateWayID, cacheHit);
        if(!cacheHit) stat_DRRIP_BI++;
    }

}

void   CACHE_REPLACEMENT_STATE::UpdateSHiP( UINT32 setIndex, INT32 updateWayID, bool cacheHit,  Addr_t PC) 
{
    LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
    // Find the Hash entry first 
    // Use the 2~15 bit of PC as HASH entry
    UINT32 SHCTindex = PC;
    SHCTindex = SHCTindex >> 2;
    SHCTindex = SHCTindex & ((1 << NumSigBits) - 1);

    UINT32 indmax = 1 << NumSigBits;
    assert(SHCTindex < indmax);
    // end
    UINT32 SHCT_MAX = 1 << NumSHCTCtrBits;
    UINT32 currsig =  replSet[updateWayID].signature_m;

    if (cacheHit)
    {
        replSet[updateWayID].outcome = true;
        if(SHCT[SHCTindex] <= SHCT_MAX) SHCT[SHCTindex]++;

        replSet[updateWayID].RRPV = 0;
    }
    else 
    {
        //decrement SHCT counter
        if (!replSet[updateWayID].outcome)
        {
            if(SHCT[currsig] > 0) SHCT[currsig]--;
        }

        replSet[updateWayID].outcome = false;
        replSet[updateWayID].signature_m = SHCTindex;

        //insert based on signature
        if (SHCT[SHCTindex] == 0)
        {
            replSet[updateWayID].RRPV = RRIP_MAX - 1;
            stat_SHiP_BI ++;
        }
        else
        {
            replSet[updateWayID].RRPV = RRIP_MAX - 2;
            stat_SHiP_GI ++;
        }
    }

}

UINT32   CACHE_REPLACEMENT_STATE::EAF_hash_a (Addr_t memaddr)
{   
    // basically we we XOR the row where the bit of memaddr=1;
    UINT32 base = 0;
    Addr_t datacopy = memaddr;
    for(UINT32 ii = 0; ii< 64; ii++)
    {
        if(datacopy%2) base = base ^ Hash_a[ii];
        datacopy = datacopy >> 1;
    }
    return base;
}

UINT32   CACHE_REPLACEMENT_STATE::EAF_hash_b (Addr_t memaddr)
{
    // basically we we XOR the row where the bit of memaddr=1;
    UINT32 base = 0;
    Addr_t datacopy = memaddr;
    for(UINT32 ii = 0; ii< 64; ii++)
    {
        if(datacopy%2) base = base ^ Hash_b[ii];
        datacopy = datacopy >> 1;
    }
    return base;
}

void   CACHE_REPLACEMENT_STATE::UpdateSEAF( UINT32 setIndex, INT32 updateWayID, bool cacheHit,const LINE_STATE *currLine )
{
    // if hit decrement the RRPV to 0;
    Addr_t memaddr = (((currLine->tag)*numsets)<<6) + (setIndex<<6);
    LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
    if (cacheHit)
    {
        if(hitpolicy)
        {
            if (replSet[updateWayID].RRPV>0) replSet[updateWayID].RRPV--;
        }
        else replSet[updateWayID].RRPV = 0;
    }
    else // if miss try to find the EAF to determine the insert position
    {
        if ((EAF[EAF_hash_a(memaddr)]) && (EAF[EAF_hash_b(memaddr)]))
        {
            replSet[updateWayID].RRPV = RRIP_MAX - 2;
            stat_EAF_SGI++;
        }
        else
        {
            replSet[updateWayID].RRPV = RRIP_MAX - 1;
            stat_EAF_SBI++;
        }
    }

}

void   CACHE_REPLACEMENT_STATE::UpdateBEAF( UINT32 setIndex, INT32 updateWayID, bool cacheHit,const LINE_STATE *currLine )
{
    // if hit decrement the RRPV to 0;
    Addr_t memaddr = (((currLine->tag)*numsets)<<6) + (setIndex<<6);
    LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
    if (cacheHit)
    {
        if(hitpolicy)
        {
            if (replSet[updateWayID].RRPV>0) replSet[updateWayID].RRPV--;
        }
        else replSet[updateWayID].RRPV = 0;
    }
    else // if miss try to find the EAF to determine the insert position
    {
        if ((EAF[EAF_hash_a(memaddr)]) && (EAF[EAF_hash_b(memaddr)]) && (rand()%10 <= 2))
        {
            replSet[updateWayID].RRPV = RRIP_MAX - 2;
            stat_EAF_BGI++;
        }
        else
        {
            replSet[updateWayID].RRPV = RRIP_MAX - 1;
            stat_EAF_BBI++;
        }
    }

}

void   CACHE_REPLACEMENT_STATE::UpdateEAF( UINT32 setIndex, INT32 updateWayID, bool cacheHit,const LINE_STATE *currLine )
{
    if (((setIndex % 33) == 0) && (setIndex < NumLeaderSets*33)) // leader sets for SEAF PSEL-- if miss
    {
        UpdateSEAF(setIndex, updateWayID, cacheHit, currLine);
        if(!cacheHit){
            if(PSEL > 0) PSEL--;
            stat_EAF_LSI++;
        }
    }
    else if(((setIndex%31)==0) && (setIndex > 0) && (setIndex<=31*NumLeaderSets)) // leader sets for BEAF PSEL++ if miss
    {
        UpdateBEAF(setIndex, updateWayID, cacheHit, currLine);
        if(!cacheHit){
            if(PSEL < PSEL_MAX) PSEL++;
            stat_EAF_LBI++;
        }
    }
    else if(PSEL >= PSEL_MAX/2) //follower sets (SRRIP wins)
    {
        UpdateSEAF(setIndex, updateWayID, cacheHit, currLine);

    }
    else if(PSEL < PSEL_MAX/2) //follower sets (BRRIP wins)
    {
        UpdateBEAF(setIndex, updateWayID, cacheHit, currLine);

    }




    /*
    // if hit decrement the RRPV to 0;
    Addr_t memaddr = (((currLine->tag)*numsets)<<6) + (setIndex<<6);
    LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
    if (cacheHit)
    {
        if(hitpolicy)
        {
            if (replSet[updateWayID].RRPV>0) replSet[updateWayID].RRPV--;
        }
        else replSet[updateWayID].RRPV = 0;
    }
    else // if miss try to find the EAF to determine the insert position
    {
        if ((EAF[EAF_hash_a(memaddr)]) && (EAF[EAF_hash_b(memaddr)]) && (rand()%10 <= 2))
        {
            replSet[updateWayID].RRPV = RRIP_MAX - 2;
            stat_EAF_GI++;
        }
        else
        {
            replSet[updateWayID].RRPV = RRIP_MAX - 1;
            stat_EAF_BI++;
        }
    }
    */

}


////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// The function prints the statistics for the cache                           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
ostream & CACHE_REPLACEMENT_STATE::PrintStats(ostream &out)
{

    out<<"=========================================================="<<endl;
    out<<"=========== Replacement Policy Statistics ================"<<endl;
    out<<"=========================================================="<<endl;

    // CONTESTANTS:  Insert your statistics printing here

    out<<"leader sets using SRRIP:    "<<stat_DRRIP_SL<<endl;
    out<<"leader sets using BRRIP:    "<<stat_DRRIP_BL<<endl;
    out<<"Following sets using SRRIP: "<<stat_DRRIP_SI<<endl;
    out<<"Following sets using BRRIP: "<<stat_DRRIP_BI<<endl;
    out<<"=================SHiP======================="<<endl;
    out<<"SHiP GOOD INSERT: "<<stat_SHiP_GI<<endl;
    out<<"SHiP BAD  INSERT: "<<stat_SHiP_BI<<endl;
    out<<"=================EAF======================="<<endl;
    out<<"EAF Leader Static INSERT: "<<stat_EAF_LSI<<endl;
    out<<"EAF Leader Bypass INSERT: "<<stat_EAF_LBI<<endl;

    out<<"EAF GOOD INSERT Static: "<<stat_EAF_SGI<<endl;
    out<<"EAF BAD  INSERT Static: "<<stat_EAF_SBI<<endl;

    out<<"EAF GOOD INSERT Bypass: "<<stat_EAF_BGI<<endl;
    out<<"EAF BAD  INSERT Bypass: "<<stat_EAF_BBI<<endl;

    out<<"=========================================================="<<endl;
    out<<"=========== Replacement Policy Stat END   ================"<<endl;
    out<<"=========================================================="<<endl;
    return out;
    
}

