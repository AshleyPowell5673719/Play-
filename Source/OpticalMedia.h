#pragma once

#include "Stream.h"
#include "ISO9660/ISO9660.h"

class COpticalMedia
{
public:
	COpticalMedia() = default;

	enum CREATE_FLAGS
	{
		CREATE_AUTO_DISABLE_DL_DETECT = 0x01,
	};

	enum TRACK_DATA_TYPE
	{
		TRACK_DATA_TYPE_AUDIO,
		TRACK_DATA_TYPE_MODE1_2048,
		TRACK_DATA_TYPE_MODE2_2352,
	};

	typedef std::shared_ptr<Framework::CStream> StreamPtr;

	static std::unique_ptr<COpticalMedia> CreateAuto(StreamPtr&, uint32 = 0);
	static std::unique_ptr<COpticalMedia> CreateDvd(StreamPtr&, bool = false, uint32 = 0);

	//TODO: Get Track Count
	TRACK_DATA_TYPE GetTrackDataType(uint32) const;
	CISO9660* GetFileSystem();
	CISO9660* GetFileSystemL1();

	bool GetDvdIsDualLayer() const;
	uint32 GetDvdSecondLayerStart() const;

private:
	typedef std::unique_ptr<CISO9660> Iso9660Ptr;

	void CheckDualLayerDvd(const StreamPtr&);
	void SetupSecondLayer(const StreamPtr&);

	TRACK_DATA_TYPE m_track0DataType = TRACK_DATA_TYPE_MODE1_2048;
	bool m_dvdIsDualLayer = false;
	uint32 m_dvdSecondLayerStart = 0;
	Iso9660Ptr m_fileSystem;
	Iso9660Ptr m_fileSystemL1;
};
