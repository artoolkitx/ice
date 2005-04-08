// **********************************************************************
//
// Copyright (c) 2003-2005 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#include <Gen.h>
#include <Slice/CPlusPlusUtil.h>
#include <IceUtil/Functional.h>
#include <IceUtil/Iterator.h>

#include <limits>
#include <sys/stat.h>

using namespace std;
using namespace Slice;
using namespace IceUtil;

Slice::Gen::Gen(const string& name, const string& base,	const string& headerExtension,
	        const string& sourceExtension, const string& include, const vector<string>& includePaths,
		const string& dllExport, const string& dir, bool imp) :
    _base(base),
    _headerExtension(headerExtension),
    _sourceExtension(sourceExtension),
    _include(include),
    _includePaths(includePaths),
    _dllExport(dllExport),
    _impl(imp)
{
    for(vector<string>::iterator p = _includePaths.begin(); p != _includePaths.end(); ++p)
    {
	if(p->length() && (*p)[p->length() - 1] != '/')
	{
	    *p += '/';
	}
    }

    string::size_type pos = _base.rfind('/');
    if(pos == string::npos)
    {
        pos = _base.rfind('\\');
    }
    if(pos != string::npos)
    {
	_base.erase(0, pos + 1);
    }

    if(_impl)
    {
        string fileImplH = _base + "I." + _headerExtension;
        string fileImplC = _base + "I." + _sourceExtension;
        if(!dir.empty())
        {
            fileImplH = dir + '/' + fileImplH;
            fileImplC = dir + '/' + fileImplC;
        }

        struct stat st;
        if(stat(fileImplH.c_str(), &st) == 0)
        {
            cerr << name << ": `" << fileImplH << "' already exists - will not overwrite" << endl;
            return;
        }
        if(stat(fileImplC.c_str(), &st) == 0)
        {
            cerr << name << ": `" << fileImplC << "' already exists - will not overwrite" << endl;
            return;
        }

        implH.open(fileImplH.c_str());
        if(!implH)
        {
            cerr << name << ": can't open `" << fileImplH << "' for writing" << endl;
            return;
        }
        
        implC.open(fileImplC.c_str());
        if(!implC)
        {
            cerr << name << ": can't open `" << fileImplC << "' for writing" << endl;
            return;
        }

        string s = fileImplH;
        if(_include.size())
        {
            s = _include + '/' + s;
        }
        transform(s.begin(), s.end(), s.begin(), ToIfdef());
        implH << "#ifndef __" << s << "__";
        implH << "\n#define __" << s << "__";
        implH << '\n';
    }

    string fileH = _base + "." + _headerExtension;
    string fileC = _base + "." + _sourceExtension;
    if(!dir.empty())
    {
	fileH = dir + '/' + fileH;
	fileC = dir + '/' + fileC;
    }

    H.open(fileH.c_str());
    if(!H)
    {
	cerr << name << ": can't open `" << fileH << "' for writing" << endl;
	return;
    }
    
    C.open(fileC.c_str());
    if(!C)
    {
	cerr << name << ": can't open `" << fileC << "' for writing" << endl;
	return;
    }

    printHeader(H);
    printHeader(C);
    H << "\n// Generated from file `" << changeInclude(_base, _includePaths) << ".ice'\n";
    C << "\n// Generated from file `" << changeInclude(_base, _includePaths) << ".ice'\n";

    string s = fileH;
    if(_include.size())
    {
	s = _include + '/' + s;
    }
    transform(s.begin(), s.end(), s.begin(), ToIfdef());
    H << "\n#ifndef __" << s << "__";
    H << "\n#define __" << s << "__";
    H << '\n';
}

Slice::Gen::~Gen()
{
    H << "\n\n#endif\n";
    C << '\n';

    if(_impl)
    {
        implH << "\n\n#endif\n";
        implC << '\n';
    }
}

bool
Slice::Gen::operator!() const
{
    if(!H || !C)
    {
        return true;
    }
    if(_impl && (!implH || !implC))
    {
        return true;
    }
    return false;
}

void
Slice::Gen::generate(const UnitPtr& p)
{
    validateMetaData(p);

    C << "\n#include <";
    if(_include.size())
    {
	C << _include << '/';
    }
    C << _base << "." << _headerExtension << ">";

    H << "\n#include <Ice/LocalObjectF.h>";
    H << "\n#include <Ice/ProxyF.h>";
    H << "\n#include <Ice/ObjectF.h>";
    H << "\n#include <Ice/Exception.h>";
    H << "\n#include <Ice/LocalObject.h>";

    if(p->usesProxies())
    {
	H << "\n#include <Ice/Proxy.h>";
    }

    if(p->hasNonLocalClassDefs())
    {
	H << "\n#include <Ice/Object.h>";
	H << "\n#include <Ice/Outgoing.h>";
	H << "\n#include <Ice/Incoming.h>";
	C << "\n#include <Ice/LocalException.h>";
    }

    if(p->hasNonLocalExceptions())
    {
	H << "\n#include <Ice/UserExceptionFactory.h>";
    }

    if(p->hasDataOnlyClasses() || p->hasNonLocalExceptions())
    {
	H << "\n#include <Ice/FactoryTable.h>";
    }

    if(p->usesNonLocals())
    {
	C << "\n#include <Ice/BasicStream.h>";
	C << "\n#include <Ice/Object.h>";
    }

    if(p->hasNonLocalExceptions())
    {
        C << "\n#include <Ice/LocalException.h>";
    }

    StringList includes = p->includeFiles();

    for(StringList::const_iterator q = includes.begin(); q != includes.end(); ++q)
    {
	H << "\n#include <" << changeInclude(*q, _includePaths) << "." << _headerExtension << ">";
    }

    printVersionCheck(H);
    printVersionCheck(C);

    printDllExportStuff(H, _dllExport);
    if(_dllExport.size())
    {
	_dllExport += " ";
    }

    ProxyDeclVisitor proxyDeclVisitor(H, C, _dllExport);
    p->visit(&proxyDeclVisitor, false);

    ObjectDeclVisitor objectDeclVisitor(H, C, _dllExport);
    p->visit(&objectDeclVisitor, false);

    IceInternalVisitor iceInternalVisitor(H, C, _dllExport);
    p->visit(&iceInternalVisitor, false);

    HandleVisitor handleVisitor(H, C, _dllExport);
    p->visit(&handleVisitor, false);

    TypesVisitor typesVisitor(H, C, _dllExport);
    p->visit(&typesVisitor, false);

    ProxyVisitor proxyVisitor(H, C, _dllExport);
    p->visit(&proxyVisitor, false);

    DelegateVisitor delegateVisitor(H, C, _dllExport);
    p->visit(&delegateVisitor, false);

    ObjectVisitor objectVisitor(H, C, _dllExport);
    p->visit(&objectVisitor, false);

    if(_impl)
    {
        implH << "\n#include <";
        if(_include.size())
        {
            implH << _include << '/';
        }
        implH << _base << ".h>";

        implC << "#include <";
        if(_include.size())
        {
            implC << _include << '/';
        }
        implC << _base << "I.h>";

        ImplVisitor implVisitor(implH, implC, _dllExport);
        p->visit(&implVisitor, false);
    }
}

Slice::Gen::TypesVisitor::TypesVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport)
{
}

