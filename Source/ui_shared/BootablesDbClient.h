#pragma once

#include <string>
#include <vector>
#include "filesystem_def.h"
#include "Types.h"
#include "Singleton.h"
#include "sqlite/SqliteDb.h"
#include "sqlite/SqliteStatement.h"

namespace BootablesDb
{
	struct Bootable
	{
		fs::path path;
		std::string discId;
		std::string title;
		std::string coverUrl;
		std::string overview;
		time_t lastBootedTime = 0;
	};

	class CClient : public CSingleton<CClient>
	{
	public:
		//NOTE: This is duplicated in the Android Java code (in BootablesInterop.java) - values matter
		enum SORT_METHOD
		{
			SORT_METHOD_RECENT,
			SORT_METHOD_HOMEBREW,
			SORT_METHOD_NONE,
		};

		CClient();
		virtual ~CClient() = default;

		bool BootableExists(const fs::path&);
		Bootable GetBootable(const fs::path&);
		std::vector<Bootable> GetBootables(int32_t = SORT_METHOD_NONE);

		void RegisterBootable(const fs::path&, const char*, const char*);
		void UnregisterBootable(const fs::path&);

		void SetDiscId(const fs::path&, const char*);
		void SetTitle(const fs::path&, const char*);
		void SetCoverUrl(const fs::path&, const char*);
		void SetLastBootedTime(const fs::path&, time_t);
		void SetOverview(const fs::path& path, const char* overview);

	private:
		static Bootable ReadBootable(Framework::CSqliteStatement&);

		void CheckDbVersion();

		fs::path m_dbPath;
		Framework::CSqliteDb m_db;
	};
};
