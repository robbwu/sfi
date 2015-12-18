
/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs 
 *  and could serve as the starting point for developing your first PIN tool
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <xed-iclass-enum.h>
#include <set>
#include <stack>
#include <vector>
#include <algorithm>
//#include <execinfo.h>
// to make Kdevelop happy

// end to make Kdevelop happy
class Frame {
    string name;
};
using namespace std;
/* ================================================================== */
// Global variables 
/* ================================================================== */

UINT64 fpCount = 0;        //number of dynamically executed instructions
UINT64 insCount = 0;
UINT64 bblCount = 0;        //number of dynamically executed basic blocks
UINT64 threadCount = 0;     //total number of threads, including main thread
map<INT32,INT64> cat;
set<string> syms;
std::ostream * out = &cerr;

struct frame {
    string name;
    ADDRINT callsite;
};
//vector<ADDRINT> stackframes;
vector<pair<ADDRINT, ADDRINT> > stackframes;
/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,  "pintool",
    "o", "sfi.out", "specify file name for MyPinTool output");

KNOB<BOOL>   KnobCount(KNOB_MODE_WRITEONCE,  "pintool",
    "count", "1", "count instructions, basic blocks and threads in the application");


/* ===================================================================== */
// Utilities
/* ===================================================================== */

UINT64 movsdcnt = 0;

map<ADDRINT, INT64> addrmap;

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool prints out the number of dynamically executed " << endl <<
            "FP instructions in the application." << endl << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}




/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID *v)
{
    *out <<  "===============================================" << endl;
    *out <<  "MyPinTool analysis results: " << endl;
    *out <<  "Number of FP instructions: " << insCount  << endl;
    //*out <<  "Number of basic blocks: " << bblCount  << endl;
    //*out <<  "Number of threads: " << threadCount  << endl;
    *out << "The instruction categories and execution counts:" << endl;
    for (map<INT32,INT64>::iterator it=cat.begin(); it!=cat.end(); it++) {
        *out << it->first << "\t" << CATEGORY_StringShort(it->first) << "\t" << it->second << endl;
    }
    *out << "MOVSD_XMM counts:" << movsdcnt << endl;
    *out << "Number of different addresses read:\t" << addrmap.size() << endl;
    *out <<  "===============================================" << endl;

    *out << "Stackframes size:" << "\t" << stackframes.size() << endl;
}


VOID instrcount(INT32 category) {
    cat[category]++;
}
VOID movsdmem(ADDRINT *addr) {
    movsdcnt += 1;
    //addrmap[addr]++;
    double val;
    //PIN_SafeCopy(&val, reinterpret_cast<VOID*>(addr), sizeof(double));
    val = 0;
    PIN_SafeCopy(addr, &val
            , sizeof(double));
    //*(double*) addr = 0;
}

static BOOL IsPLT(TRACE trace)
{
    RTN rtn = TRACE_Rtn(trace);

    // All .plt thunks have a valid RTN
    if (!RTN_Valid(rtn))
        return FALSE;

    if (".plt" == SEC_Name(RTN_Sec(rtn)))
        return TRUE;
    return FALSE;
}

string invalid = "invalid_rtn";