bool
Slice::Gen::TypesVisitor::visitModuleStart(const ModulePtr& p)
{
    string name = fixKwd(p->name());
    
    H << sp << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::TypesVisitor::visitModuleEnd(const ModulePtr& p)
{
    H << sp << nl << '}';
}

bool
Slice::Gen::TypesVisitor::visitClassDefStart(const ClassDefPtr&)
{
    return false;
}

bool
Slice::Gen::TypesVisitor::visitExceptionStart(const ExceptionPtr& p)
{
    string name = fixKwd(p->name());
    string scoped = fixKwd(p->scoped());
    ExceptionPtr base = p->base();

    H << sp << nl << "class " << _dllExport << name << " : ";
    H.useCurrentPosAsIndent();
    if(!base)
    {
	if(p->isLocal())
	{
	    H << "public ::Ice::LocalException";
	}
	else
	{
	    H << "public ::Ice::UserException";
	}
    }
    else
    {
	H << "public " << fixKwd(base->scoped());
    }
    H.restoreIndent();
    H << sb;

    H.dec();
    H << nl << "public:";
    H.inc();

    H << sp;
    if(p->isLocal())
    {
	H << nl << name << "(const char*, int);";
	C << sp << nl << scoped.substr(2) << "::" << name << "(const char* file, int line) : ";
	C.inc();
	if(!base)
	{
	    C << nl << "::Ice::LocalException(file, line)";
	}
	else
	{
	    C << nl << fixKwd(base->scoped()) << "(file, line)";
	}
	C.dec();
	C << sb;
	C << eb;
    }

    H << nl << "virtual const ::std::string& ice_name() const;";

    string flatName = p->flattenedScope() + p->name() + "_name";

    C << sp << nl << "static const ::std::string " << flatName << " = \"" << p->scoped().substr(2) << "\";";
    C << sp << nl << "const ::std::string&" << nl << scoped.substr(2) << "::ice_name() const";
    C << sb;
    C << nl << "return " << flatName << ';';
    C << eb;
    
    if(p->isLocal())
    {
	H << nl << "virtual void ice_print(::std::ostream&) const;";
    }

    H << nl << "virtual ::Ice::Exception* ice_clone() const;";
    C << sp << nl << "::Ice::Exception*" << nl << scoped.substr(2) << "::ice_clone() const";
    C << sb;
    C << nl << "return new " << name << "(*this);";
    C << eb;

    H << nl << "virtual void ice_throw() const;";
    C << sp << nl << "void" << nl << scoped.substr(2) << "::ice_throw() const";
    C << sb;
    C << nl << "throw *this;";
    C << eb;

    if(!p->isLocal())
    {
	H << sp << nl << "static const ::IceInternal::UserExceptionFactoryPtr& ice_factory();";
    }
    return true;
}

void
Slice::Gen::TypesVisitor::visitExceptionEnd(const ExceptionPtr& p)
{
    string name = fixKwd(p->name());
    string scope = fixKwd(p->scope());
    string scoped = fixKwd(p->scoped());
    DataMemberList dataMembers = p->dataMembers();
    DataMemberList::const_iterator q;
	
    string factoryName;

    if(!p->isLocal())
    {
	ExceptionPtr base = p->base();
    
	H << sp << nl << "virtual void __write(::IceInternal::BasicStream*) const;";
	H << nl << "virtual void __read(::IceInternal::BasicStream*, bool);";

	TypeStringList memberList;
	for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
	{
	    memberList.push_back(make_pair((*q)->type(), (*q)->name()));
	}
	C << sp << nl << "void" << nl << scoped.substr(2) << "::__write(::IceInternal::BasicStream* __os) const";
	C << sb;
	C << nl << "__os->write(::std::string(\"" << p->scoped() << "\"));";
	C << nl << "__os->startWriteSlice();";
	writeMarshalCode(C, memberList, 0);
	C << nl << "__os->endWriteSlice();";
	if(base)
	{
	    C.zeroIndent();
	    C << nl << "#if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug"; // COMPILERBUG
	    C.restoreIndent();
	    C << nl << fixKwd(base->name()) << "::__write(__os);";
	    C.zeroIndent();
	    C << nl << "#else";
	    C.restoreIndent();
	    C << nl << fixKwd(base->scoped()) << "::__write(__os);";
	    C.zeroIndent();
	    C << nl << "#endif";
	    C.restoreIndent();
	}
	C << eb;

	C << sp << nl << "void" << nl << scoped.substr(2) << "::__read(::IceInternal::BasicStream* __is, bool __rid)";
	C << sb;
	C << nl << "if(__rid)";
	C << sb;
	C << nl << "::std::string myId;";
	C << nl << "__is->read(myId);";
	C << eb;
	C << nl << "__is->startReadSlice();";
	writeUnmarshalCode(C, memberList, 0);
	C << nl << "__is->endReadSlice();";
	if(base)
	{
	    C.zeroIndent();
	    C << nl << "#if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug"; // COMPILERBUG
	    C.restoreIndent();
	    C << nl << fixKwd(base->name()) << "::__read(__is, true);";
	    C.zeroIndent();
	    C << nl << "#else";
	    C.restoreIndent();
	    C << nl << fixKwd(base->scoped()) << "::__read(__is, true);";
	    C.zeroIndent();
	    C << nl << "#endif";
	    C.restoreIndent();
	}
	C << eb;

	factoryName = "__F" + p->flattenedScope() + p->name();

	C << sp << nl << "struct " << factoryName << " : public ::IceInternal::UserExceptionFactory";
	C << sb;
	C << sp << nl << "virtual void";
	C << nl << "createAndThrow()";
	C << sb;
	C << nl << "throw " << scoped << "();";
	C << eb;
	C << eb << ';';

	C << sp << nl << "static ::IceInternal::UserExceptionFactoryPtr " << factoryName
	  << "__Ptr = new " << factoryName << ';';

	C << sp << nl << "const ::IceInternal::UserExceptionFactoryPtr&";
	C << nl << scoped.substr(2) << "::ice_factory()";
	C << sb;
	C << nl << "return " << factoryName << "__Ptr;";
	C << eb;

	C << sp << nl << "class " << factoryName << "__Init";
	C << sb;
	C.dec();
	C << nl << "public:";
	C.inc();
	C << sp << nl << factoryName << "__Init()";
	C << sb;
	C << nl << "::Ice::factoryTable->addExceptionFactory(\"" << p->scoped() << "\", " << scoped
	  << "::ice_factory());";
	C << eb;
	C << sp << nl << "~" << factoryName << "__Init()";
	C << sb;
	C << nl << "::Ice::factoryTable->removeExceptionFactory(\"" << p->scoped() << "\");";
	C << eb;
	C << eb << ';';
	C << sp << nl << "static " << factoryName << "__Init "<< factoryName << "__i;";
	C << sp << nl << "#ifdef __APPLE__";
	
	string initfuncname = "__F" + p->flattenedScope() + p->name() + "__initializer";
	C << nl << "extern \"C\" { void " << initfuncname << "() {} }";
	C << nl << "#endif";
    }
    H << eb << ';';

    if(!p->isLocal())
    {
	H << sp << nl << "static " << name << " __" << p->name() << "_init;";
    }
}

bool
Slice::Gen::TypesVisitor::visitStructStart(const StructPtr& p)
{
    string name = fixKwd(p->name());

    H << sp << nl << "struct " << name;
    H << sb;

    return true;
}

void
Slice::Gen::TypesVisitor::visitStructEnd(const StructPtr& p)
{
    string name = fixKwd(p->name());
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());

    H << sp;
    H << nl << _dllExport << "bool operator==(const " << name << "&) const;";
    H << nl << _dllExport << "bool operator!=(const " << name << "&) const;";
    H << nl << _dllExport << "bool operator<(const " << name << "&) const;";
    
    DataMemberList dataMembers = p->dataMembers();
    DataMemberList::const_iterator q;
    C << sp << nl << "bool" << nl << scoped.substr(2) << "::operator==(const " << name << "& __rhs) const";
    C << sb;
    C << nl << "return !operator!=(__rhs);";
    C << eb;
    C << sp << nl << "bool" << nl << scoped.substr(2) << "::operator!=(const " << name << "& __rhs) const";
    C << sb;
    C << nl << "if(this == &__rhs)";
    C << sb;
    C << nl << "return false;";
    C << eb;
    for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
	C << nl << "if(" << fixKwd((*q)->name()) << " != __rhs." << fixKwd((*q)->name()) << ')';
	C << sb;
	C << nl << "return true;";
	C << eb;
    }
    C << nl << "return false;";
    C << eb;
    C << sp << nl << "bool" << nl << scoped.substr(2) << "::operator<(const " << name << "& __rhs) const";
    C << sb;
    C << nl << "if(this == &__rhs)";
    C << sb;
    C << nl << "return false;";
    C << eb;
    for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
	C << nl << "if(" << fixKwd((*q)->name()) << " < __rhs." << fixKwd((*q)->name()) << ')';
	C << sb;
	C << nl << "return true;";
	C << eb;
	C << nl << "else if(__rhs." << fixKwd((*q)->name()) << " < " << fixKwd((*q)->name()) << ')';
	C << sb;
	C << nl << "return false;";
	C << eb;
    }
    C << nl << "return false;";
    C << eb;
    
    if(!p->isLocal())
    {
	//
	// None of these member functions is virtual!
	//
	H << sp << nl << _dllExport << "void __write(::IceInternal::BasicStream*) const;";
	H << nl << _dllExport << "void __read(::IceInternal::BasicStream*);";

	TypeStringList memberList;
	for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
	{
	    memberList.push_back(make_pair((*q)->type(), (*q)->name()));
	}
	C << sp << nl << "void" << nl << scoped.substr(2) << "::__write(::IceInternal::BasicStream* __os) const";
	C << sb;
	writeMarshalCode(C, memberList, 0);
	C << eb;

	C << sp << nl << "void" << nl << scoped.substr(2) << "::__read(::IceInternal::BasicStream* __is)";
	C << sb;
	writeUnmarshalCode(C, memberList, 0);
	C << eb;
    }

    H << eb << ';';
}

void
Slice::Gen::TypesVisitor::visitDataMember(const DataMemberPtr& p)
{
    string name = fixKwd(p->name());
    string s = typeToString(p->type());
    H << sp << nl << s << ' ' << name << ';';
}

