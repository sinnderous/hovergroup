/*
 * uPokeDBHex
 *        File: PokeDB_Info.cpp
 *  Created on: Jul 19, 2012
 *      Author: Josh Leighton
 */

/*
 * This file is modified from the original MOOS-IvP version to
 * support poking binary strings in hex representation.
 */

/*****************************************************************/
/*    NAME: Michael Benjamin, Henrik Schmidt, and John Leonard   */
/*    ORGN: Dept of Mechanical Eng / CSAIL, MIT Cambridge MA     */
/*    FILE: PokeDB_Info.cpp                                      */
/*    DATE: Dec 25th 2011                                        */
/*                                                               */
/* This program is free software; you can redistribute it and/or */
/* modify it under the terms of the GNU General Public License   */
/* as published by the Free Software Foundation; either version  */
/* 2 of the License, or (at your option) any later version.      */
/*                                                               */
/* This program is distributed in the hope that it will be       */
/* useful, but WITHOUT ANY WARRANTY; without even the implied    */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR       */
/* PURPOSE. See the GNU General Public License for more details. */
/*                                                               */
/* You should have received a copy of the GNU General Public     */
/* License along with this program; if not, write to the Free    */
/* Software Foundation, Inc., 59 Temple Place - Suite 330,       */
/* Boston, MA 02111-1307, USA.                                   */
/*****************************************************************/

// File modified July 19 2012 by Josh Leighton
#include <cstdlib>
#include <iostream>
#include "ColorParse.h"
#include "ReleaseInfo.h"
#include "PokeDB_Info.h"

using namespace std;

//----------------------------------------------------------------
// Procedure: showSynopsis

void showSynopsis() {
    blk("SYNOPSIS:                                                       ");
    blk("------------------------------------                            ");
    blk("  The uPokeDB application is a command-line tool for poking a   ");
    blk("  MOOSDB with variable-value pairs provided on the command line.");
}

//----------------------------------------------------------------
// Procedure: showHelpAndExit

void showHelpAndExit() {
    blk("                                                                ");
    blu("=============================================================== ");
    blu("Usage: uPokeDB file.moos [OPTIONS]                              ");
    blu("=============================================================== ");
    blk("                                                                ");
    showSynopsis();
    blk("                                                                ");
    blk("Options:                                                        ");
    mag("  --example, -e                                                 ");
    blk("      Display example MOOS configuration block.                 ");
    mag("  --help, -h                                                    ");
    blk("      Display this help message.                                ");
    mag("  --interface, -i                                               ");
    blk("      Display MOOS publications and subscriptions.              ");
    mag("  --version,-v                                                  ");
    blk("      Display the release version of uPokeDB.                   ");
    mag("  <host=val>                                                    ");
    blk("      Provide MOOSDB IP address on the command line rather than ");
    blk("      from a .moos file.                                        ");
    mag("  <port=val>                                                    ");
    blk("      Provide MOOSDB port number on the command line rather than");
    blk("      from a .moos file.                                        ");
    mag("  <var=value>                                                   ");
    blk("      Provide one or more MOOS variable-value pairs to poke to  ");
    blk("      the MOOSDB.                                               ");
    mag("  <var:=value>                                                  ");
    blk("      Poke the variable-value pair as a string even if the value");
    blk("      looks like a double.                                      ");
    blk("                                                                ");
    blk("Examples:                                                       ");
    blk("     $ uPokeDBHex ACOMMS_TRANSMIT_DATA:=a5:61:bc alpha.moos        ");
    exit(0);
}

//----------------------------------------------------------------
// Procedure: showExampleConfigAndExit

void showExampleConfigAndExit() {
    blk("                                                                ");
    blu("=============================================================== ");
    blu("uPokeDB Example MOOS Configuration                              ");
    blu("=============================================================== ");
    blk("                                                                ");
    mag("Not Applicable", " - uPokeDB is run from the command line. If a ");
    blk("MOOS file is provided on the command line, it is used solely to ");
    blk("read the ServerHost and ServerPort information. uPokeDB does not");
    blk("read a configuration block in the provided MOOS file.           ");
    blk("                                                                ");
    exit(0);
}

//----------------------------------------------------------------
// Procedure: showInterfaceAndExit

void showInterfaceAndExit() {
    blk("                                                                ");
    blu("=============================================================== ");
    blu("uPokeDB INTERFACE                                           ");
    blu("=============================================================== ");
    blk("                                                                ");
    showSynopsis();
    blk("                                                                ");
    blk("SUBSCRIPTIONS:                                                  ");
    blk("------------------------------------                            ");
    blk("  Variables published by the user are also subscribed for as a  ");
    blk("  way of verifying the results of the poke.                     ");
    blk("                                                                ");
    blk("PUBLICATIONS:                                                   ");
    blk("------------------------------------                            ");
    blk("  Variables published are those specified by the user.          ");
    blk("                                                                ");
    exit(0);
}

//----------------------------------------------------------------
// Procedure: showReleaseInfoAndExit

void showReleaseInfoAndExit() {
    showReleaseInfo("uPokeDB", "gpl");
    exit(0);
}