// could be improved by elminating one function call.
const string *Target2String(ADDRINT target)
{
    string name = RTN_FindNameByAddress(target);
    if (name == "")
        return &invalid;
    else
        return new string(name);
}
VOID docall1(ADDRINT ip, ADDRINT addr, ADDRINT callsite, ADDRINT sp) {

    if (syms.find(*Target2String(addr))!=syms.end()) {
        cout << "CALL1\t" << "IP " << ip << "\tINTO " << *Target2String(addr)
             << "\tFrom " << *Target2String(callsite)  
             << "\tSP " << sp << endl;
        stackframes.push_back(make_pair(addr,callsite));
    }

}
VOID docall2(ADDRINT ip, ADDRINT addr, BOOL taken, ADDRINT callsite, ADDRINT sp) {
    if (taken) {

        if (syms.find(*Target2String(addr))!=syms.end()) {
            cout << "CALL2\t" << "IP " << ip << "\tINTO " <<  *Target2String(addr) 
                 << "\tFrom " << *Target2String(callsite) << "\t" << 
                 "SP " << sp << endl;
            stackframes.push_back(make_pair(addr,callsite));
        }
    }
}
VOID docall3(ADDRINT ip, ADDRINT addr, ADDRINT callsite, ADDRINT sp) {

    if (syms.find(*Target2String(addr))!=syms.end()) {
        cout << "CALL3\t" << "IP " << ip << "\tINTO " << *Target2String(addr)
             << "\tFrom " << *Target2String(callsite) 
             << "\tSP " << sp << endl;
        stackframes.push_back(make_pair(addr,callsite));
    }

}
VOID doret(ADDRINT ip,ADDRINT addr, ADDRINT retip, ADDRINT sp) {
    string retname = *Target2String(retip);
    if (syms.find(retname)==syms.end()) return;
    cout << "RET\t" <<"IP " << ip << "\tFROM " << *Target2String(addr) 
         << "\t" << "RETTO " << *Target2String(retip) 
         << "\tSP " << sp << "\t";
//    vector<ADDRINT>::reverse_iterator it = find(stackframes.rbegin(), stackframes.rend(), addr);
//    if (it == stackframes.rend()) {
//        cout << "not match!\t"  << endl;
//    } else {
//        cout << "match\t"  << "poping " << it-stackframes.rbegin() << endl;
//        stackframes.erase(it.base()-1, stackframes.end());
    if (!stackframes.empty() && stackframes.back().first == addr) {
        cout << "match" << endl;
        stackframes.pop_back();
    } else {
        cout << "mismatch" << endl;
    }
}
VOID MyTrace(TRACE trace, VOID *V) {
    RTN rtn = TRACE_Rtn(trace);
    if ( !RTN_Valid(rtn) || SEC_Name(RTN_Sec(rtn)) != ".text") return;
    //if ( syms.find(RTN_Name(rtn)) == syms.end()) return;
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        INS tail = BBL_InsTail(bbl);
        if( INS_IsCall(tail)) {
            if ( INS_IsDirectBranchOrCall(tail)) {
                ADDRINT target = INS_DirectBranchOrCallTargetAddress(tail);
                INS_InsertPredicatedCall(tail, IPOINT_BEFORE, AFUNPTR(docall1), 
                                         IARG_INST_PTR,
                                         IARG_ADDRINT, target,
                                         IARG_ADDRINT, INS_Address(tail), 
                                         IARG_REG_VALUE, REG_EBP, IARG_END);

            } else if ( !IsPLT(trace)) {
                INS_InsertCall(tail, IPOINT_BEFORE, AFUNPTR(docall2), 
                               IARG_INST_PTR,
                               IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN,
                               IARG_ADDRINT, INS_Address(tail), 
                               IARG_REG_VALUE, REG_EBP, IARG_END);
            }
        } else if ( INS_IsRet(tail) ) {
            //cout << "Instrumentation instruction type:" << INS_Mnemonic(tail) << endl;    
            RTN rtn = INS_Rtn(tail);
            if (RTN_Valid(rtn)) {
                INS_InsertCall(tail, IPOINT_BEFORE, AFUNPTR(doret), 
                               IARG_INST_PTR,
                               IARG_ADDRINT, RTN_Address(rtn), 
                               IARG_RETURN_IP,
                               IARG_REG_VALUE, REG_EBP, IARG_END);
            }
        } else if (IsPLT(trace)) {
            INS_InsertCall(tail, IPOINT_BEFORE, 
                           AFUNPTR(docall1),
                           IARG_INST_PTR,
                           IARG_BRANCH_TARGET_ADDR,
                           IARG_ADDRINT, INS_Address(tail),
                           IARG_REG_VALUE, REG_EBP, IARG_END);
        }
    }
}


/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments, 
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char *argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid 
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
    //cout << "Argument " << argv[argc-1];
    string fileName = KnobOutputFile.Value();


    if (!fileName.empty()) { out = new std::ofstream(fileName.c_str());}
    PIN_InitSymbols();

    // get the application name
    string appname;
    for (int i=argc-1; i>=0; i--) {
        if (string(argv[i])== "--") {
            appname = argv[i+1];
        }
    }

    IMG img = IMG_Open(appname);
    for ( SYM sym = IMG_RegsymHead(img); SYM_Valid(sym); sym=SYM_Next(sym)) {
        string symname = SYM_Name(sym);
        if (symname.empty() || symname[0]=='_') continue;

        cout << symname << endl;
        syms.insert(symname);
    }
    IMG_Close(img);
    cout << "application name:\t" << appname << endl;
    if (KnobCount)
    {
        //IMG_AddInstrumentFunction(ImageLoad, 0);
        // Register function to be called to instrument traces

        TRACE_AddInstrumentFunction(MyTrace, 0);
        // Register function to be called for every thread before it starts running
        //PIN_AddThreadStartFunction(ThreadStart, 0);
        //RTN_AddInstrumentFunction(Routine, 0);

        //INS_AddInstrumentFunction(Instruction, 0);
        // Register function to be called when the application exits
        PIN_AddFiniFunction(Fini, 0);
    }
    
    cerr <<  "===============================================" << endl;
    cerr <<  "This application is instrumented by MyPinTool" << endl;
    if (!KnobOutputFile.Value().empty()) 
    {
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    }
    cerr <<  "===============================================" << endl;

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