void
Slice::Gen::TypesVisitor::visitSequence(const SequencePtr& p)
{
    string name = fixKwd(p->name());
    TypePtr type = p->type();
    string s = typeToString(type);
    if(s[0] == ':')
    {
	s.insert(0, " ");
    }
    H << sp << nl << "typedef ::std::vector<" << s << "> " << name << ';';

    BuiltinPtr builtin = BuiltinPtr::dynamicCast(type);
    if(!p->isLocal() &&
	(!builtin || builtin->kind() == Builtin::KindObject || builtin->kind() == Builtin::KindObjectProxy))
    {
	string scoped = fixKwd(p->scoped());
	string scope = fixKwd(p->scope());

	H << sp << nl << "class __U__" << name << " { };";
	H << nl << _dllExport << "void __write(::IceInternal::BasicStream*, const " << name << "&, __U__" << name
	  << ");";
	H << nl << _dllExport << "void __read(::IceInternal::BasicStream*, " << name << "&, __U__" << name << ");";

	C << sp << nl << "void" << nl << scope.substr(2) << "__write(::IceInternal::BasicStream* __os, const "
	  << scoped << "& v, " << scope << "__U__" << name << ")";
	C << sb;
	C << nl << "__os->writeSize(::Ice::Int(v.size()));";
	C << nl << scoped << "::const_iterator p;";
	C << nl << "for(p = v.begin(); p != v.end(); ++p)";
	C << sb;
	writeMarshalUnmarshalCode(C, type, "(*p)", true);
	C << eb;
	C << eb;

	C << sp << nl << "void" << nl << scope.substr(2) << "__read(::IceInternal::BasicStream* __is, " << scoped
	  << "& v, " << scope << "__U__" << name << ')';
	C << sb;
	C << nl << "::Ice::Int sz;";
	C << nl << "__is->readSize(sz);";
	if(type->isVariableLength())
	{
	    C << nl << "__is->startSeq(sz, " << type->minWireSize() << ");"; // Protect against bogus sequence sizes.
	}
	else
	{
	    C << nl << "__is->checkFixedSeq(sz, " << type->minWireSize() << ");";
	}
	C << nl << "v.resize(sz);";
	C << nl << "for(int i = 0; i < sz; ++i)";
	C << sb;
	writeMarshalUnmarshalCode(C, type, "v[i]", false);

	//
	// After unmarshaling each element, check that there are still enough bytes left in the stream
	// to unmarshal the remainder of the sequence, and decrement the count of elements
	// yet to be unmarshaled for sequences with variable-length element type (that is, for sequences
	// of classes, structs, dictionaries, sequences, strings, or proxies). This allows us to
	// abort unmarshaling for bogus sequence sizes at the earliest possible moment.
	// (For fixed-length sequences, we don't need to do this because the prediction of how many
	// bytes will be taken up by the sequence is accurate.)
	//
	if(type->isVariableLength())
	{
	    if(!SequencePtr::dynamicCast(type))
	    {
		//
		// No need to check for directly nested sequences because, at the start of each
		// sequence, we check anyway.
		//
		C << nl << "__is->checkSeq();";
	    }
	    C << nl << "__is->endElement();";
	}
	C << eb;
	if(type->isVariableLength())
	{
	    C << nl << "__is->endSeq(sz);";
	}
	C << eb;
    }
}

void
Slice::Gen::TypesVisitor::visitDictionary(const DictionaryPtr& p)
{
    string name = fixKwd(p->name());
    TypePtr keyType = p->keyType();
    TypePtr valueType = p->valueType();
    string ks = typeToString(keyType);
    if(ks[0] == ':')
    {
	ks.insert(0, " ");
    }
    string vs = typeToString(valueType);
    H << sp << nl << "typedef ::std::map<" << ks << ", " << vs << "> " << name << ';';

    if(!p->isLocal())
    {
	string scoped = fixKwd(p->scoped());
	string scope = fixKwd(p->scope());

	H << sp << nl << "class __U__" << name << " { };";
	H << nl << _dllExport << "void __write(::IceInternal::BasicStream*, const " << name
	  << "&, __U__" << name << ");";
	H << nl << _dllExport << "void __read(::IceInternal::BasicStream*, " << name
	  << "&, __U__" << name << ");";

	C << sp << nl << "void" << nl << scope.substr(2) << "__write(::IceInternal::BasicStream* __os, const "
	  << scoped << "& v, " << scope << "__U__" << name << ")";
	C << sb;
	C << nl << "__os->writeSize(::Ice::Int(v.size()));";
	C << nl << scoped << "::const_iterator p;";
	C << nl << "for(p = v.begin(); p != v.end(); ++p)";
	C << sb;
	writeMarshalUnmarshalCode(C, keyType, "p->first", true);
	writeMarshalUnmarshalCode(C, valueType, "p->second", true);
	C << eb;
	C << eb;

	C << sp << nl << "void" << nl << scope.substr(2) << "__read(::IceInternal::BasicStream* __is, " << scoped
	  << "& v, " << scope << "__U__" << name << ')';
	C << sb;
	C << nl << "::Ice::Int sz;";
	C << nl << "__is->readSize(sz);";
	C << nl << "while(sz--)";
	C << sb;
	C << nl << "::std::pair<const " << ks << ", " << vs << "> pair;";
	string pf = string("const_cast<") + ks + "&>(pair.first)";
	writeMarshalUnmarshalCode(C, keyType, pf, false);
	C << nl << scoped << "::iterator __i = v.insert(v.end(), pair);";
	writeMarshalUnmarshalCode(C, valueType, "__i->second", false);
	C << eb;
	C << eb;
    }
}

void
Slice::Gen::TypesVisitor::visitEnum(const EnumPtr& p)
{
    string name = fixKwd(p->name());
    EnumeratorList enumerators = p->getEnumerators();
    H << sp << nl << "enum " << name;
    H << sb;
    EnumeratorList::const_iterator en = enumerators.begin();
    while(en != enumerators.end())
    {
	H << nl << fixKwd((*en)->name());
	if(++en != enumerators.end())
	{
	    H << ',';
	}
    }
    H << eb << ';';

    if(!p->isLocal())
    {
	string scoped = fixKwd(p->scoped());
	string scope = fixKwd(p->scope());
	
	size_t sz = enumerators.size();
	assert(sz <= 0x7fffffff); // 64-bit enums are not supported
	
	H << sp << nl << _dllExport << "void __write(::IceInternal::BasicStream*, " << name << ");";
	H << nl << _dllExport << "void __read(::IceInternal::BasicStream*, " << name << "&);";

	C << sp << nl << "void" << nl << scope.substr(2) << "__write(::IceInternal::BasicStream* __os, " << scoped
	  << " v)";
	C << sb;
	if(sz <= 0x7f)
	{
	    C << nl << "__os->write(static_cast< ::Ice::Byte>(v));";
	}
	else if(sz <= 0x7fff)
	{
	    C << nl << "__os->write(static_cast< ::Ice::Short>(v));";
	}
	else
	{
	    C << nl << "__os->write(static_cast< ::Ice::Int>(v));";
	}
	C << eb;

	C << sp << nl << "void" << nl << scope.substr(2) << "__read(::IceInternal::BasicStream* __is, " << scoped
	  << "& v)";
	C << sb;
	if(sz <= 0x7f)
	{
	    C << nl << "::Ice::Byte val;";
	    C << nl << "__is->read(val);";
	    C << nl << "v = static_cast< " << scoped << ">(val);";
	}
	else if(sz <= 0x7fff)
	{
	    C << nl << "::Ice::Short val;";
	    C << nl << "__is->read(val);";
	    C << nl << "v = static_cast< " << scoped << ">(val);";
	}
	else
	{
	    C << nl << "::Ice::Int val;";
	    C << nl << "__is->read(val);";
	    C << nl << "v = static_cast< " << scoped << ">(val);";
	}
	C << eb;
    }
}

void
Slice::Gen::TypesVisitor::visitConst(const ConstPtr& p)
{
    H << sp;
    H << nl << "const " << typeToString(p->type()) << " " << fixKwd(p->name()) << " = ";

    BuiltinPtr bp = BuiltinPtr::dynamicCast(p->type());
    if(bp && bp->kind() == Builtin::KindString)
    {
	//
	// Expand strings into the basic source character set. We can't use isalpha() and the like
	// here because they are sensitive to the current locale.
	//
	static const string basicSourceChars = "abcdefghijklmnopqrstuvwxyz"
					       "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					       "0123456789"
					       "_{}[]#()<>%:;,?*+=/^&|~!=,\\\"' \t";
    	static const set<char> charSet(basicSourceChars.begin(), basicSourceChars.end());

	H << "\"";					// Opening "

	ios_base::fmtflags originalFlags = H.flags();	// Save stream state
	streamsize originalWidth = H.width();
	ostream::char_type originalFill = H.fill();

	const string val = p->value();
	for(string::const_iterator c = val.begin(); c != val.end(); ++c)
	{
	    if(charSet.find(*c) == charSet.end())
	    {
		unsigned char uc = *c;			// char may be signed, so make it positive
		H << "\\";    				// Print as octal if not in basic source character set
		H.flags(ios_base::oct);
		H.width(3);
		H.fill('0');
		H << static_cast<unsigned>(uc);
	    }
	    else
	    {
		H << *c;				// Print normally if in basic source character set
	    }
	}

	H.fill(originalFill);				// Restore stream state
	H.width(originalWidth);
	H.flags(originalFlags);

	H << "\"";					// Closing "
    }
    else if(bp && bp->kind() == Builtin::KindLong)
    {
	H << "ICE_INT64(" << p->value() << ")";
    }
    else
    {
	EnumPtr ep = EnumPtr::dynamicCast(p->type());
	if(ep)
	{
	    H << fixKwd(p->value());
	}
	else
	{
	    H << p->value();
	}
    }

    H << ';';
}

Slice::Gen::ProxyDeclVisitor::ProxyDeclVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport)
{
}

bool
Slice::Gen::ProxyDeclVisitor::visitUnitStart(const UnitPtr& p)
{
    if(!p->hasNonLocalClassDecls())
    {
	return false;
    }

    H << sp << nl << "namespace IceProxy" << nl << '{';

    return true;
}

void
Slice::Gen::ProxyDeclVisitor::visitUnitEnd(const UnitPtr& p)
{
    H << sp << nl << '}';
}
    
bool
Slice::Gen::ProxyDeclVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDecls())
    {
	return false;
    }

    string name = fixKwd(p->name());
    
    H << sp << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::ProxyDeclVisitor::visitModuleEnd(const ModulePtr& p)
{
    H << sp << nl << '}';
}

