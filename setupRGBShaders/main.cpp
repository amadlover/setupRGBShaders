#include <maya/MPxCommand.h>

#include <maya/MGlobal.h>
#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MDGModifier.h>
#include <maya/MSelectionList.h>
#include <maya/MPlug.h>
#include <maya/MIteratorType.h>

#include <maya/MItDag.h>

#include <maya/MFnTransform.h>
#include <maya/MFnDependencyNode.h>

#include <maya/MObjectArray.h>

using namespace std;

class setupRGBShaders : public MPxCommand
{
public:
	setupRGBShaders(){ undoLevels = 0; }
	~setupRGBShaders(){}

	static void *creator() { return new setupRGBShaders; }
	virtual MStatus doIt( const MArgList & );
	virtual bool isUndoable() { return true; }
	virtual MStatus undoIt();
	virtual MStatus redoIt();
	virtual bool hasSyntax() { return false; }

private:
	MString fileName, folderPath;
	MObjectArray meshObjs;
	MObjectArray camObjs;
	MObject mrFbList;

	MDGModifier dgMod;

	void dgDoIt();

	int undoLevels;
};

bool checkForError( MStatus stat, MString action )
{
	if( !stat ) {
		stat.perror( action );
		return false;
	}
	else
		return true;
}

void setupRGBShaders::dgDoIt()
{
	dgMod.doIt();
	undoLevels++;
}


MStatus setupRGBShaders::doIt( const MArgList & args )
{
	unsigned int FIndex = args.flagIndex( "fp", "folderPath" );
	unsigned int fIndex = args.flagIndex( "fn", "fileName" );

	if( FIndex == MArgList::kInvalidArgIndex || fIndex == MArgList::kInvalidArgIndex ) {
		MGlobal::displayError( "Error specifying flag or flag values. \n-fp, -folderPath <folder_path> \t -fn, -fileName <file_name>" );
		return MS::kFailure;
	}

	folderPath = args.asString( FIndex );
	fileName = args.asString( fIndex );

	MItDag meshIt( MItDag::kDepthFirst, MFn::kMesh );

	for( ; !meshIt.isDone(); meshIt.next() ) {
		MDagPath dagPath;

		meshIt.getPath( dagPath );
		meshObjs.append( dagPath.transform() );
	}

	MItDag camIt( MItDag::kDepthFirst, MFn::kCamera );

	for( ; !camIt.isDone(); camIt.next() ) {
		MDagPath dagPath;

		camIt.getPath( dagPath );
		MFnDependencyNode camFn( dagPath.node() );

		bool isRenderable;
		camFn.findPlug( "renderable" ).getValue( isRenderable );

		if( isRenderable )
			camObjs.append( dagPath.transform() );
	}

	MGlobal::executeCommand( "setAttr miDefaultFramebuffer.datatype 5" );

	return redoIt();
}

