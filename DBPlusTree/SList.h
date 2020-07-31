#pragma once

#include "config.h"

template <typename Type>
struct SListNode
{
	SListNode(Type *ele)
	{
		this->data = ele;
	}
	SListNode() = default;
	Type *data = nullptr;
	SListNode *next = nullptr;
	SListNode *pre = nullptr;
};

template <typename Type>
class SList
{
public:
	SList()
	{
		this->Flag = new SListNode<Type>(nullptr);
		this->Flag->pre = this->Flag;
		this->Flag->next = this->Flag;
	}

	void push_back(Type *ele)
	{
		SListNode<Type> *node = new SListNode<Type>(ele);
		_push_back(node);
	}

	void trnasToEnd(SListNode<Type> *node)
	{
		node->pre->next = node->next;
		node->next->pre = node->pre;
		_push_back(node);
	}

	SListNode<Type> *front()
	{
		if (this->Flag != nullptr && this->Flag->next != this->Flag)
		{
			return this->Flag->next;
		}
		return nullptr;
	}

	SListNode<Type> *end()
	{
		if (this->Flag != nullptr && this->Flag->next != this->Flag)
		{
			return this->Flag->pre;
		}
		return nullptr;
	}

protected:
	void _push_back(SListNode<Type> *node)
	{
		node->pre = node->next = nullptr;
		node->next = this->Flag;
		node->pre = this->Flag->pre;
		node->pre->next = node;
		this->Flag->pre = node;
	}

private:
	SListNode<Type> *Flag = nullptr;
};