void
Slice::Gen::ProxyDeclVisitor::visitClassDecl(const ClassDeclPtr& p)
{
    if(p->isLocal())
    {
	return;
    }

    string name = fixKwd(p->name());

    H << sp << nl << "class " << name << ';';
    H << nl << _dllExport << "bool operator==(const " << name << "&, const " << name << "&);";
    H << nl << _dllExport << "bool operator!=(const " << name << "&, const " << name << "&);";
    H << nl << _dllExport << "bool operator<(const " << name << "&, const " << name << "&);";
}

Slice::Gen::ProxyVisitor::ProxyVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport)
{
}

bool
Slice::Gen::ProxyVisitor::visitUnitStart(const UnitPtr& p)
{
    if(!p->hasNonLocalClassDecls())
    {
	return false;
    }

    H << sp << nl << "namespace IceProxy" << nl << '{';

    return true;
}

void
Slice::Gen::ProxyVisitor::visitUnitEnd(const UnitPtr& p)
{
    H << sp << nl << '}';
}
    
bool
Slice::Gen::ProxyVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDecls())
    {
	return false;
    }

    string name = fixKwd(p->name());
    
    H << sp << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::ProxyVisitor::visitModuleEnd(const ModulePtr& p)
{
    H << sp << nl << '}';
}

bool
Slice::Gen::ProxyVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(p->isLocal())
    {
	return false;
    }

    string name = fixKwd(p->name());
    string scoped = fixKwd(p->scoped());
    ClassList bases = p->bases();

    H << sp << nl << "class " << _dllExport << name << " : ";
    if(bases.empty())
    {
	H << "virtual public ::IceProxy::Ice::Object";
    }
    else
    {
	H.useCurrentPosAsIndent();
	ClassList::const_iterator q = bases.begin();
	while(q != bases.end())
	{
	    H << "virtual public ::IceProxy" << fixKwd((*q)->scoped());
	    if(++q != bases.end())
	    {
		H << ',' << nl;
	    }
	}
	H.restoreIndent();
    }

    H << sb;
    H.dec();
    H << nl << "public:";
    H.inc();

    return true;
}

void
Slice::Gen::ProxyVisitor::visitClassDefEnd(const ClassDefPtr& p)
{
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());
    
    H << nl << nl << "static const ::std::string& ice_staticId();";
    
    H.dec();
    H << sp << nl << "private: ";
    H.inc();
    H << sp << nl << "virtual ::IceInternal::Handle< ::IceDelegate::Ice::Object> __createDelegate();";
    H << eb << ';';

    C << sp;
    C << nl << "const ::std::string&" << nl << "IceProxy" << scoped << "::ice_staticId()";
    C << sb;
    C << nl << "return "<< scoped << "::ice_staticId();";
    C << eb;

    C << sp << nl << "::IceInternal::Handle< ::IceDelegate::Ice::Object>";
    C << nl << "IceProxy" << scoped << "::__createDelegate()";
    C << sb;
    C << nl << "return ::IceInternal::Handle< ::IceDelegate::Ice::Object>(new ::IceDelegate" << scoped << ");";
    C << eb;

    C << sp;
    C << nl << "bool" << nl << "IceProxy" << scope << "operator==(const ::IceProxy" << scoped
      << "& l, const ::IceProxy" << scoped << "& r)";
    C << sb;
    C << nl << "return static_cast<const ::IceProxy::Ice::Object&>(l) == "
      << "static_cast<const ::IceProxy::Ice::Object&>(r);";
    C << eb;
    C << sp;
    C << nl << "bool" << nl << "IceProxy" << scope << "operator!=(const ::IceProxy" << scoped
      << "& l, const ::IceProxy" << scoped << "& r)";
    C << sb;
    C << nl << "return static_cast<const ::IceProxy::Ice::Object&>(l) != "
      << "static_cast<const ::IceProxy::Ice::Object&>(r);";
    C << eb;
    C << sp;
    C << nl << "bool" << nl << "IceProxy" << scope << "operator<(const ::IceProxy" << scoped
      << "& l, const ::IceProxy" << scoped << "& r)";
    C << sb;
    C << nl << "return static_cast<const ::IceProxy::Ice::Object&>(l) < "
      << "static_cast<const ::IceProxy::Ice::Object&>(r);";
    C << eb;
}

void
Slice::Gen::ProxyVisitor::visitOperation(const OperationPtr& p)
{
    string name = p->name();
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());

    TypePtr ret = p->returnType();
    string retS = returnTypeToString(ret);

    vector<string> params;
    vector<string> paramsDecl;
    vector<string> args;

    ParamDeclList paramList = p->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
	string paramName = fixKwd((*q)->name());

#if defined(__SUNPRO_CC) && (__SUNPRO_CC==0x550)
	//
	// Work around for Sun CC 5.5 bug #4853566
	//
	string typeString;
	if((*q)->isOutParam())
	{
	    typeString = outputTypeToString((*q)->type());
	}
	else
	{
	    typeString = inputTypeToString((*q)->type());
	}
#else
	string typeString = (*q)->isOutParam() ? outputTypeToString((*q)->type()) : inputTypeToString((*q)->type());
#endif

	params.push_back(typeString);
	paramsDecl.push_back(typeString + ' ' + paramName);
	args.push_back(paramName);
    }

    string thisPointer = fixKwd(scope.substr(0, scope.size() - 2)) + "*";

    H << sp;
    H << nl << retS << ' ' << fixKwd(name) << spar << params << epar << ';';
    H << nl << retS << ' ' << fixKwd(name) << spar << params << "const ::Ice::Context&" << epar << ';';

    C << sp << nl << retS << nl << "IceProxy" << scoped << spar << paramsDecl << epar;
    C << sb;
    C << nl;
    if(ret)
    {
	C << "return ";
    }
    C << fixKwd(name) << spar << args << "__defaultContext()" << epar << ';';
    C << eb;

    C << sp << nl << retS << nl << "IceProxy" << scoped << spar << paramsDecl << "const ::Ice::Context& __ctx" << epar;
    C << sb;
    C << nl << "int __cnt = 0;";
    C << nl << "while(true)";
    C << sb;
    C << nl << "try";
    C << sb;
    if(p->returnsData())
    {
        C << nl << "__checkTwowayOnly(\"" << name << "\");";
    }
    C << nl << "::IceInternal::Handle< ::IceDelegate::Ice::Object> __delBase = __getDelegate();";
    C << nl << "::IceDelegate" << thisPointer << " __del = dynamic_cast< ::IceDelegate"
      << thisPointer << ">(__delBase.get());";
    C << nl;
    if(ret)
    {
	C << "return ";
    }
    C << "__del->" << fixKwd(name) << spar << args << "__ctx" << epar << ';';
    if(!ret)
    {
	C << nl << "return;";
    }
    C << eb;
    C << nl << "catch(const ::IceInternal::NonRepeatable& __ex)";
    C << sb;
    if(p->mode() == Operation::Idempotent || p->mode() == Operation::Nonmutating)
    {
	C << nl << "__handleException(*__ex.get(), __cnt);";
    }
    else
    {
	C << nl << "__rethrowException(*__ex.get());";
    }
    C << eb;
    C << nl << "catch(const ::Ice::LocalException& __ex)";
    C << sb;
    C << nl << "__handleException(__ex, __cnt);";
    C << eb;
    C << eb;
    C << eb;
}

Slice::Gen::DelegateVisitor::DelegateVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport)
{
}

bool
Slice::Gen::DelegateVisitor::visitUnitStart(const UnitPtr& p)
{
    if(!p->hasNonLocalClassDecls())
    {
	return false;
    }

    H << sp << nl << "namespace IceDelegate" << nl << '{';

    return true;
}

void
Slice::Gen::DelegateVisitor::visitUnitEnd(const UnitPtr& p)
{
    H << sp << nl << '}';
}
    
bool
Slice::Gen::DelegateVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasNonLocalClassDecls())
    {
	return false;
    }

    string name = fixKwd(p->name());
    
    H << sp << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::DelegateVisitor::visitModuleEnd(const ModulePtr& p)
{
    H << sp << nl << '}';
}

bool
Slice::Gen::DelegateVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(p->isLocal())
    {
	return false;
    }

    string name = fixKwd(p->name());
    ClassList bases = p->bases();

    H << sp << nl << "class " << _dllExport << name << " : ";
    if(bases.empty())
    {
	H << "virtual public ::IceDelegate::Ice::Object";
    }
    else
    {
	H.useCurrentPosAsIndent();
	ClassList::const_iterator q = bases.begin();
	while(q != bases.end())
	{
	    H << "virtual public ::IceDelegate" << fixKwd((*q)->scoped());
	    if(++q != bases.end())
	    {
		H << ',' << nl;
	    }
	}
	H.restoreIndent();
    }
    H << sb;
    H.dec();
    H << nl << "public:";
    H.inc();

    return true;
}

void
Slice::Gen::DelegateVisitor::visitClassDefEnd(const ClassDefPtr& p)
{
    H << eb << ';';
}

