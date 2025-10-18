
#include <maya/MFnPlugin.h>
#include "ToySurface.h"


MStatus initializePlugin( MObject obj_ )
{
	MStatus status = MStatus::kFailure;

	MFnPlugin plugin( obj_, "ToyExample", "1.0", "Any", &status );
	CHECK_MSTATUS_AND_RETURN_IT(status);

	status = plugin.registerCommand( "Create_Toy_Surface", Create_Toy_Surface::creator );
	CHECK_MSTATUS_AND_RETURN_IT( status );

	status = plugin.registerShape( "ToySurface", ToySurface::id, &ToySurface::creator, &ToySurface::initialize, &ToySurface::drawDbClassification );
	CHECK_MSTATUS_AND_RETURN_IT(status);

	status = MDrawRegistry::registerGeometryOverrideCreator( ToySurface::drawDbClassification, ToySurface::drawRegistrantId, ToySurfaceGeomOverride::creator);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = ToySurfaceGeomOverride::registerComponentConverters();
	CHECK_MSTATUS_AND_RETURN_IT(status);

	MGlobal::displayInfo( MString("Toy plugin is initialized!") );
	return status;
}


MStatus uninitializePlugin( MObject obj_ )
{
	MStatus status = MStatus::kFailure;

	MFnPlugin plugin( obj_, "ToyExample", "1.0", "Any", &status );
	CHECK_MSTATUS_AND_RETURN_IT(status);

	status = plugin.deregisterCommand( "Create_Toy_Surface" );
	CHECK_MSTATUS_AND_RETURN_IT(status);

	status = plugin.deregisterNode( ToySurface::id );
	CHECK_MSTATUS_AND_RETURN_IT(status);

	status = MDrawRegistry::deregisterGeometryOverrideCreator( ToySurface::drawDbClassification, ToySurface::drawRegistrantId);
	CHECK_MSTATUS_AND_RETURN_IT(status);
	status = ToySurfaceGeomOverride::deregisterComponentConverters();
	CHECK_MSTATUS_AND_RETURN_IT(status);

	MGlobal::displayInfo( MString("Toy plugin has been uninitialized.") );
	return status;
}

