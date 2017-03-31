/** @file   vise.cc
 *  @brief  main entry point for the VGG Image Search Enginer (VISE)
 *
 *  @author Abhishek Dutta (adutta@robots.ox.ac.uk)
 *  @date   31 March 2017
 */

#include <iostream>
#include <string>

#include "ViseServer.h"

int main(int argc, char** argv) {
  std::cout << "\nVGG Image Search Enginer (VISE)";
  std::cout << "\n";

  unsigned int port = 8080;
  std::cout << "\nStarting server in port " << port << " ...";

  ViseServer vise_server;
  vise_server.Start(port);

  std::string user_input;
  while ( user_input != "q" ) {
    std::cout << "\nPress \"q\" + [Enter] key to stop the server : ";
    std::cin >> user_input;
  }

  if ( vise_server.Stop() ) {
    std::cout << "\nBye\n";
    return 0;
  } else {
    std::cerr << "\nFailed to stop server!\n";
    return -1;
  }
}