MStatus setupRGBShaders::redoIt()
{
	int numLayers = meshObjs.length() / 3;

	if( numLayers == 0 && meshObjs.length() > 0 )
		numLayers++;

	if( meshObjs.length() % 3 > 0 && meshObjs.length() > 3 )
		numLayers++;

	for( int l = 0; l < numLayers; l++ ) {
		MStatus stat;

		MFnDependencyNode mrUserBufferFn( dgMod.createNode( "mentalrayUserBuffer", &stat )); 
		dgDoIt();
		//MGlobal::executeCommand( MString( "connectAttr -f " ) + mrUserBufferFn.name() + ".message miDefaultOptions.frameBufferList[" + l + "]" );
		dgMod.commandToExecute( MString( "connectAttr -f " ) + mrUserBufferFn.name() + ".message miDefaultOptions.frameBufferList[" + l + "]" );
		dgDoIt();		
		mrUserBufferFn.findPlug( "dataType" ).setValue( 5 );

		for( int i = 0; i < 3; i++ ) {
			if(( l * 3 + i ) > (int)meshObjs.length() - 1 ) break;

			MFnDependencyNode nkPassFn( dgMod.createNode( "nkPass" )); dgDoIt();
			MFnDependencyNode meshFn( meshObjs[ l * 3 + i ]);

			dgMod.commandToExecute( MString( "sets -renderable true -noSurfaceShader true -empty -name " ) + nkPassFn.name() + "SG" );dgDoIt();
			dgMod.commandToExecute( MString( "connectAttr -f " ) + nkPassFn.name() + ".outValue " + nkPassFn.name() + "SG.miMaterialShader" );dgDoIt();
			dgMod.commandToExecute( MString( "sets -e -forceElement ") + nkPassFn.name() + "SG " + meshFn.name()); dgDoIt();

			/*MGlobal::executeCommand( MString( "sets -renderable true -noSurfaceShader true -empty -name " ) + nkPassFn.name() + "SG" );
			MGlobal::executeCommand( MString( "connectAttr -f " ) + nkPassFn.name() + ".outValue " + nkPassFn.name() + "SG.miMaterialShader" );
			MGlobal::executeCommand( MString( "sets -e -forceElement ") + nkPassFn.name() + "SG " + meshFn.name());*/

			nkPassFn.findPlug( "layerNumber" ).setValue( l );

			if( i == 0 )
				dgMod.commandToExecute( MString( "setAttr " ) + nkPassFn.name() + ".color -type double3 1 0 0" );
				//MGlobal::executeCommand( MString( "setAttr " ) + nkPassFn.name() + ".color -type double3 1 0 0" );

			if( i == 1 )
				dgMod.commandToExecute( MString( "setAttr " ) + nkPassFn.name() + ".color -type double3 0 1 0" );
				//MGlobal::executeCommand( MString( "setAttr " ) + nkPassFn.name() + ".color -type double3 0 1 0" );

			if( i == 2 )
				dgMod.commandToExecute( MString( "setAttr " ) + nkPassFn.name() + ".color -type double3 0 0 1" );
				//MGlobal::executeCommand( MString( "setAttr " ) + nkPassFn.name() + ".color -type double3 0 0 1" );

			dgDoIt();
		}
	}

	for( unsigned int c = 0; c < camObjs.length(); c++ ) {
		MFnDependencyNode mrOutputPass( dgMod.createNode( "mentalrayOutputPass" )); dgDoIt();
		MFnDependencyNode nkSaver( dgMod.createNode( "nkSaver" )); dgDoIt();
		dgMod.connect( nkSaver.findPlug( "outValue" ), mrOutputPass.findPlug( "outputShader" )); dgDoIt();

		MFnDependencyNode camFn( camObjs[c] );

		//MGlobal::executeCommand( MString( "connectAttr -f " ) + mrOutputPass.name() + ".message " + camFn.name() + ".miOutputShaderList[0]" );
		dgMod.commandToExecute( MString( "connectAttr -f " ) + mrOutputPass.name() + ".message " + camFn.name() + ".miOutputShaderList[0]" ); dgDoIt();
		
		nkSaver.findPlug( "numLayers" ).setValue( numLayers );
		nkSaver.findPlug( "camName" ).setValue( camFn.name() );
		nkSaver.findPlug( "fileName" ).setValue( fileName );
		nkSaver.findPlug( "folderPath" ).setValue( folderPath );
		mrOutputPass.findPlug( "datatype" ).setValue( 5 );
	}

	setResult( "Shaders setup to Render" );

	return MS::kSuccess;
}

MStatus setupRGBShaders::undoIt()
{
	for( int u = 0; u < undoLevels; u++ )
		dgMod.undoIt();

	return MS::kSuccess;
}

#include <maya/MFnPlugin.h>
#include <maya/MImage.h>

__declspec( dllexport ) MStatus initializePlugin( MObject obj )
{
	char *version;

#ifdef MAYA2010
	version = "2010";
#else
	version = "2008";
#endif

	MImage ip1;
	ip1.readFromFile( "E:/Release/bump.jpg" );

	unsigned char *ip1Pix = ip1.pixels();
	unsigned int ip1width, ip1height; ip1.getSize( ip1width, ip1height );

	MImage ip2;
	ip2.readFromFile( "E:/Release/transReflect.jpg" );

	unsigned char *ip2Pix = ip2.pixels();
	unsigned int ip2Width, ip2Height; ip2.getSize( ip2Width, ip2Height );

	float *pixels = new float[ ip2Width, ip2Height ];

	for( int y = 150; y < 200; y++ ) {
		for( int x = 100; x < 150; x++ ) {
			ip2Pix[ ( y * ip2Width + x ) * ip2.depth() ] += 1;//ip1Pix[ ( y * ip1width + x ) * ip1.depth() ];
			ip2Pix[ ( y * ip2Width + x ) * ip2.depth() + 1 ] += 1;//ip1Pix[ ( y * ip1width + x ) * ip1.depth() + 1 ];
			ip2Pix[ ( y * ip2Width + x ) * ip2.depth() + 2 ] += 1;//ip1Pix[ ( y * ip1width + x ) * ip1.depth() + 2 ];
		}
	}

	MImage img;
	img.create( ip2Width, ip2Height, ip2.depth() );
	img.setPixels( ip2Pix, ip2Width, ip2Height );
	img.writeToFile( "E:/Release/fucked.jpg", "jpg" );

	MFnPlugin plugFn( obj, "The LABS", version );
	MStatus stat = plugFn.registerCommand( "setupRGBShaders", setupRGBShaders::creator );
	
	if( !stat ) {
		stat.perror( "Registering Command setupRGBShaders" );
		return MS::kFailure;
	}



	return MS::kSuccess;
}

__declspec( dllexport ) MStatus uninitializePlugin( MObject obj )
{
	MFnPlugin plugFn( obj );
	plugFn.deregisterCommand( "setupRGBShaders" );
	return MS::kSuccess;
}