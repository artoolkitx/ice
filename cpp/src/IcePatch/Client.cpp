// **********************************************************************
//
// Copyright (c) 2002
// MutableRealms, Inc.
// Huntsville, AL, USA
//
// All Rights Reserved
//
// **********************************************************************

#include <Ice/Application.h>
#include <IcePatch/NodeDescFactory.h>
#include <IcePatch/Util.h>

using namespace std;
using namespace Ice;
using namespace IcePatch;

namespace IcePatch
{

class Client : public Application
{
public:

    void usage();
    virtual int run(int, char*[]);
    void printNodeDescSeq(const NodeDescSeq&, const string&);
    void getFile(const FilePrx&);
};

};

void
IcePatch::Client::usage()
{
    cerr << "Usage: " << appName() << " [options]\n";
    cerr <<     
        "Options:\n"
        "-h, --help           Show this message.\n"
        "-v, --version        Display the Ice version.\n"
        ;
}

int
IcePatch::Client::run(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
	if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
	{
	    usage();
	    return EXIT_SUCCESS;
	}
	else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0)
	{
	    cout << ICE_STRING_VERSION << endl;
	    return EXIT_SUCCESS;
	}
	else
	{
	    cerr << appName() << ": unknown option `" << argv[i] << "'" << endl;
	    usage();
	    return EXIT_FAILURE;
	}
    }

    PropertiesPtr properties = communicator()->getProperties();
    
    //
    // Get the working directory and change to this directory.
    //
    const char* directoryProperty = "IcePatch.Directory";
    string directory = properties->getProperty(directoryProperty);
    if (!directory.empty())
    {
	if (chdir(directory.c_str()) == -1)
	{
	    cerr << appName() << ": can't change to directory `" << directory << "': " << strerror(errno) << endl;
	    return EXIT_FAILURE;
	}
    }

    //
    // Get the IcePatch endpoints.
    //
    const char* endpointsProperty = "IcePatch.Endpoints";
    string endpoints = properties->getProperty(endpointsProperty);
    if (endpoints.empty())
    {
	cerr << appName() << ": property `" << endpointsProperty << "' is not set" << endl;
	return EXIT_FAILURE;
    }

    //
    // Create and install the node description factory.
    //
    ObjectFactoryPtr factory = new NodeDescFactory;
    communicator()->addObjectFactory(factory, "::IcePatch::DirectoryDesc");
    communicator()->addObjectFactory(factory, "::IcePatch::FileDesc");

    try
    {
	//
	// Display node structure.
	//
	Identity identity = pathToIdentity(".");
	ObjectPrx topObj = communicator()->stringToProxy(identityToString(identity) + ':' + endpoints);
	NodePrx top = NodePrx::checkedCast(topObj);
	assert(top);
	DirectoryDescPtr topDesc = DirectoryDescPtr::dynamicCast(top->describe());
	assert(topDesc);
	string path = identityToPath(topDesc->directory->ice_getIdentity());
	cout << pathToName(path) << endl;
	cout << "|" << endl;
	printNodeDescSeq(topDesc->directory->getContents(), "");
    }
    catch (const NodeAccessException& ex)
    {
	cerr << ex << ":\n" << ex.reason << endl;
	return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void
IcePatch::Client::printNodeDescSeq(const NodeDescSeq& nodeDescSeq, const string& indent)
{
    if (nodeDescSeq.empty())
    {
	return;
    }

    for (unsigned int i = 0; i < nodeDescSeq.size(); ++i)
    {
	string path;
	DirectoryDescPtr directoryDesc = DirectoryDescPtr::dynamicCast(nodeDescSeq[i]);
	FileDescPtr fileDesc;
	if (directoryDesc)
	{
	    path = identityToPath(directoryDesc->directory->ice_getIdentity());
	}
	else
	{
	    fileDesc = FileDescPtr::dynamicCast(nodeDescSeq[i]);
	    assert(fileDesc);
	    path = identityToPath(fileDesc->file->ice_getIdentity());
	}

	bool last = (i == nodeDescSeq.size() - 1);
	
	if (directoryDesc)
	{
	    string newIndent;
	    if (last)
	    {
		newIndent = indent + "  ";
	    }
	    else
	    {
		newIndent = indent + "| ";
	    }
	    cout << indent << "+-" << pathToName(path) << "... " << flush;

	    FileInfo info = getFileInfo(path);
	    switch (info)
	    {
		case FileInfoNotExist:
		{
		    cout << "creating directory... " << flush;
		    createDirectory(path);
		    break;
		}

		case FileInfoDirectory:
		{
		    break;
		}

		case FileInfoRegular:
		{
		    cout << "removing regular file... " << flush;
		    removeRecursive(path);
		    cout << "creating directory... " << flush;
		    createDirectory(path);
		    break;
		}

		case FileInfoUnknown:
		{
		    cout << "removing unknown file... " << flush;
		    removeRecursive(path);
		    cout << "creating directory... " << flush;
		    createDirectory(path);
		    break;
		}
	    }

	    cout << "ok" << endl;

	    cout << newIndent << "|" << endl;
	    printNodeDescSeq(directoryDesc->directory->getContents(), newIndent);
	}
	else
	{
	    assert(fileDesc);
	    cout << indent << "+-" << pathToName(path) << "... " << flush;

	    FileInfo info = getFileInfo(path);
	    switch (info)
	    {
		case FileInfoNotExist:
		{
		    cout << "getting file... " << flush;
		    getFile(fileDesc->file);
		    break;
		}

		case FileInfoDirectory:
		{
		    cout << "removing directory... " << flush;
		    removeRecursive(path);
		    cout << "getting file... " << flush;
		    getFile(fileDesc->file);
		    break;
		}

		case FileInfoRegular:
		{
		    ByteSeq md5 = getMD5(path);
		    if (md5 != fileDesc->md5)
		    {
			cout << "removing file... " << flush;
			removeRecursive(path);
			cout << "getting file... " << flush;
			getFile(fileDesc->file);
		    }
		    break;
		}

		case FileInfoUnknown:
		{
		    cout << "removing unknown file... " << flush;
		    removeRecursive(path);
		    cout << "getting file... " << flush;
		    getFile(fileDesc->file);
		    break;
		}
	    }

	    cout << "ok" << endl;

	    if (last)
	    {
		cout << indent << endl;
	    }
	}
    }
}

void
IcePatch::Client::getFile(const FilePrx& file)
{
    ByteSeq bytes;
    Int pos = 0;

    do
    {
	bytes = file->getBytesBZ2(pos, 256 * 1024);
	pos += bytes.size();
    }
    while (!bytes.empty());
}

int
main(int argc, char* argv[])
{
    addArgumentPrefix("IcePatch");
    Client app;
    return app.main(argc, argv);
}