void
Slice::Gen::DelegateVisitor::visitOperation(const OperationPtr& p)
{
    string name = fixKwd(p->name());
    string scoped = fixKwd(p->scoped());

    TypePtr ret = p->returnType();
    string retS = returnTypeToString(ret);

    vector<string> params;
    vector<string> paramsDecl;

    TypeStringList inParams;
    TypeStringList outParams;
    ParamDeclList paramList = p->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
	string paramName = fixKwd((*q)->name());
	TypePtr type = (*q)->type();
	bool isOutParam = (*q)->isOutParam();
	string typeString;
	if(isOutParam)
	{
	    outParams.push_back(make_pair(type, paramName));
	    typeString = outputTypeToString(type);
	}
	else
	{
	    inParams.push_back(make_pair(type, paramName));
	    typeString = inputTypeToString(type);
	}

	params.push_back(typeString);
	paramsDecl.push_back(typeString + ' ' + paramName);
    }

    params.push_back("const ::Ice::Context&");
    paramsDecl.push_back("const ::Ice::Context& __context");

    H << sp << nl << "virtual " << retS << ' ' << name << spar << params << epar << ';';
    C << sp << nl << retS << nl << "IceDelegate" << scoped << spar << paramsDecl << epar;
    C << sb;
    C << nl << "static const ::std::string __operation(\"" << p->name() << "\");";
    C << nl << "::IceInternal::Outgoing __out(__connection.get(), __reference.get(), __operation, "
      << "static_cast< ::Ice::OperationMode>(" << p->mode() << "), __context);";
    if(!inParams.empty())
    {
	C << nl << "try";
	C << sb;
	C << nl << "::IceInternal::BasicStream* __os = __out.os();";
	writeMarshalCode(C, inParams, 0);
	C << eb;
	C << nl << "catch(const ::Ice::LocalException& __ex)";
	C << sb;
	C << nl << "__out.abort(__ex);";
	C << eb;
    }
    C << nl << "bool __ok = __out.invoke();";
    C << nl << "try";
    C << sb;
    C << nl << "::IceInternal::BasicStream* __is = __out.is();";
    C << nl << "if(!__ok)";
    C << sb;
    C << nl << "__is->throwException();";
    C << eb;
    writeAllocateCode(C, TypeStringList(), ret);
    writeUnmarshalCode(C, outParams, ret);
    if(ret)
    {
	C << nl << "return __ret;";
    }
    C << eb;

    //
    // Generate a catch block for each legal user exception. This is necessary
    // to prevent an "impossible" user exception to be thrown if client and
    // and server use different exception specifications for an operation. For
    // example:
    //
    // Client compiled with:
    // exception A {};
    // exception B {};
    // interface I {
    //     void op() throws A;
    // };
    //
    // Server compiled with:
    // exception A {};
    // exception B {};
    // interface I {
    //     void op() throws B; // Differs from client
    // };
    //
    // We need the catch blocks so, if the server throws B from op(), the
    // client receives UnknownUserException instead of B.
    //
    ExceptionList throws = p->throws();
    throws.sort();
    throws.unique();
#if defined(__SUNPRO_CC)
    throws.sort(derivedToBaseCompare);
#else
    throws.sort(Slice::DerivedToBaseCompare());
#endif
    for(ExceptionList::const_iterator i = throws.begin(); i != throws.end(); ++i)
    {
	string scoped = (*i)->scoped();
	C << nl << "catch(const " << fixKwd((*i)->scoped()) << "&)";
	C << sb;
	C << nl << "throw;";
	C << eb;
    }
    C << nl << "catch(const ::Ice::UserException&)";
    C << sb;
    C << nl << "throw ::Ice::UnknownUserException(__FILE__, __LINE__);";
    C << eb;
    C << nl << "catch(const ::Ice::LocalException& __ex)";
    C << sb;
    C << nl << "throw ::IceInternal::NonRepeatable(__ex);";
    C << eb;
    C << eb;
}

Slice::Gen::ObjectDeclVisitor::ObjectDeclVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport)
{
}

bool
Slice::Gen::ObjectDeclVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasClassDecls())
    {
	return false;
    }

    string name = fixKwd(p->name());
    
    H << sp << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::ObjectDeclVisitor::visitModuleEnd(const ModulePtr& p)
{
    H << sp << nl << '}';
}

void
Slice::Gen::ObjectDeclVisitor::visitClassDecl(const ClassDeclPtr& p)
{
    string name = fixKwd(p->name());
    
    H << sp << nl << "class " << name << ';';
    H << nl << _dllExport << "bool operator==(const " << name << "&, const " << name << "&);";
    H << nl << _dllExport << "bool operator!=(const " << name << "&, const " << name << "&);";
    H << nl << _dllExport << "bool operator<(const " << name << "&, const " << name << "&);";
}

Slice::Gen::ObjectVisitor::ObjectVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport)
{
}

bool
Slice::Gen::ObjectVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasClassDefs())
    {
	return false;
    }

    string name = fixKwd(p->name());
    
    H << sp << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::ObjectVisitor::visitModuleEnd(const ModulePtr& p)
{
    H << sp;
    H << nl << '}';
}

bool
Slice::Gen::ObjectVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    string name = fixKwd(p->name());
    ClassList bases = p->bases();

    H << sp;
    H << nl << "class " << _dllExport << name << " : ";
    H.useCurrentPosAsIndent();
    if(bases.empty())
    {
	if(p->isLocal())
	{
	    H << "virtual public ::Ice::LocalObject";
	}
	else
	{
	    H << "virtual public ::Ice::Object";
	}
    }
    else
    {
	ClassList::const_iterator q = bases.begin();
	while(q != bases.end())
	{
	    H << "virtual public " << fixKwd((*q)->scoped());
	    if(++q != bases.end())
	    {
		H << ',' << nl;
	    }
	}
    }
    H.restoreIndent();
    H << sb;
    H.dec();
    H << nl << "public:";
    H.inc();

    if(!p->isLocal())
    {
	ClassList allBases = p->allBases();
	StringList ids;
#if defined(__IBMCPP__) && defined(NDEBUG)
//
// VisualAge C++ 6.0 does not see that ClassDef is a Contained,
// when inlining is on. The code below issues a warning: better
// than an error!
//
	transform(allBases.begin(), allBases.end(), back_inserter(ids), ::IceUtil::constMemFun<string,ClassDef>(&Contained::scoped));
#else
	transform(allBases.begin(), allBases.end(), back_inserter(ids), ::IceUtil::constMemFun(&Contained::scoped));
#endif
	StringList other;
	other.push_back(p->scoped());
	other.push_back("::Ice::Object");
	other.sort();
	ids.merge(other);
	ids.unique();
        StringList::const_iterator firstIter = ids.begin();
        StringList::const_iterator scopedIter = find(ids.begin(), ids.end(), p->scoped());
        assert(scopedIter != ids.end());
        StringList::difference_type scopedPos = ice_distance(firstIter, scopedIter);

	StringList::const_iterator q;

	H << sp;
	H << nl << "virtual bool ice_isA"
	  << "(const ::std::string&, const ::Ice::Current& = ::Ice::Current()) const;";
	H << nl << "virtual ::std::vector< ::std::string> ice_ids"
	  << "(const ::Ice::Current& = ::Ice::Current()) const;";
	H << nl << "virtual const ::std::string& ice_id(const ::Ice::Current& = ::Ice::Current()) const;";
	H << nl << "static const ::std::string& ice_staticId();";

	string flatName = p->flattenedScope() + p->name() + "_ids";

	C << sp;
	C << nl << "static const ::std::string " << flatName << '[' << ids.size() << "] =";
	C << sb;
	q = ids.begin();
	while(q != ids.end())
	{
	    C << nl << '"' << *q << '"';
	    if(++q != ids.end())
	    {
		C << ',';
	    }
	}
	C << eb << ';';

	C << sp;
	C << nl << "bool" << nl << fixKwd(p->scoped()).substr(2)
          << "::ice_isA(const ::std::string& _s, const ::Ice::Current&) const";
	C << sb;
	C << nl << "return ::std::binary_search(" << flatName << ", " << flatName << " + " << ids.size() << ", _s);";
	C << eb;

	C << sp;
	C << nl << "::std::vector< ::std::string>" << nl << fixKwd(p->scoped()).substr(2)
	  << "::ice_ids(const ::Ice::Current&) const";
	C << sb;
	C << nl << "return ::std::vector< ::std::string>(&" << flatName << "[0], &" << flatName
	  << '[' << ids.size() << "]);";
	C << eb;

	C << sp;
	C << nl << "const ::std::string&" << nl << fixKwd(p->scoped()).substr(2)
	  << "::ice_id(const ::Ice::Current&) const";
	C << sb;
	C << nl << "return " << flatName << '[' << scopedPos << "];";
	C << eb;

	C << sp;
	C << nl << "const ::std::string&" << nl << fixKwd(p->scoped()).substr(2) << "::ice_staticId()";
	C << sb;
	C << nl << "return " << flatName << '[' << scopedPos << "];";
	C << eb;
    }

    return true;
}

