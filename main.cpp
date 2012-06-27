/** RayPlatform is LGPLv3 **/
/** author: Sébastien Boisvert **/

#include <core/ComputeCore.h>
#include "plugins/NetworkTest/NetworkTest.h"
using namespace std;

int main(int argc,char**argv){

/** The ComputeCore is the kernel of 
 * a RayPlatform-based parallel application.
 * It manages a lot of stuff for the programmer.
 * **/
	ComputeCore core;
	core.constructor(&argc,&argv);

/**
 * The only thing you can do to alter your application
 * is to register plugins.
 **/
	NetworkTest testPlugin;
	core.registerPlugin(&testPlugin);

	// more plugins can be registered !

/**
 * Then the core is started
 */
	core.run();

// write plugin information
	//core.printPlugins(".");

/**
 * When the core returns
 * we destroy it safely 
 */
	core.destructor();

	return 0;
}
