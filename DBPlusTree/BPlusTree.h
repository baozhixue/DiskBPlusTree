#pragma once

#include <iostream>
#include <array>
#include <algorithm>
#include <map>
#include <string>
#include <random>
#include <ctime>

#include "config.h"
#include "MemMan.h"

class Node
{
public:
	Node()
	{
		this->keys.fill(0);
		this->childs.fill(FNUL);
		this->Appends.fill(FNUL);
	}

	size_t push(KEY _Key, FPTR _Val, MemMan<DeltaItem>& _Dat, MemMan<Append>& _L2app, bool _Keep = true)
	{
		auto pos = std::lower_bound(this->keys.begin(), this->keys.begin() + this->keys_len, _Key) - this->keys.begin();
		if (this->keys_len > 0 && pos < this->keys_len && this->keys[pos] == _Key)
		{
			if (this->LeafNode == 1)
			{
				if (_Keep == true)
				{
					this->childs[pos + 1] = _Val; // 更新L1层叶节点指向的L2Root信息
				}
				else
				{
					// append，	L2层中同样_Version的数据当多于1个时，自第二个开始将结果保存至Append
					Pointer<Append> pApp;
					Append app;
					Pointer<DeltaItem> pDelta;
					DeltaItem delta;
					_Dat.get(pDelta, _Val);
					delta = *pDelta.data();
					if (this->Appends[pos + 1] == FNUL)
					{
						for (uint8_t i = 0; i < DATA_FIELD_NUM; ++i)
						{
							app.data[i] = delta.delta[i];
						}
						_L2app.alloc(pApp, app);
						this->Appends[pos + 1] = pApp.offset();
					}
					else
					{
						_L2app.get(pApp, this->Appends[pos + 1]);
						app = *pApp.data();
						for (uint8_t i = 0; i < DATA_FIELD_NUM; ++i)
						{
							app.data[i] += delta.delta[i];
						}
						*pApp.data() = app;
					}
				}
			}
			return this->keys_len;
		}
		std::copy_backward(this->keys.begin() + pos, this->keys.begin() + this->keys_len, this->keys.begin() + this->keys_len + 1);
		std::copy_backward(this->childs.begin() + pos + 1, this->childs.begin() + this->keys_len + 1, this->childs.begin() + this->keys_len + 2);

		this->keys[pos] = _Key;
		this->childs[pos + 1] = _Val;

		this->keys_len += 1;
		return this->keys_len;
	}

	std::pair<KEY, Pointer<Node>> split(FPTR selfOffset, MemMan<Node>& idx, MemMan<Append>& _L2app)
	{
		Node Rnode;
		Rnode.LeafNode = this->LeafNode;
		Rnode.father = this->father;

		Pointer<Node> pRnode;
		idx.alloc(pRnode, Rnode); // 为Rnode分配磁盘

		size_t copy_len = MaxM * 0.5;
		size_t midKey = this->keys[copy_len];

		Rnode.nextNode = this->nextNode;
		this->nextNode = pRnode.offset();

		if (this->LeafNode == 1)
		{

			std::copy(this->keys.begin() + copy_len, this->keys.end(), Rnode.keys.begin());
			std::copy(this->childs.begin() + copy_len + 1, this->childs.end(), Rnode.childs.begin() + 1);

			this->keys_len = copy_len;
			Rnode.keys_len = copy_len + 1;
		}
		else
		{
			std::copy(this->keys.begin() + copy_len + 1, this->keys.end(), Rnode.keys.begin());
			std::copy(this->childs.begin() + copy_len + 1, this->childs.end(), Rnode.childs.begin());

			this->keys_len = copy_len;
			Rnode.keys_len = copy_len;

			Pointer<Node> pnode;
			for (size_t i = 0; i <= Rnode.keys_len; ++i)
			{
				//Rnode.childs[i]->father = pRnode.offset();
				idx.get(pnode, Rnode.childs[i]);
				pnode.data()->father = pRnode.offset();
			}
		}

		*pRnode.data() = Rnode; // 写入Rnode数据
		return std::pair<size_t, Pointer<Node>>(midKey, pRnode);
	}