void
Slice::Gen::ObjectVisitor::visitClassDefEnd(const ClassDefPtr& p)
{
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());

    if(!p->isLocal())
    {	
	ClassList bases = p->bases();
	ClassDefPtr base;
	if(!bases.empty() && !bases.front()->isInterface())
	{
	    base = bases.front();
	}
    
	OperationList allOps = p->allOperations();
	if(!allOps.empty())
	{
	    StringList allOpNames;
#if defined(__IBMCPP__) && defined(NDEBUG)
//
// See comment for transform above
//
	    transform(allOps.begin(), allOps.end(), back_inserter(allOpNames),
		      ::IceUtil::constMemFun<string,Operation>(&Contained::name));
#else
	    transform(allOps.begin(), allOps.end(), back_inserter(allOpNames),
                      ::IceUtil::constMemFun(&Contained::name));
#endif
	    allOpNames.push_back("ice_id");
	    allOpNames.push_back("ice_ids");
	    allOpNames.push_back("ice_isA");
	    allOpNames.push_back("ice_ping");
	    allOpNames.sort();
	    allOpNames.unique();

	    StringList::const_iterator q;
	    
	    H << sp;
	    H << nl
	      << "virtual ::IceInternal::DispatchStatus __dispatch(::IceInternal::Incoming&, const ::Ice::Current&);";

	    string flatName = p->flattenedScope() + p->name() + "_all";
	    C << sp;
	    C << nl << "static ::std::string " << flatName << "[] =";
	    C << sb;
	    q = allOpNames.begin();
	    while(q != allOpNames.end())
	    {
		C << nl << '"' << *q << '"';
		if(++q != allOpNames.end())
		{
		    C << ',';
		}
	    }
	    C << eb << ';';
	    C << sp;
	    C << nl << "::IceInternal::DispatchStatus" << nl << scoped.substr(2)
	      << "::__dispatch(::IceInternal::Incoming& in, const ::Ice::Current& current)";
	    C << sb;
	  
	    C << nl << "::std::pair< ::std::string*, ::std::string*> r = "
	      << "::std::equal_range(" << flatName << ", " << flatName << " + " << allOpNames.size()
	      << ", current.operation);";
	    C << nl << "if(r.first == r.second)";
	    C << sb;
	    C << nl << "return ::IceInternal::DispatchOperationNotExist;";
	    C << eb;
	    C << sp;
	    C << nl << "switch(r.first - " << flatName << ')';
	    C << sb;
	    int i = 0;
	    for(q = allOpNames.begin(); q != allOpNames.end(); ++q)
	    {
		C << nl << "case " << i++ << ':';
		C << sb;
		C << nl << "return ___" << *q << "(in, current);";
		C << eb;
	    }
	    C << eb;
	    C << sp;
	    C << nl << "assert(false);";
	    C << nl << "return ::IceInternal::DispatchOperationNotExist;";
	    C << eb;
	}
	
	H << sp;
	H << nl << "virtual void __write(::IceInternal::BasicStream*) const;";
	H << nl << "virtual void __read(::IceInternal::BasicStream*, bool);";

	TypeStringList memberList;
	DataMemberList dataMembers = p->dataMembers();
	for(DataMemberList::const_iterator q = dataMembers.begin(); q != dataMembers.end(); ++q)
	{
	    memberList.push_back(make_pair((*q)->type(), (*q)->name()));
	}
	C << sp;
	C << nl << "void" << nl << scoped.substr(2)
          << "::__write(::IceInternal::BasicStream* __os) const";
	C << sb;
	C << nl << "__os->writeTypeId(ice_staticId());";
	C << nl << "__os->startWriteSlice();";
	writeMarshalCode(C, memberList, 0);
	C << nl << "__os->endWriteSlice();";
	C.zeroIndent();
	C << nl << "#if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug"; // COMPILERBUG
	C.restoreIndent();
	C << nl << (base ? fixKwd(base->name()) : "Object") << "::__write(__os);";
	C.zeroIndent();
	C << nl << "#else";
	C.restoreIndent();
	C << nl << (base ? fixKwd(base->scoped()) : "::Ice::Object") << "::__write(__os);";
	C.zeroIndent();
	C << nl << "#endif";
	C.restoreIndent();
	C << eb;
	C << sp;
	C << nl << "void" << nl << scoped.substr(2) << "::__read(::IceInternal::BasicStream* __is, bool __rid)";
	C << sb;
	C << nl << "if(__rid)";
	C << sb;
	C << nl << "::std::string myId;";
	C << nl << "__is->readTypeId(myId);";
	C << eb;
	C << nl << "__is->startReadSlice();";
	writeUnmarshalCode(C, memberList, 0);
	C << nl << "__is->endReadSlice();";
	C.zeroIndent();
	C << nl << "#if defined(_MSC_VER) && (_MSC_VER < 1300) // VC++ 6 compiler bug"; // COMPILERBUG
	C.restoreIndent();
	C << nl << (base ? fixKwd(base->name()) : "Object") << "::__read(__is, true);";
	C.zeroIndent();
	C << nl << "#else";
	C.restoreIndent();
	C << nl << (base ? fixKwd(base->scoped()) : "::Ice::Object") << "::__read(__is, true);";
	C.zeroIndent();
	C << nl << "#endif";
	C.restoreIndent();
	C << eb;

    }

    H << eb << ';';

    if(p->isLocal())
    {
	C << sp;
	C << nl << "bool" << nl << scope.substr(2) << "operator==(const " << scoped
	  << "& l, const " << scoped << "& r)";
	C << sb;
	C << nl << "return static_cast<const ::Ice::LocalObject&>(l) == static_cast<const ::Ice::LocalObject&>(r);";
	C << eb;
	C << sp;
	C << nl << "bool" << nl << scope.substr(2) << "operator!=(const " << scoped
	  << "& l, const " << scoped << "& r)";
	C << sb;
	C << nl << "return static_cast<const ::Ice::LocalObject&>(l) != static_cast<const ::Ice::LocalObject&>(r);";
	C << eb;
	C << sp;
	C << nl << "bool" << nl << scope.substr(2) << "operator<(const " << scoped
	  << "& l, const " << scoped << "& r)";
	C << sb;
	C << nl << "return static_cast<const ::Ice::LocalObject&>(l) < static_cast<const ::Ice::LocalObject&>(r);";
	C << eb;
    }
    else
    {
	string name = p->name();

	C << sp;
	C << nl << "bool" << nl << scope.substr(2) << "operator==(const " << scoped
	  << "& l, const " << scoped << "& r)";
	C << sb;
	C << nl << "return static_cast<const ::Ice::Object&>(l) == static_cast<const ::Ice::Object&>(r);";
	C << eb;
	C << sp;
	C << nl << "bool" << nl << scope.substr(2) << "operator!=(const " << scoped
	  << "& l, const " << scoped << "& r)";
	C << sb;
	C << nl << "return static_cast<const ::Ice::Object&>(l) != static_cast<const ::Ice::Object&>(r);";
	C << eb;
	C << sp;
	C << nl << "bool" << nl << scope.substr(2) << "operator<(const " << scoped
	  << "& l, const " << scoped << "& r)";
	C << sb;
	C << nl << "return static_cast<const ::Ice::Object&>(l) < static_cast<const ::Ice::Object&>(r);";
	C << eb;
    }
}

bool
Slice::Gen::ObjectVisitor::visitExceptionStart(const ExceptionPtr&)
{
    return false;
}

bool
Slice::Gen::ObjectVisitor::visitStructStart(const StructPtr&)
{
    return false;
}

void
Slice::Gen::ObjectVisitor::visitOperation(const OperationPtr& p)
{
    string name = p->name();
    string scoped = fixKwd(p->scoped());
    string scope = fixKwd(p->scope());

    TypePtr ret = p->returnType();
    string retS = returnTypeToString(ret);

    string params = "(";
    string paramsDecl = "(";
    string args = "(";

    ContainerPtr container = p->container();
    ClassDefPtr cl = ClassDefPtr::dynamicCast(container);
    string classNameAMD = "AMD_" + cl->name();
    string classScope = fixKwd(cl->scope());
    string classScopedAMD = classScope + classNameAMD;

    string paramsAMD = "(const " + classScopedAMD + '_' + name + "Ptr&, ";
    string paramsDeclAMD = "(const " + classScopedAMD + '_' + name + "Ptr& __cb, ";
    string argsAMD = "(__cb, ";

    TypeStringList inParams;
    TypeStringList outParams;
    ParamDeclList paramList = p->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
	string paramName = fixKwd((*q)->name());
	TypePtr type = (*q)->type();
	bool isOutParam = (*q)->isOutParam();
	string typeString;
	if(isOutParam)
	{
	    outParams.push_back(make_pair(type, paramName));
	    typeString = outputTypeToString(type);
	}
	else
	{
	    inParams.push_back(make_pair(type, paramName));
	    typeString = inputTypeToString((*q)->type());
	}

	if(q != paramList.begin())
	{
	    params += ", ";
	    paramsDecl += ", ";
	    args += ", ";
	}

	params += typeString;
	paramsDecl += typeString;
	paramsDecl += ' ';
	paramsDecl += paramName;
	args += paramName;

	if(!isOutParam)
	{
	    paramsAMD += typeString;
	    paramsAMD += ", ";
	    paramsDeclAMD += typeString;
	    paramsDeclAMD += ' ';
	    paramsDeclAMD += paramName;
	    paramsDeclAMD += ", ";
	    argsAMD += paramName;
	    argsAMD += ", ";
	}
    }

    if(!cl->isLocal())
    {
	if(!paramList.empty())
	{
	    params += ", ";
	    paramsDecl += ", ";
	    args += ", ";
	}

	params += "const ::Ice::Current& = ::Ice::Current())";
	paramsDecl += "const ::Ice::Current& __current)";
	args += "__current)";
    }
    else
    {
	params += ')';
	paramsDecl += ')';
	args += ')';
    }

    paramsAMD += "const ::Ice::Current& = ::Ice::Current())";
    paramsDeclAMD += "const ::Ice::Current& __current)";
    argsAMD += "__current)";
    
    bool nonmutating = p->mode() == Operation::Nonmutating;

    H << sp;
    H << nl << "virtual " << retS << ' ' << fixKwd(name) << params << (nonmutating ? " const" : "") << " = 0;";

    if(!cl->isLocal())
    {
	H << nl << "::IceInternal::DispatchStatus ___" << name
	  << "(::IceInternal::Incoming&, const ::Ice::Current&)" << (nonmutating ? " const" : "") << ';';

	C << sp;
	C << nl << "::IceInternal::DispatchStatus" << nl << scope.substr(2) << "___" << name
	  << "(::IceInternal::Incoming& __in, const ::Ice::Current& __current)" << (nonmutating ? " const" : "");
	C << sb;

	ExceptionList throws = p->throws();
	throws.sort();
	throws.unique();

	//
	// Arrange exceptions into most-derived to least-derived order. If we don't
	// do this, a base exception handler can appear before a derived exception
	// handler, causing compiler warnings and resulting in the base exception
	// being marshaled instead of the derived exception.
	//

#if defined(__SUNPRO_CC)
	throws.sort(derivedToBaseCompare);
#else
	throws.sort(Slice::DerivedToBaseCompare());
#endif

	if(!inParams.empty())
	{
	    C << nl << "::IceInternal::BasicStream* __is = __in.is();";
	}
	if(ret || !outParams.empty() || !throws.empty())
	{
	    C << nl << "::IceInternal::BasicStream* __os = __in.os();";
	}
	writeAllocateCode(C, inParams, 0);
	writeUnmarshalCode(C, inParams, 0);
	writeAllocateCode(C, outParams, 0);
	if(!throws.empty())
	{
	    C << nl << "try";
	    C << sb;
	}
	C << nl;
	if(ret)
	{
	    C << retS << " __ret = ";
	}
	C << fixKwd(name) << args << ';';
	writeMarshalCode(C, outParams, ret);
	if(!throws.empty())
	{
	    C << eb;
	    ExceptionList::const_iterator r;
	    for(r = throws.begin(); r != throws.end(); ++r)
	    {
	        C << nl << "catch(const " << fixKwd((*r)->scoped()) << "& __ex)";
	        C << sb;
	        C << nl << "__os->write(__ex);";
	        C << nl << "return ::IceInternal::DispatchUserException;";
	        C << eb;
	    }
	}
	C << nl << "return ::IceInternal::DispatchOK;";
	C << eb;
    }	
}

