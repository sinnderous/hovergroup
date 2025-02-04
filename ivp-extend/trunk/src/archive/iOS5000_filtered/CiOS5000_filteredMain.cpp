// $Header: /home/cvsroot/project-marine-shell/src/iOS5000/CiOS5000Main.cpp,v 1.2 2007/08/03 19:18:14 mikerb Exp $
// (c) 2004

#include <string>
#include "MOOSLib.h"
#include "MOOSGenLib.h"
#include "CiOS5000_filtered.h"

int main(int argc, char *argv[])
{
  std::string sMissionFile = "iOS5000.moos";
	
  if(argc > 1) {
    sMissionFile = argv[1];
  }
  
  CiOS5000_filtered iOS5000;
  
  iOS5000.Run("iOS5000_filtered", sMissionFile.c_str());
  
  return 0;
}

