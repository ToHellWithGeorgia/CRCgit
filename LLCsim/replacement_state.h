#ifndef REPL_STATE_H
#define REPL_STATE_H

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

#include <cstdlib>
#include <cassert>
#include <iostream>
#include "utils.h"
#include "crc_cache_defs.h"

// Replacement Policies Supported
typedef enum 
{
    CRC_REPL_LRU        = 0,
    CRC_REPL_RANDOM     = 1,
    CRC_REPL_SRRIP      = 2,
    CRC_REPL_DRRIP      = 3,
    CRC_REPL_SHiP       = 4,
    CRC_REPL_EAF        = 5,
    CRC_REPL_CUSTOM     = 6
} ReplacemntPolicy;

// Replacement State Per Cache Line
typedef struct
{
    UINT32  LRUstackposition;
    // for DRRIP
    UINT32  RRPV;   //re-reference prediction value
    // for SHiP
    UINT32  signature_m;
    bool    outcome;

    // CONTESTANTS: Add extra state per cache line here

} LINE_REPLACEMENT_STATE;


// The implementation for the cache replacement policy
class CACHE_REPLACEMENT_STATE
{

  private:
    UINT32 numsets;
    UINT32 assoc;
    UINT32 replPolicy;

    // For SRRIP
    bool hitpolicy; // 0 for HP (hit to 0) 1 for FP (hit decrement)
    UINT32 RRIP_MAX; //maximum of RRPV value
    // For DRRIP
    UINT32 NumLeaderSets;
    UINT32 PSEL_MAX;
    UINT32 PSEL;
    UINT32 BRRIP_rate; //determine how frequent to have a bimodal insertion.
    // For SHiP
    UINT32 NumSHCTEntries;
    UINT32 NumSigBits;
    UINT32 NumSHCTCtrBits;
    UINT32 *SHCT;
    // For EAF
    UINT32 Alpha;
    UINT32 NumEAFEntry; // m = alpha * #cacheblocks (alpha = 8)
    UINT32 AddrCounter; // counter of number of addresses
    UINT32 *EAF;
    UINT32 *Hash_a;
    UINT32 *Hash_b;
    UINT32 NumHash;

    
    LINE_REPLACEMENT_STATE   **repl;

    COUNTER mytimer;  // tracks # of references to the cache

    // CONTESTANTS:  Add extra state for cache here
    // below are stats
    // DRRIP
    UINT32 stat_DRRIP_BL; // BRRIP Leader
    UINT32 stat_DRRIP_SL; // SRRIP Leader
    UINT32 stat_DRRIP_BI; // BRRIP insert
    UINT32 stat_DRRIP_SI; // SRRIP insert

    UINT32 stat_SHiP_BI;
    UINT32 stat_SHiP_GI;

    UINT32 stat_EAF_LSI; //leader set static insert
    UINT32 stat_EAF_LBI; //leader set bypass insert

    UINT32 stat_EAF_SBI; //EAF bad insert static
    UINT32 stat_EAF_SGI; //EAF good insert static

    UINT32 stat_EAF_BBI; //EAF bad insert bypass
    UINT32 stat_EAF_BGI; //EAF good insert bypass

  public:

    // The constructor CAN NOT be changed
    CACHE_REPLACEMENT_STATE( UINT32 _sets, UINT32 _assoc, UINT32 _pol );

    INT32  GetVictimInSet( UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType );
    void   UpdateReplacementState( UINT32 setIndex, INT32 updateWayID );

    void   SetReplacementPolicy( UINT32 _pol ) { replPolicy = _pol; } 
    void   IncrementTimer() { mytimer++; } 

    void   UpdateReplacementState( UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine, 
                                   UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit );

    ostream&   PrintStats( ostream &out);

  private:
    
    void   InitReplacementState();
    INT32  Get_Random_Victim( UINT32 setIndex );

    INT32  Get_LRU_Victim( UINT32 setIndex );
    void   UpdateLRU( UINT32 setIndex, INT32 updateWayID );

    INT32  Get_SRRIP_Victim( UINT32 setIndex );

    void   UpdateSRRIP( UINT32 setIndex, INT32 updateWayID, bool cacheHit );

    void   UpdateBRRIP( UINT32 setIndex, INT32 updateWayID, bool cacheHit );
    void   UpdateDRRIP( UINT32 setIndex, INT32 updateWayID, bool cacheHit );

    void   UpdateSHiP( UINT32 setIndex, INT32 updateWayID, bool cacheHit,  Addr_t PC);

    UINT32   EAF_hash_a (Addr_t memaddr); 
    UINT32   EAF_hash_b (Addr_t memaddr); 

    INT32  Get_EAF_Victim( UINT32 setIndex, const LINE_STATE *vicSet );
    void   UpdateEAF( UINT32 setIndex, INT32 updateWayID, bool cacheHit,const LINE_STATE *currLine );
    void   UpdateSEAF( UINT32 setIndex, INT32 updateWayID, bool cacheHit,const LINE_STATE *currLine );
    void   UpdateBEAF( UINT32 setIndex, INT32 updateWayID, bool cacheHit,const LINE_STATE *currLine );
};


#endif