void
Slice::Gen::ObjectVisitor::visitDataMember(const DataMemberPtr& p)
{
    string name = fixKwd(p->name());
    string s = typeToString(p->type());
    H << sp;
    H << nl << s << ' ' << name << ';';
}

Slice::Gen::IceInternalVisitor::IceInternalVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport)
{
}

bool
Slice::Gen::IceInternalVisitor::visitUnitStart(const UnitPtr& p)
{
    if(!p->hasClassDecls())
    {
	return false;
    }

    H << sp;
    H << nl << "namespace IceInternal" << nl << '{';

    return true;
}

void
Slice::Gen::IceInternalVisitor::visitUnitEnd(const UnitPtr& p)
{
    H << sp;
    H << nl << '}';
}
    
void
Slice::Gen::IceInternalVisitor::visitClassDecl(const ClassDeclPtr& p)
{
    string scoped = fixKwd(p->scoped());
    
    H << sp;
    H << nl << _dllExport << "void incRef(" << scoped << "*);";
    H << nl << _dllExport << "void decRef(" << scoped << "*);";
    if(!p->isLocal())
    {
	H << sp;
	H << nl << _dllExport << "void incRef(::IceProxy" << scoped << "*);";
	H << nl << _dllExport << "void decRef(::IceProxy" << scoped << "*);";
    }
}

bool
Slice::Gen::IceInternalVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    string scoped = fixKwd(p->scoped());
    
    C << sp;
    C << nl << "void" << nl << "IceInternal::incRef(" << scoped << "* p)";
    C << sb;
    C << nl << "p->__incRef();";
    C << eb;

    C << sp;
    C << nl << "void" << nl << "IceInternal::decRef(" << scoped << "* p)";
    C << sb;
    C << nl << "p->__decRef();";
    C << eb;

    if(!p->isLocal())
    {
	C << sp;
	C << nl << "void" << nl << "IceInternal::incRef(::IceProxy" << scoped << "* p)";
	C << sb;
	C << nl << "p->__incRef();";
	C << eb;

	C << sp;
	C << nl << "void" << nl << "IceInternal::decRef(::IceProxy" << scoped << "* p)";
	C << sb;
	C << nl << "p->__decRef();";
	C << eb;
    }

    return true;
}

Slice::Gen::HandleVisitor::HandleVisitor(Output& h, Output& c, const string& dllExport) :
    H(h), C(c), _dllExport(dllExport)
{
}

bool
Slice::Gen::HandleVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasClassDecls())
    {
	return false;
    }

    string name = fixKwd(p->name());
    
    H << sp;
    H << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::HandleVisitor::visitModuleEnd(const ModulePtr& p)
{
    H << sp;
    H << nl << '}';
}

void
Slice::Gen::HandleVisitor::visitClassDecl(const ClassDeclPtr& p)
{
    string name = p->name();
    string scoped = fixKwd(p->scoped());
    
    H << sp;
    H << nl << "typedef ::IceInternal::Handle< " << scoped << "> " << name << "Ptr;";
    if(!p->isLocal())
    {
	H << nl << "typedef ::IceInternal::ProxyHandle< ::IceProxy" << scoped << "> " << name << "Prx;";

	H << sp;
	H << nl << _dllExport << "void __write(::IceInternal::BasicStream*, const " << name << "Prx&);";
	H << nl << _dllExport << "void __read(::IceInternal::BasicStream*, " << name << "Prx&);";
	H << nl << _dllExport << "void __write(::IceInternal::BasicStream*, const " << name << "Ptr&);";
	H << nl << _dllExport << "void __patch__" << name << "Ptr(void*, ::Ice::ObjectPtr&);";
    }
}

bool
Slice::Gen::HandleVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(!p->isLocal())
    {
        string name = p->name();
	string scoped = fixKwd(p->scoped());
	string scope = fixKwd(p->scope());

	string factory;
	string type;
	if(!p->isAbstract())
	{
	    type = scoped + "::ice_staticId()";
	    factory = scoped + "::ice_factory()";
	}
	else
	{
	    type = "\"\"";
	    factory = "0";
	}

	C << sp;
	C << nl << "void" << nl << scope.substr(2) << "__write(::IceInternal::BasicStream* __os, const "
	  << scope << name << "Prx& v)";
	C << sb;
	C << nl << "__os->write(::Ice::ObjectPrx(v));";
	C << eb;

	C << sp;
	C << nl << "void" << nl << scope.substr(2) << "__read(::IceInternal::BasicStream* __is, "
	  << scope << name << "Prx& v)";
	C << sb;
	C << nl << "::Ice::ObjectPrx proxy;";
	C << nl << "__is->read(proxy);";
	C << nl << "if(!proxy)";
	C << sb;
	C << nl << "v = 0;";
	C << eb;
	C << nl << "else";
	C << sb;
	C << nl << "v = new ::IceProxy" << scoped << ';';
	C << nl << "v->__copyFrom(proxy);";
	C << eb;
	C << eb;

	C << sp;
	C << nl << "void" << nl << scope.substr(2) << "__write(::IceInternal::BasicStream* __os, const "
	   << scope << name << "Ptr& v)";
	C << sb;
	C << nl << "__os->write(::Ice::ObjectPtr(v));";
	C << eb;
    }

    return true;
}

Slice::Gen::ImplVisitor::ImplVisitor(Output& h, Output& c,
                                     const string& dllExport) :
    H(h), C(c), _dllExport(dllExport)
{
}

void
Slice::Gen::ImplVisitor::writeDecl(Output& out, const string& name, const TypePtr& type)
{
    out << nl << typeToString(type) << ' ' << name;

    BuiltinPtr builtin = BuiltinPtr::dynamicCast(type);
    if(builtin)
    {
        switch(builtin->kind())
        {
            case Builtin::KindBool:
            {
                out << " = false";
                break;
            }
            case Builtin::KindByte:
            case Builtin::KindShort:
            case Builtin::KindInt:
            case Builtin::KindLong:
            {
                out << " = 0";
                break;
            }
            case Builtin::KindFloat:
            case Builtin::KindDouble:
            {
                out << " = 0.0";
                break;
            }
	    case Builtin::KindObject: 
            case Builtin::KindString:
            case Builtin::KindObjectProxy:
            case Builtin::KindLocalObject:
            {
                break;
            }
        }
    }

    EnumPtr en = EnumPtr::dynamicCast(type);
    if(en)
    {
        EnumeratorList enumerators = en->getEnumerators();
        out << " = " << fixKwd(en->scope()) << fixKwd(enumerators.front()->name());
    }

    out << ';';
}

