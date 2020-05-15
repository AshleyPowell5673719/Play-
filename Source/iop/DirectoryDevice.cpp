#include <cassert>
#include "DirectoryDevice.h"
#include "Iop_PathUtils.h"
#include "StdStream.h"
#include "string_cast.h"
#include "../AppConfig.h"

using namespace Iop::Ioman;

CDirectoryDevice::CDirectoryDevice(const char* basePathPreferenceName)
    : m_basePathPreferenceName(basePathPreferenceName)
{
}

template <typename StringType>
Framework::CStdStream* CreateStdStream(const StringType&, const char*);

template <>
Framework::CStdStream* CreateStdStream(const std::string& path, const char* mode)
{
	return new Framework::CStdStream(path.c_str(), mode);
}

template <>
Framework::CStdStream* CreateStdStream(const std::wstring& path, const char* mode)
{
	auto cvtMode = string_cast<std::wstring>(mode);
	return new Framework::CStdStream(path.c_str(), cvtMode.c_str());
}

Framework::CStream* CDirectoryDevice::GetFile(uint32 accessType, const char* devicePath)
{
	auto basePath = CAppConfig::GetInstance().GetPreferencePath(m_basePathPreferenceName.c_str());
	auto path = Iop::PathUtils::MakeHostPath(basePath, devicePath);

	const char* mode = nullptr;
	switch(accessType)
	{
	case 0:
	case OPEN_FLAG_RDONLY:
		mode = "rb";
		break;
	case(OPEN_FLAG_WRONLY | OPEN_FLAG_CREAT):
	case(OPEN_FLAG_WRONLY | OPEN_FLAG_CREAT | OPEN_FLAG_TRUNC):
		mode = "wb";
		break;
	case OPEN_FLAG_RDWR:
		mode = "r+";
		break;
	case(OPEN_FLAG_RDWR | OPEN_FLAG_CREAT):
		mode = "w+";
		break;
	default:
		assert(0);
		break;
	}

	try
	{
		return CreateStdStream(path.native(), mode);
	}
	catch(...)
	{
		return nullptr;
	}
}

Directory CDirectoryDevice::GetDirectory(const char* devicePath)
{
	auto basePath = CAppConfig::GetInstance().GetPreferencePath(m_basePathPreferenceName.c_str());
	auto path = Iop::PathUtils::MakeHostPath(basePath, devicePath);
	if(!fs::is_directory(path))
	{
		throw std::runtime_error("Not a directory.");
	}
	return fs::directory_iterator(path);
}

void CDirectoryDevice::CreateDirectory(const char* devicePath)
{
	auto basePath = CAppConfig::GetInstance().GetPreferencePath(m_basePathPreferenceName.c_str());
	auto path = Iop::PathUtils::MakeHostPath(basePath, devicePath);
	if(!fs::create_directory(path))
	{
		throw std::runtime_error("Failed to create directory.");
	}
}
