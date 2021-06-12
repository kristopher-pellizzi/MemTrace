
/*! @file
 *  Empty pintool used to compare MemTrace instrumentation overhead with intel PIN
 *  internal overhead
 */

#include "pin.H"
#include <iostream>
#include <fstream>
using std::cerr;
using std::string;
using std::endl;

/* ================================================================== */
// Global variables 
/* ================================================================== */


/* ===================================================================== */
// Command line switches
/* ===================================================================== */


/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool does absolutely nothing." << endl <<
            "This is only used as a timing comparison." << endl << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */


/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

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

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