	size_t L2push(KEY _Key, FPTR _Val, MemMan<Node>& _Idx, MemMan<DeltaItem>& _Dat, MemMan<Data2V2>& _L2dat, MemMan<Append>& _L2app)
	{
		size_t len = this->push(_Key, _Val, _Dat, _L2app, false);
		if (this->LeafNode == 1)
		{
			// childs[0] 是Data2
			Data2V2 data2;
			DeltaItem delta;
			Pointer<Data2V2> pdata2;
			Pointer<DeltaItem> pdelta;

			_L2dat.get(pdata2, this->childs[0]);
			data2 = *pdata2.data();

			_Dat.get(pdelta, _Val);
			delta = *pdelta.data();

			for (uint8_t i = 0; i < DATA_FIELD_NUM; ++i)
			{
				data2.data[i] += delta.delta[i];
			}
			*pdata2.data() = data2;

			FPTR nextData2V2 = data2.next_data;
			while (nextData2V2 != FNUL)
			{
				_L2dat.get(pdata2, nextData2V2);
				data2 = *pdata2.data();
				for (uint8_t i = 0; i < DATA_FIELD_NUM; ++i)
				{
					data2.data[i] += delta.delta[i];
				}
				*pdata2.data() = data2;
				nextData2V2 = data2.next_data;
			}
		}

		return len;
	}

	std::pair<KEY, Pointer<Node>> L2split(FPTR selfOffset, MemMan<Node>& _Idx, MemMan<DeltaItem>& _Dat, MemMan<Data2V2>& _L2dat, MemMan<Append>& _L2app)
	{
		auto re = this->split(selfOffset, _Idx, _L2app);
		if (this->LeafNode == 1)
		{
			Pointer<Data2V2> pdata2;
			Data2V2 data2;

			Pointer<Data2V2> Rpdata2;
			Data2V2 Rdata2;

			uint64_t copy_len = MaxM * 0.5;

			_L2dat.get(pdata2, this->childs[0]);
			data2 = *pdata2.data();

			Pointer<DeltaItem> RpDelta;
			DeltaItem Rdelta;
			Node Rnode;
			Rnode = *re.second.data();

			std::copy(this->Appends.begin() + copy_len + 1, this->Appends.end(), Rnode.Appends.begin() + 1);
			*re.second.data() = Rnode;

			Pointer<Append> pApp;
			Append app;
			memcpy(&Rdata2, &data2, sizeof(Data2V2));
			Rdata2.next_data = data2.next_data;
			Rdata2.pre_data = pdata2.offset();
			for (size_t i = 0; i <= copy_len; ++i)
			{
				_Dat.get(RpDelta, Rnode.childs[i + 1]);
				Rdelta = *RpDelta.data();
				if (Rnode.Appends[i + 1] != FNUL)
				{
					_L2app.get(pApp, Rnode.Appends[i + 1]);
					app = *pApp.data();
				}
				for (uint8_t j = 0; j < DATA_FIELD_NUM; ++j)
				{
					data2.data[j] -= Rdelta.delta[j];
					//Rdata2.data[j] += Rdelta.delta[j];
					if (Rnode.Appends[i + 1] != FNUL)
					{
						data2.data[j] -= app.data[j];
					}
				}
			}

			_L2dat.alloc(Rpdata2, Rdata2);
			if (data2.next_data != FNUL)
			{
				Pointer<Data2V2> tmp;
				_L2dat.get(tmp, data2.next_data);
				tmp.data()->pre_data = Rpdata2.offset();
			}
			data2.next_data = Rpdata2.offset();

			*pdata2.data() = data2;
			re.second.data()->childs[0] = Rpdata2.offset();
		}
		return re;
	}

	size_t LeafNode = 1;
	size_t keys_len = 0;
	std::array<KEY, MaxM> keys;
	std::array<FPTR, MaxM + 1> childs;
	std::array<FPTR, MaxM + 1> Appends;
	FPTR father = FNUL;
	FPTR nextNode = FNUL;
};

class BPlusTree
{
public:
	BPlusTree() = default;
	void init(const std::string& dir)
	{
		std::string datName = "testdb.dat";
		std::string idxName = "testdb.idx";
		std::string L2datName = "testdb.L2dat";
		std::string L2appName = "testdb.L2app";

		bool restoreFlag = false;

		if (DiskIO<DeltaItem>::existCheck(datName.c_str()) == true && DiskIO<DeltaItem>::restoreCheck(datName.c_str()) == true)
		{
			restoreFlag = true;
		}
		this->dat.init((2 << 18), datName.c_str(), IOTYPE::O_READ);
		this->idx.init((2 << 15), idxName.c_str(), IOTYPE::O_REWR, restoreFlag);
		this->L2dat.init((2 << 15), L2datName.c_str(), IOTYPE::O_REWR, restoreFlag);
		this->L2app.init((2 << 15), L2appName.c_str(), IOTYPE::O_REWR, restoreFlag);

		Node RootNode;
		RootNode.LeafNode = 1;
		this->idx.alloc(this->Root, RootNode);

		if (restoreFlag == true)
		{
			this->restore();
		}
	}

