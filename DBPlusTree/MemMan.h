#pragma once

#include <unordered_map>
#include "config.h"
#include "SList.h"
#include "DiskIO.h"

template <typename Type>
class MemMan;
template <typename Type>
class Pointer;

template <typename Type>
class Pointer
{
	friend MemMan<Type>;

public:
	~Pointer()
	{
		this->plevel = PointerLevel::PL_NUL;
		this->ID = 0;
		this->mem = nullptr;
		this->memMan = nullptr;
	}

	Pointer() = default;

	Pointer(Type *_Data, uint64_t _ID, PointerLevel _PLevel, MemMan<Type> *_MemMan)
	{
		this->mem = _Data;
		this->ID = _ID;
		this->plevel = _PLevel;
		this->memMan = _MemMan;
	}

	Type *data()
	{
		if (isNULL() == false)
		{
			this->mem = this->memMan->get(this->ID);
			return this->mem;
		}
		return nullptr;
	}

	bool isNULL()
	{
		return this->plevel == PointerLevel::PL_NUL;
	}

	uint64_t offset()
	{
		return this->ID;
	}

private:
	Type *mem = nullptr;
	uint64_t ID = 0;
	PointerLevel plevel = PointerLevel::PL_NUL;
	MemMan<Type> *memMan = nullptr;
};

template <typename Type>
class MemMan
{
	friend Pointer<Type>;

public:
	void init(size_t _PoolSize, const char *fileName, IOTYPE _TOType = IOTYPE::O_REWR,bool clearFlag=false)
	{
		this->ele_size = sizeof(Type);
		this->pool_size = _PoolSize;
		this->buf_pool = new char[this->pool_size * this->ele_size];
		char *ptr = this->buf_pool;
		for (size_t i = 0; i < this->pool_size; ++i)
		{
			this->List.push_back((Type *)ptr);
			ptr += this->ele_size;
		}
		this->disk.init(fileName, _TOType,clearFlag);
	}

	void get(Pointer<Type> &Ptr, uint64_t ID)
	{
		if (ID2PtrRegister.count(ID) > 0)
		{
			SListNode<Type> *node = this->ID2PtrRegister[ID];
			Ptr = Pointer<Type>(node->data, ID, PointerLevel::PL_USE, this);
			this->List.trnasToEnd(node);
		}
		else
		{
			// LRU 置换
			// step 1: 将List第一个节点压入尾部
			SListNode<Type> *node = this->List.front();
			this->List.trnasToEnd(node);

			// step 2: 检测是否达到内存池最大限制
			if (this->ID2PtrRegister.size() >= this->pool_size)
			{
				// 根据Ptr2ID获取当前占用该位置的ID并注销
				uint64_t freeID = this->Ptr2IDRegister[node->data];
				Pointer<Type> put_Ptr(node->data, freeID, PointerLevel::PL_FRE, this);
				this->put(put_Ptr);
				this->ID2PtrRegister.erase(freeID);
			}
			memset(node->data, 0, this->ele_size);
			// step 3: 将ID和ptr分别注册至相应hash表
			this->Ptr2IDRegister[node->data] = ID;
			this->ID2PtrRegister[ID] = node;
			//*node->data = disk[ID];
			if (this->disk.get(*node->data, ID) == true)
			{
				Ptr = Pointer<Type>(node->data, ID, PointerLevel::PL_USE, this);
			}
			else
			{
				Ptr = Pointer<Type>(nullptr, 0, PointerLevel::PL_NUL, nullptr);
			}
		}
	}

	void alloc(Pointer<Type> &Ptr, const Type &ele)
	{
		uint64_t ID = this->disk.alloc(ele);
		this->get(Ptr, ID);
	}

	void put(Pointer<Type> &Ptr)
	{
		this->disk.put(*Ptr.data(), Ptr.offset());
	}

	uint64_t alloc(char *Ptr, size_t _Len)
	{
		return this->disk.dwrite(Ptr, _Len);
	}

	uint64_t dread(char **ptr, uint64_t _Offset){
		return this->disk.dread(ptr,_Offset);
	}

protected:
	Type *get(uint64_t ID)
	{
		if (ID == 0)
		{
			return nullptr;
		}
		if (ID2PtrRegister.count(ID) > 0)
		{
			return this->ID2PtrRegister[ID]->data;
		}
		else
		{
			SListNode<Type> *node = this->List.front();
			this->List.trnasToEnd(node);

			// step 2: 检测是否达到内存池最大限制
			if (this->ID2PtrRegister.size() >= this->pool_size)
			{
				// 根据Ptr2ID获取当前占用该位置的ID并注销
				uint64_t freeID = this->Ptr2IDRegister[node->data];
				Pointer<Type> put_Ptr(node->data, freeID, PointerLevel::PL_FRE, this);
				this->put(put_Ptr);
				this->ID2PtrRegister.erase(freeID);
			}
			memset(node->data, 0, this->ele_size);
			// step 3: 将ID和ptr分别注册至相应hash表
			this->Ptr2IDRegister[node->data] = ID;
			this->ID2PtrRegister[ID] = node;

			if (this->disk.get(*node->data, ID) == true)
			{
				return node->data;
			}
		}
		return nullptr;
	}

private:
	size_t pool_size = 0; //
	size_t ele_size = 0;
	char *buf_pool = nullptr;
	SList<Type> List;
	std::unordered_map<uint64_t, SListNode<Type> *> ID2PtrRegister;
	std::unordered_map<Type *, uint64_t> Ptr2IDRegister;
	DiskIO<Type> disk;
};
