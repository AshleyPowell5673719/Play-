#pragma once

#include "zip/ZipFile.h"
#include "RegisterState.h"

class CRegisterStateFile : public Framework::CZipFile
{
public:
	CRegisterStateFile(const char*);
	CRegisterStateFile(Framework::CStream&);
	virtual ~CRegisterStateFile() = default;

	void SetRegister32(const char*, uint32);
	void SetRegister64(const char*, uint64);
	void SetRegister128(const char*, uint128);

	uint32 GetRegister32(const char*) const;
	uint64 GetRegister64(const char*) const;
	uint128 GetRegister128(const char*) const;

	void Read(Framework::CStream&);
	void Write(Framework::CStream&) override;

private:
	CRegisterState m_registers;
};