	void restore()
	{
		uint64_t _Offset = FileHeaderOffset;
		char* packet = nullptr;
		while (this->dat.dread(&packet, _Offset) != FNUL)
		{
			_Offset += sizeof(DeltaPacket);
			DeltaPacket* ptr = (DeltaPacket*)packet;
			for (size_t i = 0; i < ptr->delta_count; ++i)
			{
				FPTR L2RootOffset = this->find(ptr->deltas[i].key);
				Pointer<Node> pL2Root;

				if (L2RootOffset == FNUL)
				{
					pL2Root = this->L2init(ptr->version, _Offset, ptr->deltas[i]);
					this->push(ptr->deltas[i].key, pL2Root.offset());
				}
				else {
					uint64_t newL2RootOffset = this->push(L2RootOffset, ptr->version, _Offset);
					if (newL2RootOffset != L2RootOffset) {
						this->push(ptr->deltas[i].key, newL2RootOffset);
					}
				}
				_Offset += sizeof(DeltaItem);
			}

			delete packet;
			packet = nullptr;
		}
	}

	void push(KEY _Key, FPTR _Val)
	{
		Pointer<Node> node = _find(_Key);
		_add(node, _Key, _Val);
	}

	FPTR push(FPTR L2RootOffset, KEY _Key, FPTR _Val)
	{
		Pointer<Node> L2Root;
		this->idx.get(L2Root, L2RootOffset);
		this->push(L2Root, _Key, _Val);
		return L2Root.offset();
	}

	void push(Pointer<Node>& L2Root, KEY _Key, FPTR _Val)
	{
		Pointer<Node> node = _find(L2Root, _Key);
		_add(L2Root, node, _Key, _Val);
	}

	Pointer<Node> L2init(uint64_t _Version, uint64_t _Offset, const DeltaItem& delta)
	{
		Node L2Root;
		L2Root.LeafNode = 1;
		L2Root.keys[0] = _Version;
		L2Root.childs[0] = 0; // data	,L2数据节点的childs[0]保留当前及前序节点deltas累加值
		L2Root.childs[1] = _Offset;
		L2Root.keys_len = 1;

		Data2V2 data2;
		for (uint8_t i = 0; i < 64; ++i)
		{
			data2.data[i] = delta.delta[i];
		}
		Pointer<Data2V2> pdata2;
		this->L2dat.alloc(pdata2, data2);
		L2Root.childs[0] = pdata2.offset();
		Pointer<Node> pL2Root;
		this->idx.alloc(pL2Root, L2Root);
		return pL2Root;
	}

	FPTR find(KEY _Key)
	{
		Pointer<Node> pnode = this->_find(_Key);
		Node node = *pnode.data();
		auto pos = std::lower_bound(node.keys.begin(), node.keys.begin() + node.keys_len, _Key) - node.keys.begin();
		if (pos < node.keys_len && node.keys[pos] == _Key)
		{
			return node.childs[pos + 1];
		}
		return FNUL;
	}

	bool find(KEY _Key, uint64_t _Version, Data& data)
	{
		FPTR L2RootOffset = this->find(_Key);
		if (L2RootOffset == FNUL)
		{
			return false;
		}

		Pointer<Node> pL2Root;
		this->idx.get(pL2Root, L2RootOffset);

		Pointer<Node> pL2LeafNode = this->_find(pL2Root, _Version);
		Node L2LeafNode = *pL2LeafNode.data();

		if (L2LeafNode.keys_len == 0 || L2LeafNode.keys[0] > _Version)
		{
			return false;
		}

		Pointer<Data2V2> pdata2;
		this->L2dat.get(pdata2, L2LeafNode.childs[0]);

		Data2V2 data2;
		data2 = *pdata2.data();

		Pointer<DeltaItem> pDelta;
		DeltaItem delta;

		auto iter = std::upper_bound(L2LeafNode.keys.begin(), L2LeafNode.keys.begin() + L2LeafNode.keys_len, _Version) - L2LeafNode.keys.begin();

		data.version = L2LeafNode.keys[iter - 1];
		memcpy(data.field, data2.data, 512);
		if (iter < L2LeafNode.keys_len)
		{
			Pointer<Append> pApp;
			Append app;

			for (; iter < L2LeafNode.keys_len; ++iter)
			{
				this->dat.get(pDelta, L2LeafNode.childs[iter + 1]);
				delta = *pDelta.data();
				if (L2LeafNode.Appends[iter + 1] != FNUL)
				{
					this->L2app.get(pApp, L2LeafNode.Appends[iter + 1]);
					app = *pApp.data();
				}
				for (uint8_t j = 0; j < DATA_FIELD_NUM; ++j)
				{
					data.field[j] -= delta.delta[j];
					if (L2LeafNode.Appends[iter + 1] != FNUL)
					{
						data.field[j] -= app.data[j];
					}
				}
			}
		}

		this->idx.get(pL2LeafNode, L2LeafNode.nextNode);
		return true;
	}