void
Slice::Gen::ImplVisitor::writeReturn(Output& out, const TypePtr& type)
{
    BuiltinPtr builtin = BuiltinPtr::dynamicCast(type);
    if(builtin)
    {
        switch(builtin->kind())
        {
            case Builtin::KindBool:
            {
                out << nl << "return false;";
                break;
            }
            case Builtin::KindByte:
            case Builtin::KindShort:
            case Builtin::KindInt:
            case Builtin::KindLong:
            {
                out << nl << "return 0;";
                break;
            }
            case Builtin::KindFloat:
            case Builtin::KindDouble:
            {
                out << nl << "return 0.0;";
                break;
            }
            case Builtin::KindString:
            {
                out << nl << "return ::std::string();";
                break;
            }
	    case Builtin::KindObject: 
	    case Builtin::KindObjectProxy:
	    case Builtin::KindLocalObject:
            {
                out << nl << "return 0;";
                break;
            }
        }
    }
    else
    {
        ProxyPtr prx = ProxyPtr::dynamicCast(type);
        if(prx)
	{
	    out << nl << "return 0;";
	}
	else
	{
	    ClassDeclPtr cl = ClassDeclPtr::dynamicCast(type);
	    if(cl)
	    {
		out << nl << "return 0;";
	    }
	    else
	    {
		StructPtr st = StructPtr::dynamicCast(type);
		if(st)
		{
		    out << nl << "return " << fixKwd(st->scoped()) << "();";
		}
		else
		{
		    EnumPtr en = EnumPtr::dynamicCast(type);
		    if(en)
		    {
			EnumeratorList enumerators = en->getEnumerators();
			out << nl << "return " << fixKwd(en->scope()) << fixKwd(enumerators.front()->name()) << ';';
		    }
		    else
		    {
			SequencePtr seq = SequencePtr::dynamicCast(type);
			if(seq)
			{
			    out << nl << "return " << fixKwd(seq->scoped()) << "();";
			}
			else
			{
			    DictionaryPtr dict = DictionaryPtr::dynamicCast(type);
			    assert(dict);
			    out << nl << "return " << fixKwd(dict->scoped()) << "();";
			}
		    }
		}
	    }
	}
    }
}

bool
Slice::Gen::ImplVisitor::visitModuleStart(const ModulePtr& p)
{
    if(!p->hasClassDefs())
    {
        return false;
    }

    string name = fixKwd(p->name());
    
    H << sp << nl << "namespace " << name << nl << '{';

    return true;
}

void
Slice::Gen::ImplVisitor::visitModuleEnd(const ModulePtr& p)
{
    H << sp;
    H << nl << '}';
}

bool
Slice::Gen::ImplVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(!p->isAbstract())
    {
        return false;
    }

    string name = p->name();
    string scope = fixKwd(p->scope());
    string cls = scope.substr(2) + name + "I";
    string classScopedAMD = scope + "AMD_" + name;

    ClassList bases = p->bases();
    ClassDefPtr base;
    if(!bases.empty() && !bases.front()->isInterface())
    {
        base = bases.front();
    }

    H << sp;
    H << nl << "class " << name << "I : ";
    H.useCurrentPosAsIndent();
    H << "virtual public " << fixKwd(name);
    for(ClassList::const_iterator q = bases.begin(); q != bases.end(); ++q)
    {
        H << ',' << nl << "virtual public " << fixKwd((*q)->scope());
	if((*q)->isAbstract())
	{
	    H << (*q)->name() << "I";
	}
	else
	{
	    H << fixKwd((*q)->name());
	}
    }
    H.restoreIndent();

    H << sb;
    H.dec();
    H << nl << "public:";
    H.inc();

    OperationList ops = p->operations();
    OperationList::const_iterator r;

    for(r = ops.begin(); r != ops.end(); ++r)
    {
        OperationPtr op = (*r);
        string opName = op->name();

        TypePtr ret = op->returnType();
        string retS = returnTypeToString(ret);

        H << sp << nl << "virtual " << retS << ' ' << fixKwd(opName) << '(';
        H.useCurrentPosAsIndent();
        ParamDeclList paramList = op->parameters();
        ParamDeclList::const_iterator q;
        for(q = paramList.begin(); q != paramList.end(); ++q)
        {
            if(q != paramList.begin())
            {
                H << ',' << nl;
            }
#if defined(__SUNPRO_CC) && (__SUNPRO_CC==0x550)
	    //
	    // Work around for Sun CC 5.5 bug #4853566
	    //
	    string typeString;
	    if((*q)->isOutParam())
	    {
	        typeString = outputTypeToString((*q)->type());
	    }
	    else
	    {
	        typeString = inputTypeToString((*q)->type());
	    }
#else
            string typeString = (*q)->isOutParam() ?
                outputTypeToString((*q)->type()) : inputTypeToString((*q)->type());
#endif
            H << typeString;
        }
        if(!p->isLocal())
        {
            if(!paramList.empty())
            {
                H << ',' << nl;
            }
            H << "const Ice::Current&";
        }
        H.restoreIndent();

        bool nonmutating = op->mode() == Operation::Nonmutating;

        H << ")" << (nonmutating ? " const" : "") << ';';

        C << sp << nl << retS << nl;
	C << scope.substr(2) << name << "I::" << fixKwd(opName) << '(';
        C.useCurrentPosAsIndent();
        for(q = paramList.begin(); q != paramList.end(); ++q)
        {
            if(q != paramList.begin())
            {
                C << ',' << nl;
            }
#if defined(__SUNPRO_CC) && (__SUNPRO_CC==0x550)
	    //
	    // Work around for Sun CC 5.5 bug #4853566
	    //
	    string typeString;
	    if((*q)->isOutParam())
	    {
	        typeString = outputTypeToString((*q)->type());
	    }
	    else
	    {
	        typeString = inputTypeToString((*q)->type());
	    }
#else
            string typeString = (*q)->isOutParam() ?
                outputTypeToString((*q)->type()) : inputTypeToString((*q)->type());
#endif
            C << typeString << ' ' << fixKwd((*q)->name());
        }
        if(!p->isLocal())
        {
            if(!paramList.empty())
            {
                C << ',' << nl;
            }
            C << "const Ice::Current& current";
        }
        C.restoreIndent();
        C << ')';
	C << (nonmutating ? " const" : "");
        C << sb;

        if(ret)
        {
            writeReturn(C, ret);
        }

        C << eb;
    }

    H << eb << ';';

    return true;
}

void
Slice::Gen::validateMetaData(const UnitPtr& unit)
{
    MetaDataVisitor visitor;
    unit->visit(&visitor, false);
}

bool
Slice::Gen::MetaDataVisitor::visitModuleStart(const ModulePtr& p)
{
    validate(p);
    return true;
}

void
Slice::Gen::MetaDataVisitor::visitModuleEnd(const ModulePtr&)
{
}

void
Slice::Gen::MetaDataVisitor::visitClassDecl(const ClassDeclPtr& p)
{
    validate(p);
}

bool
Slice::Gen::MetaDataVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    validate(p);
    return true;
}

void
Slice::Gen::MetaDataVisitor::visitClassDefEnd(const ClassDefPtr&)
{
}

bool
Slice::Gen::MetaDataVisitor::visitExceptionStart(const ExceptionPtr& p)
{
    validate(p);
    return true;
}

void
Slice::Gen::MetaDataVisitor::visitExceptionEnd(const ExceptionPtr&)
{
}

bool
Slice::Gen::MetaDataVisitor::visitStructStart(const StructPtr& p)
{
    validate(p);
    return true;
}

void
Slice::Gen::MetaDataVisitor::visitStructEnd(const StructPtr&)
{
}

void
Slice::Gen::MetaDataVisitor::visitOperation(const OperationPtr& p)
{
    validate(p);
}

void
Slice::Gen::MetaDataVisitor::visitParamDecl(const ParamDeclPtr& p)
{
    validate(p);
}

void
Slice::Gen::MetaDataVisitor::visitDataMember(const DataMemberPtr& p)
{
    validate(p);
}

void
Slice::Gen::MetaDataVisitor::visitSequence(const SequencePtr& p)
{
    validate(p);
}

void
Slice::Gen::MetaDataVisitor::visitDictionary(const DictionaryPtr& p)
{
    validate(p);
}

void
Slice::Gen::MetaDataVisitor::visitEnum(const EnumPtr& p)
{
    validate(p);
}

void
Slice::Gen::MetaDataVisitor::visitConst(const ConstPtr& p)
{
    validate(p);
}

void
Slice::Gen::MetaDataVisitor::validate(const ContainedPtr& cont)
{
    DefinitionContextPtr dc = cont->definitionContext();
    assert(dc);
    StringList globalMetaData = dc->getMetaData();
    string file = dc->filename();

    StringList localMetaData = cont->getMetaData();

    StringList::const_iterator p;
    static const string prefix = "cpp:";

    for(p = globalMetaData.begin(); p != globalMetaData.end(); ++p)
    {
        string s = *p;
        if(_history.count(s) == 0)
        {
            if(s.find(prefix) == 0)
            {
		cout << file << ": warning: ignoring invalid global metadata `" << s << "'" << endl;
            }
            _history.insert(s);
        }
    }

    for(p = localMetaData.begin(); p != localMetaData.end(); ++p)
    {
	string s = *p;
        if(_history.count(s) == 0)
        {
            if(s.find(prefix) == 0)
            {
	    	if(SequencePtr::dynamicCast(cont))
		{
		    if(s.substr(prefix.size()) == "collection")
		    {
			continue;
		    }
		}
		if(StructPtr::dynamicCast(cont))
		{
		    if(s.substr(prefix.size()) == "class")
		    {
		        continue;
		    }
		}
		cout << file << ": warning: ignoring invalid metadata `" << s << "'" << endl;
            }
            _history.insert(s);
        }
    }
}
