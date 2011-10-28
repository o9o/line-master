/*
 *	Copyright (C) 2011 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef QPAIRINGHEAP_H
#define QPAIRINGHEAP_H

#include <QtCore>

template<typename T>
class QPairingHeap;

template<typename T>
class QPairingHeapPrivate
{
private:
	quint64 priority;
	T data;
	bool empty;
	QList<QPairingHeapPrivate<T> *> subheaps;

	QPairingHeapPrivate() :
		priority(0), empty(true) {
		subheaps = QList<QPairingHeapPrivate<T> *>();
	}

	QPairingHeapPrivate(T data, quint64 priority) :
		priority(priority), data(data), empty(false) {
		subheaps = QList<QPairingHeapPrivate<T> *>();
	}

	~QPairingHeapPrivate() {}

	static QPairingHeapPrivate<T> *createHeap() {
		return new QPairingHeapPrivate<T>();
	}

	T getData() {
		return data;
	}

	quint64 getPriority() {
		return priority;
	}

	bool isEmpty() {
		return empty;
	}

	QPairingHeapPrivate *findMin() {
		return this;
	}

	QPairingHeapPrivate *merge(QPairingHeapPrivate *other) {
		if (this->isEmpty()) {
			delete this;
			return other;
		} else if (other->isEmpty()) {
			delete other;
			return this;
		} else if (this->priority < other->priority) {
			subheaps << other;
			return this;
		} else {
			other->subheaps << this;
			return other;
		}
	}

	QPairingHeapPrivate *insert(T data, quint64 priority) {
		QPairingHeapPrivate *basic = new QPairingHeapPrivate(data, priority);
		return merge(basic);
	}

	QPairingHeapPrivate *deleteMin() {
		if (isEmpty()) {
			return this;
		} else if (subheaps.isEmpty()) {
			this->empty = true;
			return this;
		} else if (subheaps.count() == 1) {
			QPairingHeapPrivate *result = subheaps.takeFirst();
			delete this;
			return result;
		} else {
			QPairingHeapPrivate *result = mergePairs(subheaps);
			delete this;
			return result;
		}
	}

	QPairingHeapPrivate *mergePairs(QList<QPairingHeapPrivate*> list) {
		Q_ASSERT(!list.isEmpty());
		if (list.count() == 1)
			return list.first();

		// merge pairs from left to right
		QList<QPairingHeapPrivate*> pairsLR;
		for (int i = 0; i < list.count() - 1; i += 2) {
			pairsLR << list[i]->merge(list[i+1]);
		}
		if (list.count() % 2)
			pairsLR << list.last();

		// merge cumulatively from right to left
		while (pairsLR.count() >= 2) {
			QPairingHeapPrivate* r2 = pairsLR.takeLast();
			QPairingHeapPrivate* r1 = pairsLR.takeLast();
			pairsLR << r1->merge(r2);
		}
		return pairsLR.first();
	}

	friend class QPairingHeap<T>;
};

template<typename T>
class QPairingHeap {
public:
	QPairingHeap() {
		heap = QPairingHeapPrivate<T>::createHeap();
	}

	~QPairingHeap() {
		delete heap;
	}

	bool isEmpty() {
		return heap->isEmpty();
	}

	QPair<T, quint64> findMin() {
		Q_ASSERT(!heap->isEmpty());

		return QPair<T, quint64>(heap->getData(), heap->getPriority());
	}

	void deleteMin() {
		heap = heap->deleteMin();
	}

	void insert(T data, quint64 priority) {
		heap = heap->insert(data, priority);
	}

	void merge(QPairingHeap<T> *other) {
		heap = heap->merge(other->heap);
		other->heap = heap;
	}

private:
	QPairingHeapPrivate<T> *heap;
};

void QPairingHeap_test();
void QPairingHeap_testPerf();


#endif // QPAIRINGHEAP_H