	uint64_t datDwrite(char* _Contents, size_t _Len)
	{
		return this->dat.alloc(_Contents, _Len);
	}

protected:
	void _add(Pointer<Node>& pnode, KEY _Key, FPTR _Val)
	{
		Node node = *pnode.data();
		size_t len = node.push(_Key, _Val, this->dat, this->L2app);
		*pnode.data() = node;

		if (len == MaxM)
		{
			auto re = node.split(pnode.offset(), this->idx, this->L2app);
			*pnode.data() = node;

			if (pnode.offset() == this->Root.offset())
			{
				Node Rnode;
				Rnode.keys[0] = re.first;
				Rnode.childs[0] = pnode.offset();
				Rnode.childs[1] = re.second.offset();
				Rnode.keys_len = 1;
				Rnode.LeafNode = 0;
				this->idx.alloc(this->Root, Rnode);
				pnode.data()->father = this->Root.offset();
				re.second.data()->father = this->Root.offset();
			}
			else
			{
				Pointer<Node> fpnode;
				this->idx.get(fpnode, node.father);
				_add(fpnode, re.first, re.second.offset());
			}
		}
	}

	void _add(Pointer<Node>& L2Root, Pointer<Node>& pnode, KEY _Key, FPTR _Val)
	{
		Node node = *pnode.data();
		size_t len = node.L2push(_Key, _Val, this->idx, this->dat, this->L2dat, this->L2app);
		*pnode.data() = node;

		if (len == MaxM)
		{
			auto re = node.L2split(pnode.offset(), this->idx, this->dat, this->L2dat, this->L2app);
			*pnode.data() = node;

			if (pnode.offset() == L2Root.offset())
			{
				Node Rnode;
				Rnode.keys[0] = re.first;
				Rnode.childs[0] = pnode.offset();
				Rnode.childs[1] = re.second.offset();
				Rnode.keys_len = 1;
				Rnode.LeafNode = 0;
				this->idx.alloc(L2Root, Rnode);
				pnode.data()->father = L2Root.offset();
				re.second.data()->father = L2Root.offset();
			}
			else
			{
				Pointer<Node> fpnode;
				this->idx.get(fpnode, node.father);
				_add(L2Root, fpnode, re.first, re.second.offset());
			}
		}
	}

	Pointer<Node> _find(KEY _Key)
	{
		Pointer<Node> pnode;
		Node node;
		pnode = this->Root;
		node = *this->Root.data();

		while (node.LeafNode == 0)
		{
			auto pos = std::lower_bound(node.keys.begin(), node.keys.begin() + node.keys_len, _Key) - node.keys.begin();
			if (pos < node.keys_len && node.keys[pos] == _Key)
			{
				++pos;
			}
			this->idx.get(pnode, node.childs[pos]);
			node = *pnode.data();
		}
		return pnode;
	}

	Pointer<Node> _find(Pointer<Node>& _Root, KEY _Key)
	{
		Pointer<Node> pnode;
		Node node;
		pnode = _Root;
		node = *_Root.data();

		while (node.LeafNode == 0)
		{
			auto pos = std::lower_bound(node.keys.begin(), node.keys.begin() + node.keys_len, _Key) - node.keys.begin();
			if (pos < node.keys_len && node.keys[pos] == _Key)
			{
				++pos;
			}
			this->idx.get(pnode, node.childs[pos]);
			node = *pnode.data();
		}
		return pnode;
	}

private:
	Pointer<Node> Root;
	MemMan<Node> idx;
	MemMan<DeltaItem> dat;
	MemMan<Data2V2> L2dat;
	MemMan<Append> L2app;
};