#pragma once



#include "config.h"

#include <fstream>
#include <algorithm>
#include <string>

template <typename Type>
class DiskIO
{
public:
	~DiskIO()
	{
		if (this->fd != nullptr)
		{
			fclose(this->fd);
			this->fd = nullptr;
		}
	}

	void init(const char *_fileName, IOTYPE _type = IOTYPE::O_REWR, bool clearFlag = false)
	{
		bool existFlag = existCheck(_fileName);
		if (clearFlag == true && existFlag == true)
		{
			clearFile(_fileName);
		}

		if (existFlag == false)
		{
			this->fd = fopen(_fileName, "wb");
			fclose(this->fd);
			this->fd = nullptr;
			//std::cout << "creat file " << _fileName << "\n";
		}

		this->fd = fopen(_fileName, "rb+");
		if (clearFlag == true || existFlag == false)
		{
			FileHeader header;
			memset(&header, 0, sizeof(FileHeader));
			memcpy(header.fileName, _fileName, std::min(strlen(_fileName), size_t(27)));
			fwrite(&header, sizeof(FileHeader), 1, this->fd);
		}
		this->fileName = _fileName;
		this->type = _type;
		setvbuf(this->fd, NULL, _IONBF, 0);
	}

	bool put(const Type &_Ele, uint64_t _Offset)
	{
		if (this->type == IOTYPE::O_READ)
		{
			return true;
		}
		fseek(this->fd, 0, SEEK_END);
		if (_Offset > ftell(this->fd))
		{
			return false;
		}
		if (true)
		{
			fseek(this->fd, _Offset, SEEK_SET);
			fwrite(&_Ele, sizeof(Type), 1, this->fd);
		}
		return true;
	}

	bool get(Type &_Ele, uint64_t _Offset)
	{
		if (_Offset < FileHeaderOffset)
		{
			return false;
		}
		fseek(this->fd, _Offset, SEEK_SET);
		int rc = fread(&_Ele, sizeof(Type), 1, this->fd);
		return true;
	}

	uint64_t alloc(const Type &ele)
	{
		fseek(this->fd, 0, SEEK_END);
		fpos_t _Offset; // = 0;
		fgetpos(this->fd, &_Offset);
		fwrite(&ele, sizeof(Type), 1, this->fd);
		return _Offset;
	}

	uint64_t dwrite(char *ptr, uint64_t _Len)
	{
		fseek(this->fd, 0, SEEK_END);

		//uint64_t _Offset = ftell(this->fd);	// <=2G
		//uint64_t _Offset = 0;					// >=2G
		fpos_t _Offset; // = 0;
		fgetpos(this->fd, &_Offset);
		fwrite(ptr, _Len, 1, this->fd);
		return _Offset;
	}

	uint64_t dread(char **ptr, uint64_t _Offset)
	{
		fseek(this->fd,0,SEEK_END);
		fpos_t _fOffset;
		fgetpos(this->fd,&_fOffset);
		if(_Offset>=_fOffset){
			return FNUL;
		}

		_fOffset = _Offset;
		fsetpos(this->fd,&_fOffset);

		DeltaPacket delatPacke;
		int rc = fread(&delatPacke,sizeof(DeltaPacket),1,this->fd);
		int packet_len = sizeof(DeltaPacket) + delatPacke.delta_count * sizeof(DeltaItem);
		*ptr = new char[packet_len];
		fsetpos(this->fd,&_fOffset);
		rc = fread(*ptr,packet_len,1,this->fd);

		fgetpos(this->fd,&_fOffset);
		return _fOffset;
	}

	static bool existCheck(const char *fileName)
	{
		FILE *efd = fopen(fileName, "rb");
		bool existFlag = false;
		if (efd != nullptr)
		{
			existFlag = true;
			fclose(efd);
		}
		return existFlag;
	}

	static bool restoreCheck(const char *fileName)
	{
		FILE *refd = fopen(fileName, "rb");
		bool restoreFlag = false;
		if (refd != nullptr)
		{
			fseek(refd, 0, SEEK_END);
			uint64_t len = ftell(refd);
			if (len > FileHeaderOffset)
			{
				restoreFlag = true;
			}
			fclose(refd);
		}
		return restoreFlag;
	}

	static void clearFile(const char *fileName)
	{
		if (existCheck(fileName) == true)
		{
			FILE *clfd = fopen(fileName, "w");
			fclose(clfd);
		}
	}

private:
	IOTYPE type = IOTYPE::O_REWR;
	FILE *fd = nullptr;
	std::string fileName;
};